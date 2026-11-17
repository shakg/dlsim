// Owns the link configuration being edited and the configuration the helper
// last applied. Sends minimal-diff applies, handles CUT/RESTORE and computes
// the command preview with the same TcCommandBuilder the helper uses.
#pragma once

#include "Model.h"
#include "TcCommandBuilder.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

#include <optional>

class EventLog;
class HelperClient;
class SystemInfo;

class LinkController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY configChanged)
    Q_PROPERTY(QString groundIface READ groundIface WRITE setGroundIface NOTIFY configChanged)
    Q_PROPERTY(QString airIface READ airIface WRITE setAirIface NOTIFY configChanged)
    Q_PROPERTY(QString hostIface READ hostIface WRITE setHostIface NOTIFY configChanged)
    Q_PROPERTY(QVariantMap uplink READ uplink NOTIFY configChanged)
    Q_PROPERTY(QVariantMap downlink READ downlink NOTIFY configChanged)
    Q_PROPERTY(QVariantList classes READ classes NOTIFY configChanged)
    // [{name, ip, uses}]: named devices traffic classes can refer to.
    Q_PROPERTY(QVariantList endpoints READ endpoints NOTIFY configChanged)
    Q_PROPERTY(bool otherExcluded READ otherExcluded WRITE setOtherExcluded NOTIFY configChanged)
    Q_PROPERTY(QString activePreset READ activePreset NOTIFY configChanged)
    // Plain-language result of both profiles, e.g. "≈ 80 ms round trip · ...".
    Q_PROPERTY(QString summary READ summary NOTIFY configChanged)
    Q_PROPERTY(bool autoApply READ autoApply WRITE setAutoApply NOTIFY autoApplyChanged)

    Q_PROPERTY(bool active READ isActive NOTIFY appliedChanged)
    Q_PROPERTY(bool bridgeUp READ bridgeUp NOTIFY appliedChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(bool cut READ isCut NOTIFY cutChanged)
    Q_PROPERTY(QStringList cutSources READ cutSources NOTIFY cutChanged)
    Q_PROPERTY(double outageRemaining READ outageRemaining NOTIFY outageChanged)

    Q_PROPERTY(bool dirty READ isDirty NOTIFY previewChanged)
    Q_PROPERTY(QStringList validationErrors READ validationErrors NOTIFY previewChanged)
    // Field path ("airIface", "classes.2.ip", "uplink.delayMs", ...) -> short message; first issue per field.
    Q_PROPERTY(QVariantMap fieldErrors READ fieldErrors NOTIFY previewChanged)
    Q_PROPERTY(QString previewLevel READ previewLevel NOTIFY previewChanged)
    Q_PROPERTY(QString previewDiff READ previewDiff NOTIFY previewChanged)
    Q_PROPERTY(QString previewFull READ previewFull NOTIFY previewChanged)
    Q_PROPERTY(QString lastExecuted READ lastExecuted NOTIFY lastExecutedChanged)

public:
    LinkController(HelperClient *helper, EventLog *log, SystemInfo *sys, QObject *parent = nullptr);

    QString mode() const { return dlsim::toString(m_edit.mode); }
    QString groundIface() const { return m_edit.groundIface; }
    QString airIface() const { return m_edit.airIface; }
    QString hostIface() const { return m_edit.hostIface; }
    QVariantMap uplink() const { return m_edit.uplink.toJson().toVariantMap(); }
    QVariantMap downlink() const { return m_edit.downlink.toJson().toVariantMap(); }
    QVariantList classes() const;
    QVariantList endpoints() const;
    bool otherExcluded() const { return m_edit.otherMode == dlsim::ClassMode::Excluded; }
    QString activePreset() const { return m_activePreset; }
    QString summary() const;
    bool autoApply() const { return m_autoApply; }

    void setMode(const QString &m);
    void setGroundIface(const QString &v);
    void setAirIface(const QString &v);
    void setHostIface(const QString &v);
    void setOtherExcluded(bool v);
    void setAutoApply(bool v);

    bool isActive() const { return m_appliedPlan.has_value(); }
    bool bridgeUp() const { return m_bridgeUp; }
    bool isBusy() const { return m_inFlight || m_structuralBusy; }
    bool isCut() const { return !m_cutSources.isEmpty(); }
    QStringList cutSources() const { return QStringList(m_cutSources.begin(), m_cutSources.end()); }
    double outageRemaining() const;

    bool isDirty() const;
    QStringList validationErrors() const { return m_validation; }
    QVariantMap fieldErrors() const { return m_fieldErrors; }
    QString previewLevel() const { return m_previewLevel; }
    QString previewDiff() const { return m_previewDiff; }
    QString previewFull() const { return m_previewFull; }
    QString lastExecuted() const { return m_lastExecuted; }

    const dlsim::LinkConfig &editConfig() const { return m_edit; }
    dlsim::Profile profile(dlsim::Direction d) const { return d == dlsim::Direction::Uplink ? m_edit.uplink : m_edit.downlink; }

    // Profile editing. dir is "uplink" or "downlink"; key is a Profile JSON key.
    Q_INVOKABLE void setProfileField(const QString &dir, const QString &key, const QVariant &value);
    // Simple mode (see SimpleMode.h). dir: "uplink", "downlink" or "both" (set only).
    Q_INVOKABLE QVariantMap simpleOf(const QString &dir) const;
    Q_INVOKABLE void setSimple(const QString &dir, const QVariantMap &settings);
    Q_INVOKABLE QStringList advancedFields(const QString &dir) const;
    Q_INVOKABLE void setProfiles(const QVariantMap &uplink, const QVariantMap &downlink, const QString &presetName);
    Q_INVOKABLE void applyPreset(const QVariantMap &preset);
    // Follow a preset rename: the active name changes, the modified mark stays.
    Q_INVOKABLE void renameActivePreset(const QString &from, const QString &to);

    // netem queue limit the builder would pick for this profile when limitPkts is 0.
    Q_INVOKABLE int autoLimit(const QVariantMap &profile) const;

    // Traffic classes
    Q_INVOKABLE void addClass(const QString &name, const QString &proto, const QString &ip, int port, const QString &mode);
    Q_INVOKABLE void removeClass(int index);
    // From a traffic template: proto, port and mode from it, IP from the endpoint (empty = any IP).
    Q_INVOKABLE void addClassFor(const QString &name, const QString &proto, const QString &endpoint, int port,
                                 const QString &mode);
    Q_INVOKABLE void addEndpoint(const QString &name, const QString &ip);
    Q_INVOKABLE bool renameEndpoint(const QString &from, const QString &to);
    Q_INVOKABLE void setEndpointIp(const QString &name, const QString &ip);
    Q_INVOKABLE void removeEndpoint(const QString &name);
    Q_INVOKABLE void moveClass(int index, int delta);
    Q_INVOKABLE void setClassField(int index, const QString &key, const QVariant &value);
    Q_INVOKABLE void setClassProfileField(int index, const QString &dir, const QString &key, const QVariant &value);

    // Lifecycle
    Q_INVOKABLE void start();    // bridge: setup_bridge + apply; host: apply
    Q_INVOKABLE void apply();
    // Throw away edits made since the last apply (active link only).
    Q_INVOKABLE void discardChanges();
    Q_INVOKABLE void stop();     // remove impairment (and the bridge)
    Q_INVOKABLE void reset();    // remove everything DLSim created

    // Link state
    Q_INVOKABLE void setCut(bool cut);            // manual CUT / RESTORE (restore clears every source)
    Q_INVOKABLE void timedOutage(double seconds);
    void setCutSource(const QString &source, bool on);

    // Used by the scenario engine: replace both profiles and push them out
    // (coalesced, so it can be called at 10 Hz).
    void applyProfilesLive(const dlsim::Profile &up, const dlsim::Profile &down, const QString &label);

Q_SIGNALS:
    void configChanged();
    void autoApplyChanged();
    void appliedChanged();
    void busyChanged();
    void cutChanged();
    void outageChanged();
    void previewChanged();
    void lastExecutedChanged();
    // context: see dlsim::explainError
    void errorOccurred(const QString &context, const QString &message);

private:
    void edited(bool presetModified = true);
    void sendApply(const QString &reason, bool force = false);
    void sendCutState();
    void updatePreview();
    void onHelperLost();
    void onCommandsExecuted(const QString &op, const QJsonArray &cmds);
    void saveSettings();
    void loadSettings();
    void markInactive();
    dlsim::LinkConfig desiredConfig() const;

    HelperClient *m_helper;
    EventLog *m_log;
    SystemInfo *m_sys;

    dlsim::LinkConfig m_edit;
    std::optional<dlsim::LinkConfig> m_applied;
    std::optional<dlsim::Plan> m_appliedPlan;
    bool m_bridgeUp = false;
    QString m_bridgeGround, m_bridgeAir;
    QString m_activePreset;
    QString m_appliedPreset; // m_activePreset at the last successful apply
    bool m_autoApply = true;

    bool m_inFlight = false;
    bool m_pendingApply = false;
    QString m_pendingReason;
    bool m_structuralBusy = false;
    QSet<QString> m_cutSources;
    bool m_cutApplied = false;

    QTimer m_autoApplyTimer;
    QTimer m_saveTimer;
    QTimer m_outageTimer;
    QTimer m_outageTick;
    QElapsedTimer m_outageClock;
    double m_outageSeconds = 0;

    QStringList m_validation;
    QVariantMap m_fieldErrors;
    QString m_previewLevel, m_previewDiff, m_previewFull, m_lastExecuted;
};
