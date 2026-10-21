#include "PresetStore.h"

#include "Model.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

QJsonDocument readJsonFile(const QString &path, QString *error = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return {};
    }
    QJsonParseError pe;
    QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError && error)
        *error = pe.errorString();
    return d;
}

QString localPath(const QUrl &u) { return u.isLocalFile() ? u.toLocalFile() : u.toString(); }

const QJsonObject *findByName(const QList<QJsonObject> &list, const QString &name)
{
    for (const auto &o : list)
        if (o.value("name").toString() == name)
            return &o;
    return nullptr;
}

} // namespace

PresetStore::PresetStore(QObject *parent) : QObject(parent)
{
    m_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/dlsim";
    QDir().mkpath(m_dir + "/presets");
    QDir().mkpath(m_dir + "/scenarios");
    load();
}

void PresetStore::load()
{
    m_builtinPresets.clear();
    m_userPresets.clear();
    m_builtinScenarios.clear();
    m_userScenarios.clear();
    for (const auto &v : readJsonFile(":/presets/presets.json").array())
        m_builtinPresets << normalizePreset(v.toObject());
    for (const auto &v : readJsonFile(":/presets/scenarios.json").array())
        m_builtinScenarios << normalizeScenario(v.toObject());

    auto loadDir = [this](const QString &sub, QList<QJsonObject> &out, auto normalize) {
        QDir d(m_dir + '/' + sub);
        for (const QString &f : d.entryList({"*.json"}, QDir::Files, QDir::Name)) {
            const QJsonObject o = readJsonFile(d.filePath(f)).object();
            if (!o.value("name").toString().isEmpty())
                out << normalize(o);
        }
    };
    loadDir("presets", m_userPresets, &PresetStore::normalizePreset);
    loadDir("scenarios", m_userScenarios, &PresetStore::normalizeScenario);
    Q_EMIT presetsChanged();
    Q_EMIT scenariosChanged();
}

bool PresetStore::fail(const QString &msg)
{
    m_lastError = msg;
    Q_EMIT lastErrorChanged();
    return false;
}

QJsonObject PresetStore::normalizePreset(const QJsonObject &o)
{
    return QJsonObject{
        {"name", o.value("name").toString().trimmed()},
        {"description", o.value("description").toString()},
        {"uplink", dlsim::Profile::fromJson(o.value("uplink").toObject()).toJson()},
        {"downlink", dlsim::Profile::fromJson(o.value("downlink").toObject()).toJson()},
    };
}

QJsonObject PresetStore::normalizeScenario(const QJsonObject &o)
{
    QJsonArray steps;
    for (const auto &v : o.value("steps").toArray()) {
        const QJsonObject s = v.toObject();
        QString type = s.value("type").toString();
        if (type != "preset" && type != "outage" && type != "fade")
            continue;
        steps.append(QJsonObject{
            {"type", type},
            {"preset", s.value("preset").toString()},
            {"durationS", std::clamp(s.value("durationS").toDouble(10), 0.1, 86400.0)},
        });
    }
    return QJsonObject{
        {"name", o.value("name").toString().trimmed()},
        {"loop", o.value("loop").toBool(false)},
        {"steps", steps},
    };
}

QString PresetStore::fileFor(const QString &kind, const QString &name) const
{
    QString safe = name;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_. -]")), "_");
    return QStringLiteral("%1/%2/%3.json").arg(m_dir, kind, safe);
}

bool PresetStore::writeJson(const QString &path, const QJsonObject &o)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("cannot write %1: %2").arg(path, f.errorString()));
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}

// ---------------------------------------------------------------- presets

QStringList PresetStore::presetNames() const
{
    QStringList names;
    for (const auto &o : m_builtinPresets)
        names << o.value("name").toString();
    for (const auto &o : m_userPresets)
        if (!names.contains(o.value("name").toString()))
            names << o.value("name").toString();
    return names;
}

QVariantMap PresetStore::preset(const QString &name) const
{
    const QJsonObject *o = findByName(m_builtinPresets, name);
    const bool builtin = o != nullptr;
    if (!o)
        o = findByName(m_userPresets, name);
    if (!o)
        return {};
    QVariantMap m = o->toVariantMap();
    m["builtin"] = builtin;
    return m;
}

bool PresetStore::isBuiltinPreset(const QString &name) const { return findByName(m_builtinPresets, name); }

bool PresetStore::savePreset(const QString &name, const QString &description, const QVariantMap &uplink,
                             const QVariantMap &downlink)
{
    const QString n = name.trimmed();
    if (n.isEmpty())
        return fail("preset name is empty");
    if (isBuiltinPreset(n))
        return fail(QStringLiteral("'%1' is a built-in preset; choose another name").arg(n));
    const QJsonObject o = normalizePreset(QJsonObject{
        {"name", n}, {"description", description},
        {"uplink", QJsonObject::fromVariantMap(uplink)}, {"downlink", QJsonObject::fromVariantMap(downlink)}});
    if (!writeJson(fileFor("presets", n), o))
        return false;
    load();
    return true;
}

bool PresetStore::deletePreset(const QString &name)
{
    if (isBuiltinPreset(name))
        return fail("built-in presets cannot be deleted");
    if (!QFile::remove(fileFor("presets", name)))
        return fail(QStringLiteral("cannot delete preset '%1'").arg(name));
    load();
    return true;
}

QString PresetStore::importPreset(const QUrl &file)
{
    QString err;
    const QJsonDocument d = readJsonFile(localPath(file), &err);
    if (!d.isObject() || d.object().value("name").toString().trimmed().isEmpty()) {
        fail(QStringLiteral("not a preset file: %1").arg(err.isEmpty() ? "missing name" : err));
        return {};
    }
    QJsonObject o = normalizePreset(d.object());
    if (isBuiltinPreset(o.value("name").toString()))
        o["name"] = o.value("name").toString() + " (imported)";
    const QString name = o.value("name").toString();
    if (!writeJson(fileFor("presets", name), o))
        return {};
    load();
    return name;
}

bool PresetStore::exportPreset(const QString &name, const QUrl &file)
{
    QVariantMap m = preset(name);
    if (m.isEmpty())
        return fail(QStringLiteral("unknown preset '%1'").arg(name));
    m.remove("builtin");
    return writeJson(localPath(file), QJsonObject::fromVariantMap(m));
}

// ---------------------------------------------------------------- scenarios

QStringList PresetStore::scenarioNames() const
{
    QStringList names;
    for (const auto &o : m_builtinScenarios)
        names << o.value("name").toString();
    for (const auto &o : m_userScenarios)
        if (!names.contains(o.value("name").toString()))
            names << o.value("name").toString();
    return names;
}

QVariantMap PresetStore::scenario(const QString &name) const
{
    const QJsonObject *o = findByName(m_builtinScenarios, name);
    const bool builtin = o != nullptr;
    if (!o)
        o = findByName(m_userScenarios, name);
    if (!o)
        return {};
    QVariantMap m = o->toVariantMap();
    m["builtin"] = builtin;
    return m;
}

bool PresetStore::isBuiltinScenario(const QString &name) const { return findByName(m_builtinScenarios, name); }

bool PresetStore::saveScenario(const QVariantMap &scenario)
{
    const QJsonObject o = normalizeScenario(QJsonObject::fromVariantMap(scenario));
    const QString n = o.value("name").toString();
    if (n.isEmpty())
        return fail("scenario name is empty");
    if (isBuiltinScenario(n))
        return fail(QStringLiteral("'%1' is a built-in scenario; choose another name").arg(n));
    if (!writeJson(fileFor("scenarios", n), o))
        return false;
    load();
    return true;
}

bool PresetStore::deleteScenario(const QString &name)
{
    if (isBuiltinScenario(name))
        return fail("built-in scenarios cannot be deleted");
    if (!QFile::remove(fileFor("scenarios", name)))
        return fail(QStringLiteral("cannot delete scenario '%1'").arg(name));
    load();
    return true;
}

QString PresetStore::importScenario(const QUrl &file)
{
    QString err;
    const QJsonDocument d = readJsonFile(localPath(file), &err);
    if (!d.isObject() || d.object().value("name").toString().trimmed().isEmpty()) {
        fail(QStringLiteral("not a scenario file: %1").arg(err.isEmpty() ? "missing name" : err));
        return {};
    }
    QJsonObject o = normalizeScenario(d.object());
    if (isBuiltinScenario(o.value("name").toString()))
        o["name"] = o.value("name").toString() + " (imported)";
    const QString name = o.value("name").toString();
    if (!writeJson(fileFor("scenarios", name), o))
        return {};
    load();
    return name;
}

bool PresetStore::exportScenario(const QString &name, const QUrl &file)
{
    QVariantMap m = scenario(name);
    if (m.isEmpty())
        return fail(QStringLiteral("unknown scenario '%1'").arg(name));
    m.remove("builtin");
    return writeJson(localPath(file), QJsonObject::fromVariantMap(m));
}
