// Built-in and user presets/scenarios. User files live in ~/.config/dlsim/
// (presets/<name>.json, scenarios/<name>.json).
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

class PresetStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY presetsChanged)
    Q_PROPERTY(QStringList scenarioNames READ scenarioNames NOTIFY scenariosChanged)
    Q_PROPERTY(QString configDir READ configDir CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
public:
    explicit PresetStore(QObject *parent = nullptr);

    QStringList presetNames() const;
    QStringList scenarioNames() const;
    QString configDir() const { return m_dir; }
    QString lastError() const { return m_lastError; }

    // {name, description, builtin, uplink, downlink}; empty map if unknown
    Q_INVOKABLE QVariantMap preset(const QString &name) const;
    Q_INVOKABLE bool isBuiltinPreset(const QString &name) const;
    Q_INVOKABLE bool savePreset(const QString &name, const QString &description,
                                const QVariantMap &uplink, const QVariantMap &downlink);
    Q_INVOKABLE bool deletePreset(const QString &name);
    Q_INVOKABLE QString importPreset(const QUrl &file);
    Q_INVOKABLE bool exportPreset(const QString &name, const QUrl &file);

    // {name, builtin, loop, steps: [{type, preset, durationS}]}
    Q_INVOKABLE QVariantMap scenario(const QString &name) const;
    Q_INVOKABLE bool isBuiltinScenario(const QString &name) const;
    Q_INVOKABLE bool saveScenario(const QVariantMap &scenario);
    Q_INVOKABLE bool deleteScenario(const QString &name);
    Q_INVOKABLE QString importScenario(const QUrl &file);
    Q_INVOKABLE bool exportScenario(const QString &name, const QUrl &file);

Q_SIGNALS:
    void presetsChanged();
    void scenariosChanged();
    void lastErrorChanged();

private:
    void load();
    bool fail(const QString &msg);
    QString fileFor(const QString &kind, const QString &name) const;
    bool writeJson(const QString &path, const QJsonObject &o);
    static QJsonObject normalizePreset(const QJsonObject &o);
    static QJsonObject normalizeScenario(const QJsonObject &o);

    QString m_dir;
    QList<QJsonObject> m_builtinPresets, m_userPresets;
    QList<QJsonObject> m_builtinScenarios, m_userScenarios;
    QString m_lastError;
};
