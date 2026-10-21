#include "LinkController.h"

#include "EventLog.h"
#include "HelperClient.h"
#include "SystemInfo.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

using namespace dlsim;
namespace B = dlsim::TcCommandBuilder;

namespace {

QString describe(const Profile &p)
{
    QStringList parts;
    if (p.delayMs > 0 || p.jitterMs > 0)
        parts << (p.jitterMs > 0 ? QStringLiteral("%1±%2ms").arg(B::formatNumber(p.delayMs), B::formatNumber(p.jitterMs))
                                 : QStringLiteral("%1ms").arg(B::formatNumber(p.delayMs)));
    if (p.lossModel == LossModel::Random && p.lossPct > 0)
        parts << QStringLiteral("loss %1%").arg(B::formatNumber(p.lossPct));
    if (p.lossModel == LossModel::GilbertElliott && p.geP > 0)
        parts << QStringLiteral("GE loss p=%1% r=%2%").arg(B::formatNumber(p.geP), B::formatNumber(p.geR));
    if (p.rateKbit > 0)
        parts << QStringLiteral("%1 kbit/s").arg(p.rateKbit);
    if (p.duplicatePct > 0)
        parts << QStringLiteral("dup %1%").arg(B::formatNumber(p.duplicatePct));
    if (p.reorderPct > 0)
        parts << QStringLiteral("reorder %1%").arg(B::formatNumber(p.reorderPct));
    if (p.corruptPct > 0)
        parts << QStringLiteral("corrupt %1%").arg(B::formatNumber(p.corruptPct));
    return parts.isEmpty() ? QStringLiteral("clean") : parts.join(' ');
}

QString describe(const LinkConfig &c)
{
    return QStringLiteral("up: %1 | down: %2").arg(describe(c.uplink), describe(c.downlink));
}

Profile withField(const Profile &p, const QString &key, const QVariant &value)
{
    QJsonObject o = p.toJson();
    o.insert(key, QJsonValue::fromVariant(value));
    return Profile::fromJson(o);
}

TrafficClass defaultClass(const QString &name, Proto proto, const QString &ip, quint16 port)
{
    TrafficClass c;
    c.name = name;
    c.proto = proto;
    c.ip = ip;
    c.port = port;
    return c;
}

} // namespace

LinkController::LinkController(HelperClient *helper, EventLog *log, SystemInfo *sys, QObject *parent)
    : QObject(parent), m_helper(helper), m_log(log), m_sys(sys)
{
    m_edit.classes << defaultClass("MAVLink TCP 4555", Proto::Tcp, "192.0.2.10", 4555)
                   << defaultClass("Video UDP 5600", Proto::Udp, {}, 5600);
    loadSettings();

    m_autoApplyTimer.setSingleShot(true);
    m_autoApplyTimer.setInterval(150);
    connect(&m_autoApplyTimer, &QTimer::timeout, this, [this] { sendApply(QStringLiteral("auto-apply")); });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &LinkController::saveSettings);

    m_outageTimer.setSingleShot(true);
    connect(&m_outageTimer, &QTimer::timeout, this, [this] {
        m_outageTick.stop();
        setCutSource("timed", false);
        Q_EMIT outageChanged();
    });
    m_outageTick.setInterval(100);
    connect(&m_outageTick, &QTimer::timeout, this, &LinkController::outageChanged);

    connect(m_helper, &HelperClient::lost, this, &LinkController::onHelperLost);
    connect(m_helper, &HelperClient::ready, this, [this] {
        markInactive(); // a fresh helper has cleaned everything up
        updatePreview();
    });
    connect(m_helper, &HelperClient::commandsExecuted, this, &LinkController::onCommandsExecuted);
    connect(m_sys, &SystemInfo::changed, this, &LinkController::updatePreview);
    updatePreview();
}

QVariantList LinkController::classes() const
{
    QVariantList out;
    for (const auto &c : m_edit.classes)
        out << c.toJson().toVariantMap();
    return out;
}

// ---------------------------------------------------------------- setters

void LinkController::setMode(const QString &m)
{
    Mode mode;
    if (!parse(m, &mode) || mode == m_edit.mode)
        return;
    m_edit.mode = mode;
    // Host mode impairs this PC's own NIC: keep unrelated traffic (SSH...) out by default.
    m_edit.otherMode = mode == Mode::Host ? ClassMode::Excluded : ClassMode::Link;
    edited(false);
}

void LinkController::setGroundIface(const QString &v)
{
    if (v == m_edit.groundIface)
        return;
    m_edit.groundIface = v;
    edited(false);
}

void LinkController::setAirIface(const QString &v)
{
    if (v == m_edit.airIface)
        return;
    m_edit.airIface = v;
    edited(false);
}

void LinkController::setHostIface(const QString &v)
{
    if (v == m_edit.hostIface)
        return;
    m_edit.hostIface = v;
    edited(false);
}

void LinkController::setOtherExcluded(bool v)
{
    const ClassMode m = v ? ClassMode::Excluded : ClassMode::Link;
    if (m == m_edit.otherMode)
        return;
    m_edit.otherMode = m;
    edited(true);
}

void LinkController::setAutoApply(bool v)
{
    if (v == m_autoApply)
        return;
    m_autoApply = v;
    Q_EMIT autoApplyChanged();
    m_saveTimer.start();
}

void LinkController::edited(bool autoApplicable)
{
    Q_EMIT configChanged();
    updatePreview();
    m_saveTimer.start();
    if (autoApplicable && m_autoApply && isActive())
        m_autoApplyTimer.start();
}

// ---------------------------------------------------------------- profiles

void LinkController::setProfileField(const QString &dir, const QString &key, const QVariant &value)
{
    Profile &p = dir == "uplink" ? m_edit.uplink : m_edit.downlink;
    const Profile np = withField(p, key, value);
    if (np == p)
        return;
    p = np;
    if (!m_activePreset.isEmpty() && !m_activePreset.endsWith('*'))
        m_activePreset += '*';
    edited(true);
}

void LinkController::setProfiles(const QVariantMap &uplink, const QVariantMap &downlink, const QString &presetName)
{
    m_edit.uplink = Profile::fromJson(QJsonObject::fromVariantMap(uplink));
    m_edit.downlink = Profile::fromJson(QJsonObject::fromVariantMap(downlink));
    m_activePreset = presetName;
    edited(true);
}

void LinkController::applyPreset(const QVariantMap &preset)
{
    if (preset.isEmpty())
        return;
    m_log->log("preset", QStringLiteral("preset '%1' selected").arg(preset.value("name").toString()));
    setProfiles(preset.value("uplink").toMap(), preset.value("downlink").toMap(), preset.value("name").toString());
}

void LinkController::applyProfilesLive(const Profile &up, const Profile &down, const QString &label)
{
    m_edit.uplink = up;
    m_edit.downlink = down;
    if (!label.isEmpty())
        m_activePreset = label;
    Q_EMIT configChanged();
    updatePreview();
    sendApply(QString());
}

int LinkController::autoLimit(const QVariantMap &profile) const
{
    return int(B::autoLimit(Profile::fromJson(QJsonObject::fromVariantMap(profile))));
}

// ---------------------------------------------------------------- classes

void LinkController::addClass(const QString &name, const QString &proto, const QString &ip, int port, const QString &mode)
{
    if (m_edit.classes.size() >= kMaxClasses)
        return;
    TrafficClass c;
    c.name = name.isEmpty() ? QStringLiteral("Class %1").arg(m_edit.classes.size() + 1) : name;
    parse(proto, &c.proto);
    c.ip = ip.trimmed();
    c.port = quint16(std::clamp(port, 0, 65535));
    parse(mode, &c.mode);
    c.uplink = m_edit.uplink;
    c.downlink = m_edit.downlink;
    m_edit.classes << c;
    edited(true);
}

void LinkController::removeClass(int index)
{
    if (index < 0 || index >= m_edit.classes.size())
        return;
    m_edit.classes.removeAt(index);
    edited(true);
}

void LinkController::moveClass(int index, int delta)
{
    const int to = index + delta;
    if (index < 0 || index >= m_edit.classes.size() || to < 0 || to >= m_edit.classes.size())
        return;
    m_edit.classes.move(index, to);
    edited(true);
}

void LinkController::setClassField(int index, const QString &key, const QVariant &value)
{
    if (index < 0 || index >= m_edit.classes.size())
        return;
    QJsonObject o = m_edit.classes[index].toJson();
    o.insert(key, QJsonValue::fromVariant(value));
    const TrafficClass c = TrafficClass::fromJson(o);
    if (c == m_edit.classes[index])
        return;
    m_edit.classes[index] = c;
    edited(true);
}

void LinkController::setClassProfileField(int index, const QString &dir, const QString &key, const QVariant &value)
{
    if (index < 0 || index >= m_edit.classes.size())
        return;
    TrafficClass &c = m_edit.classes[index];
    Profile &p = dir == "uplink" ? c.uplink : c.downlink;
    const Profile np = withField(p, key, value);
    if (np == p)
        return;
    p = np;
    edited(true);
}

// ---------------------------------------------------------------- lifecycle

LinkConfig LinkController::desiredConfig() const
{
    LinkConfig c = m_edit;
    c.cut = isCut();
    return c;
}

void LinkController::start()
{
    const QStringList errs = m_edit.validate(m_sys->interfaceNames());
    if (!errs.isEmpty()) {
        m_log->log("error", QStringLiteral("cannot start: %1").arg(errs.join("; ")));
        Q_EMIT errorOccurred(errs.join("\n"));
        return;
    }
    auto applyFirst = [this] { sendApply(QStringLiteral("started (%1 mode)").arg(mode()), true); };

    // Leaving bridge mode, or changing bridge ports: remove the old setup first.
    const bool modeChanged = m_applied && m_applied->mode != m_edit.mode;
    const bool leaveBridge = m_bridgeUp && m_edit.mode == Mode::Host;
    if (modeChanged || leaveBridge) {
        m_structuralBusy = true;
        Q_EMIT busyChanged();
        m_helper->request("reset", {}, [this](bool ok, const QJsonObject &, const QString &err) {
            m_structuralBusy = false;
            Q_EMIT busyChanged();
            markInactive();
            if (!ok) {
                m_log->log("error", QStringLiteral("reset failed: %1").arg(err));
                return;
            }
            start();
        });
        return;
    }

    if (m_edit.mode == Mode::Bridge) {
        m_structuralBusy = true;
        Q_EMIT busyChanged();
        const QString g = m_edit.groundIface, a = m_edit.airIface;
        m_helper->request("setup_bridge", {{"groundIface", g}, {"airIface", a}},
                          [this, g, a, applyFirst](bool ok, const QJsonObject &res, const QString &err) {
                              m_structuralBusy = false;
                              Q_EMIT busyChanged();
                              if (!ok) {
                                  m_log->log("error", QStringLiteral("bridge setup failed: %1").arg(err));
                                  Q_EMIT errorOccurred(err);
                                  return;
                              }
                              if (!res.value("unchanged").toBool()) {
                                  // setup_bridge removes any impairment it replaces.
                                  m_applied.reset();
                                  m_appliedPlan.reset();
                                  m_log->log("bridge", QStringLiteral("%1 created: ground=%2 air=%3").arg(kBridgeName, g, a));
                              }
                              m_bridgeUp = true;
                              m_bridgeGround = g;
                              m_bridgeAir = a;
                              Q_EMIT appliedChanged();
                              applyFirst();
                          });
    } else {
        applyFirst();
    }
}

void LinkController::apply()
{
    if (!isActive()) {
        start();
        return;
    }
    const bool bridgeChanged = m_edit.mode == Mode::Bridge
                               && (m_edit.groundIface != m_bridgeGround || m_edit.airIface != m_bridgeAir);
    if (bridgeChanged || m_applied->mode != m_edit.mode) {
        start();
        return;
    }
    sendApply(QStringLiteral("applied"));
}

void LinkController::sendApply(const QString &reason, bool force)
{
    if (!isActive() && !force)
        return;
    if (m_inFlight) {
        m_pendingApply = true;
        if (!reason.isEmpty())
            m_pendingReason = reason;
        return;
    }
    const LinkConfig cfg = desiredConfig();
    if (m_applied && cfg == *m_applied)
        return;
    const QStringList errs = cfg.validate(m_sys->interfaceNames());
    if (!errs.isEmpty()) {
        m_log->log("error", QStringLiteral("not applied: %1").arg(errs.join("; ")));
        return;
    }
    m_inFlight = true;
    Q_EMIT busyChanged();
    m_helper->request("apply_profile", {{"config", cfg.toJson()}},
                      [this, cfg, reason](bool ok, const QJsonObject &res, const QString &err) {
                          m_inFlight = false;
                          if (ok) {
                              const bool wasActive = isActive();
                              m_applied = cfg;
                              m_appliedPlan = B::buildPlan(cfg);
                              if (!reason.isEmpty())
                                  m_log->log("apply", QStringLiteral("%1 [%2]: %3")
                                                          .arg(reason, res.value("level").toString(), describe(cfg)));
                              if (!wasActive)
                                  Q_EMIT appliedChanged();
                          } else {
                              m_log->log("error", err);
                              Q_EMIT errorOccurred(err);
                              // The helper removes the impairment when an apply fails.
                              if (isActive()) {
                                  m_applied.reset();
                                  m_appliedPlan.reset();
                                  Q_EMIT appliedChanged();
                              }
                          }
                          Q_EMIT busyChanged();
                          updatePreview();
                          if (m_pendingApply) {
                              m_pendingApply = false;
                              sendApply(std::exchange(m_pendingReason, QString()));
                          }
                      });
}

void LinkController::stop()
{
    const QString op = m_bridgeUp ? QStringLiteral("teardown") : QStringLiteral("reset");
    m_structuralBusy = true;
    Q_EMIT busyChanged();
    m_helper->request(op, {}, [this, op](bool ok, const QJsonObject &, const QString &err) {
        m_structuralBusy = false;
        Q_EMIT busyChanged();
        if (!ok) {
            m_log->log("error", QStringLiteral("%1 failed: %2").arg(op, err));
            return;
        }
        m_log->log("apply", "impairment removed, link is clean");
        markInactive();
    });
}

void LinkController::reset()
{
    m_structuralBusy = true;
    Q_EMIT busyChanged();
    m_helper->request("reset", {}, [this](bool ok, const QJsonObject &, const QString &err) {
        m_structuralBusy = false;
        Q_EMIT busyChanged();
        if (!ok) {
            m_log->log("error", QStringLiteral("reset failed: %1").arg(err));
            return;
        }
        m_log->log("apply", "reset: every qdisc, filter, IFB and bridge created by DLSim removed");
        markInactive();
    });
}

void LinkController::markInactive()
{
    m_applied.reset();
    m_appliedPlan.reset();
    m_bridgeUp = false;
    m_bridgeGround.clear();
    m_bridgeAir.clear();
    m_cutApplied = false;
    Q_EMIT appliedChanged();
    updatePreview();
}

void LinkController::onHelperLost()
{
    m_inFlight = false;
    m_pendingApply = false;
    m_structuralBusy = false;
    Q_EMIT busyChanged();
    markInactive();
}

// ---------------------------------------------------------------- cut / restore

double LinkController::outageRemaining() const
{
    if (!m_outageTimer.isActive())
        return 0;
    return std::max(0.0, m_outageSeconds - m_outageClock.elapsed() / 1000.0);
}

void LinkController::setCut(bool cut)
{
    if (cut) {
        setCutSource("manual", true);
        return;
    }
    m_outageTimer.stop();
    m_outageTick.stop();
    Q_EMIT outageChanged();
    if (m_cutSources.isEmpty())
        return;
    m_cutSources.clear();
    m_log->log("outage", "LINK RESTORED (manual)");
    Q_EMIT cutChanged();
    updatePreview();
    sendCutState();
}

void LinkController::timedOutage(double seconds)
{
    if (seconds <= 0)
        return;
    m_outageSeconds = seconds;
    m_outageClock.start();
    m_outageTimer.start(int(seconds * 1000));
    m_outageTick.start();
    m_log->log("outage", QStringLiteral("timed outage: %1 s").arg(seconds));
    setCutSource("timed", true);
    Q_EMIT outageChanged();
}

void LinkController::setCutSource(const QString &source, bool on)
{
    const bool before = isCut();
    if (on)
        m_cutSources.insert(source);
    else
        m_cutSources.remove(source);
    const bool after = isCut();
    if (before == after)
        return;
    m_log->log("outage", after ? QStringLiteral("LINK CUT (%1)").arg(source) : QStringLiteral("LINK RESTORED (%1)").arg(source));
    Q_EMIT cutChanged();
    updatePreview();
    sendCutState();
}

void LinkController::sendCutState()
{
    if (!isActive())
        return;
    const bool want = isCut();
    m_helper->request(want ? "cut" : "restore", {}, [this, want](bool ok, const QJsonObject &, const QString &err) {
        if (!ok) {
            m_log->log("error", QStringLiteral("%1 failed: %2").arg(want ? "cut" : "restore", err));
            return;
        }
        if (m_applied) {
            m_applied->cut = want;
            m_appliedPlan = B::buildPlan(*m_applied);
        }
        updatePreview();
    });
}

// ---------------------------------------------------------------- preview

bool LinkController::isDirty() const
{
    return m_applied && !(desiredConfig() == *m_applied);
}

void LinkController::updatePreview()
{
    const LinkConfig desired = desiredConfig();
    m_validation = desired.validate(m_sys->interfaceNames());
    const Plan plan = B::buildPlan(desired);

    CommandList full;
    if (desired.mode == Mode::Bridge)
        full << B::setupBridge(desired.groundIface, desired.airIface);
    full << B::buildAll(plan);
    m_previewFull = B::render(full);

    if (!m_validation.isEmpty()) {
        m_previewLevel = QStringLiteral("invalid");
        m_previewDiff = "# configuration is not valid:\n# " + m_validation.join("\n# ");
    } else if (!isActive()) {
        m_previewLevel = QStringLiteral("start");
        CommandList cmds;
        const bool bridgeReady = m_bridgeUp && m_bridgeGround == desired.groundIface && m_bridgeAir == desired.airIface;
        if (desired.mode == Mode::Bridge && !bridgeReady)
            cmds << B::setupBridge(desired.groundIface, desired.airIface);
        cmds << B::buildAll(plan);
        m_previewDiff = B::render(cmds);
    } else {
        const Diff d = B::diff(m_appliedPlan, plan);
        m_previewLevel = toString(d.level);
        m_previewDiff = d.commands.isEmpty() ? QStringLiteral("# applied state matches, nothing to do")
                                             : B::render(d.commands);
    }
    Q_EMIT previewChanged();
}

void LinkController::onCommandsExecuted(const QString &op, const QJsonArray &cmds)
{
    QStringList lines{QStringLiteral("# %1  %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), op)};
    for (const auto &v : cmds) {
        const QJsonObject c = v.toObject();
        QString line = c.value("cmd").toString();
        if (!c.value("ok").toBool())
            line += QStringLiteral("    # exit %1%2: %3")
                        .arg(c.value("exitCode").toInt())
                        .arg(c.value("ignoreError").toBool() ? " (ignored)" : "")
                        .arg(c.value("stderr").toString());
        lines << line;
    }
    // Keep the newest ~300 lines.
    QStringList all = (lines.join('\n') + '\n' + m_lastExecuted).split('\n');
    if (all.size() > 300)
        all = all.mid(0, 300);
    m_lastExecuted = all.join('\n');
    Q_EMIT lastExecutedChanged();
}

// ---------------------------------------------------------------- persistence

void LinkController::saveSettings()
{
    QSettings s;
    LinkConfig c = m_edit;
    c.cut = false;
    s.setValue("link/config", QString::fromUtf8(QJsonDocument(c.toJson()).toJson(QJsonDocument::Compact)));
    s.setValue("link/activePreset", m_activePreset);
    s.setValue("link/autoApply", m_autoApply);
}

void LinkController::loadSettings()
{
    QSettings s;
    const QJsonDocument d = QJsonDocument::fromJson(s.value("link/config").toString().toUtf8());
    if (d.isObject()) {
        m_edit = LinkConfig::fromJson(d.object());
        m_edit.cut = false;
    }
    m_activePreset = s.value("link/activePreset").toString();
    m_autoApply = s.value("link/autoApply", true).toBool();
}
