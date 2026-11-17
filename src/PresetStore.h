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
    // Last failure of a preset or scenario operation; cleared when the next one starts.
    Q_PROPERTY(QString presetError READ presetError NOTIFY presetErrorChanged)
    Q_PROPERTY(QString scenarioError READ scenarioError NOTIFY scenarioErrorChanged)
public:
    explicit PresetStore(QObject *parent = nullptr);

    QStringList presetNames() const;
    QStringList scenarioNames() const;
    QString configDir() const { return m_dir; }
    QString presetError() const { return m_presetError; }
    QString scenarioError() const { return m_scenarioError; }
    Q_INVOKABLE void clearPresetError() { setError(Kind::Preset, {}); }
    Q_INVOKABLE void clearScenarioError() { setError(Kind::Scenario, {}); }

    // {name, description, builtin, uplink, downlink}; empty map if unknown
    Q_INVOKABLE QVariantMap preset(const QString &name) const;
    Q_INVOKABLE bool isBuiltinPreset(const QString &name) const;
    // True if saving under this name would replace a user preset file.
    Q_INVOKABLE bool userPresetExists(const QString &name) const;
    Q_INVOKABLE bool savePreset(const QString &name, const QString &description,
                                const QVariantMap &uplink, const QVariantMap &downlink);
    Q_INVOKABLE bool deletePreset(const QString &name);
    // Renames a user preset. Fails for built-ins and when the new name is taken.
    Q_INVOKABLE bool renamePreset(const QString &from, const QString &to);
    Q_INVOKABLE QString importPreset(const QUrl &file);
    Q_INVOKABLE bool exportPreset(const QString &name, const QUrl &file);

    // {name, builtin, loop, steps: [{type, preset, durationS}]}
    Q_INVOKABLE QVariantMap scenario(const QString &name) const;
    Q_INVOKABLE bool isBuiltinScenario(const QString &name) const;
    Q_INVOKABLE bool userScenarioExists(const QString &name) const;
    Q_INVOKABLE bool saveScenario(const QVariantMap &scenario);
    Q_INVOKABLE bool deleteScenario(const QString &name);
    Q_INVOKABLE QString importScenario(const QUrl &file);
    Q_INVOKABLE bool exportScenario(const QString &name, const QUrl &file);
    // Preset names used by the scenario's preset/fade steps that don't exist, in step order, unique.
    Q_INVOKABLE QStringList missingPresets(const QVariantMap &scenario) const;

Q_SIGNALS:
    void presetsChanged();
    void scenariosChanged();
    void presetErrorChanged();
    void scenarioErrorChanged();

private:
    enum class Kind { Preset, Scenario };
    void load();
    void setError(Kind kind, const QString &msg);
    bool fail(Kind kind, const QString &msg);
    QString fileFor(const QString &kind, const QString &name) const;
    bool writeJson(Kind kind, const QString &path, const QJsonObject &o);
    static QJsonObject normalizePreset(const QJsonObject &o);
    static QJsonObject normalizeScenario(const QJsonObject &o);

    QString m_dir;
    QList<QJsonObject> m_builtinPresets, m_userPresets;
    QList<QJsonObject> m_builtinScenarios, m_userScenarios;
    QString m_presetError, m_scenarioError;
};
