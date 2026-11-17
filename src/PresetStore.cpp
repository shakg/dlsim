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

void PresetStore::setError(Kind kind, const QString &msg)
{
    QString &e = kind == Kind::Preset ? m_presetError : m_scenarioError;
    if (e == msg)
        return;
    e = msg;
    if (kind == Kind::Preset)
        Q_EMIT presetErrorChanged();
    else
        Q_EMIT scenarioErrorChanged();
}

bool PresetStore::fail(Kind kind, const QString &msg)
{
    setError(kind, msg);
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

bool PresetStore::writeJson(Kind kind, const QString &path, const QJsonObject &o)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(kind, QStringLiteral("cannot write %1: %2").arg(path, f.errorString()));
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

bool PresetStore::userPresetExists(const QString &name) const
{
    const QString n = name.trimmed();
    return !n.isEmpty() && QFile::exists(fileFor("presets", n));
}

bool PresetStore::savePreset(const QString &name, const QString &description, const QVariantMap &uplink,
                             const QVariantMap &downlink)
{
    setError(Kind::Preset, {});
    const QString n = name.trimmed();
    if (n.isEmpty())
        return fail(Kind::Preset, "preset name is empty");
    if (isBuiltinPreset(n))
        return fail(Kind::Preset, QStringLiteral("'%1' is a built-in preset; choose another name").arg(n));
    const QJsonObject o = normalizePreset(QJsonObject{
        {"name", n}, {"description", description},
        {"uplink", QJsonObject::fromVariantMap(uplink)}, {"downlink", QJsonObject::fromVariantMap(downlink)}});
    if (!writeJson(Kind::Preset, fileFor("presets", n), o))
        return false;
    load();
    return true;
}

bool PresetStore::deletePreset(const QString &name)
{
    setError(Kind::Preset, {});
    if (isBuiltinPreset(name))
        return fail(Kind::Preset, "built-in presets cannot be deleted");
    if (!QFile::remove(fileFor("presets", name)))
        return fail(Kind::Preset, QStringLiteral("cannot delete preset '%1'").arg(name));
    load();
    return true;
}

bool PresetStore::renamePreset(const QString &from, const QString &to)
{
    setError(Kind::Preset, {});
    const QString n = to.trimmed();
    if (n.isEmpty())
        return fail(Kind::Preset, "preset name is empty");
    if (n == from)
        return true;
    if (isBuiltinPreset(from))
        return fail(Kind::Preset, "built-in presets cannot be renamed");
    if (isBuiltinPreset(n) || userPresetExists(n))
        return fail(Kind::Preset, QStringLiteral("a preset named '%1' already exists").arg(n));
    QVariantMap m = preset(from);
    if (m.isEmpty())
        return fail(Kind::Preset, QStringLiteral("unknown preset '%1'").arg(from));
    m.remove("builtin");
    m["name"] = n;
    if (!writeJson(Kind::Preset, fileFor("presets", n), normalizePreset(QJsonObject::fromVariantMap(m))))
        return false;
    QFile::remove(fileFor("presets", from));
    load();
    return true;
}

QString PresetStore::importPreset(const QUrl &file)
{
    setError(Kind::Preset, {});
    QString err;
    const QJsonDocument d = readJsonFile(localPath(file), &err);
    if (!d.isObject() || d.object().value("name").toString().trimmed().isEmpty()) {
        fail(Kind::Preset, QStringLiteral("not a preset file: %1").arg(err.isEmpty() ? "missing name" : err));
        return {};
    }
    QJsonObject o = normalizePreset(d.object());
    if (isBuiltinPreset(o.value("name").toString()))
        o["name"] = o.value("name").toString() + " (imported)";
    const QString name = o.value("name").toString();
    if (!writeJson(Kind::Preset, fileFor("presets", name), o))
        return {};
    load();
    return name;
}

bool PresetStore::exportPreset(const QString &name, const QUrl &file)
{
    setError(Kind::Preset, {});
    QVariantMap m = preset(name);
    if (m.isEmpty())
        return fail(Kind::Preset, QStringLiteral("unknown preset '%1'").arg(name));
    m.remove("builtin");
    return writeJson(Kind::Preset, localPath(file), QJsonObject::fromVariantMap(m));
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

bool PresetStore::userScenarioExists(const QString &name) const
{
    const QString n = name.trimmed();
    return !n.isEmpty() && QFile::exists(fileFor("scenarios", n));
}

bool PresetStore::saveScenario(const QVariantMap &scenario)
{
    setError(Kind::Scenario, {});
    const QJsonObject o = normalizeScenario(QJsonObject::fromVariantMap(scenario));
    const QString n = o.value("name").toString();
    if (n.isEmpty())
        return fail(Kind::Scenario, "scenario name is empty");
    if (isBuiltinScenario(n))
        return fail(Kind::Scenario, QStringLiteral("'%1' is a built-in scenario; choose another name").arg(n));
    if (!writeJson(Kind::Scenario, fileFor("scenarios", n), o))
        return false;
    load();
    return true;
}

bool PresetStore::deleteScenario(const QString &name)
{
    setError(Kind::Scenario, {});
    if (isBuiltinScenario(name))
        return fail(Kind::Scenario, "built-in scenarios cannot be deleted");
    if (!QFile::remove(fileFor("scenarios", name)))
        return fail(Kind::Scenario, QStringLiteral("cannot delete scenario '%1'").arg(name));
    load();
    return true;
}

QString PresetStore::importScenario(const QUrl &file)
{
    setError(Kind::Scenario, {});
    QString err;
    const QJsonDocument d = readJsonFile(localPath(file), &err);
    if (!d.isObject() || d.object().value("name").toString().trimmed().isEmpty()) {
        fail(Kind::Scenario, QStringLiteral("not a scenario file: %1").arg(err.isEmpty() ? "missing name" : err));
        return {};
    }
    QJsonObject o = normalizeScenario(d.object());
    if (isBuiltinScenario(o.value("name").toString()))
        o["name"] = o.value("name").toString() + " (imported)";
    const QString name = o.value("name").toString();
    if (!writeJson(Kind::Scenario, fileFor("scenarios", name), o))
        return {};
    load();
    return name;
}

bool PresetStore::exportScenario(const QString &name, const QUrl &file)
{
    setError(Kind::Scenario, {});
    QVariantMap m = scenario(name);
    if (m.isEmpty())
        return fail(Kind::Scenario, QStringLiteral("unknown scenario '%1'").arg(name));
    m.remove("builtin");
    return writeJson(Kind::Scenario, localPath(file), QJsonObject::fromVariantMap(m));
}

QStringList PresetStore::missingPresets(const QVariantMap &scenario) const
{
    QStringList missing;
    for (const QVariant &v : scenario.value("steps").toList()) {
        const QVariantMap s = v.toMap();
        if (s.value("type").toString() == "outage")
            continue;
        const QString p = s.value("preset").toString();
        if (!missing.contains(p) && preset(p).isEmpty())
            missing << p;
    }
    return missing;
}
