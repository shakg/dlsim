#include "ScenarioEngine.h"

#include "EventLog.h"
#include "LinkController.h"
#include "PresetStore.h"

#include <QJsonObject>

using namespace dlsim;

namespace {
constexpr int kTickMs = 100;
}

ScenarioEngine::ScenarioEngine(LinkController *link, PresetStore *presets, EventLog *log, QObject *parent)
    : QObject(parent), m_link(link), m_presets(presets), m_log(log)
{
    m_timer.setInterval(kTickMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &ScenarioEngine::tick);
}

QString ScenarioEngine::state() const
{
    switch (m_state) {
    case State::Stopped: return QStringLiteral("stopped");
    case State::Playing: return QStringLiteral("playing");
    case State::Paused: return QStringLiteral("paused");
    }
    return {};
}

double ScenarioEngine::totalDuration() const
{
    double t = 0;
    for (const auto &s : m_steps)
        t += s.toMap().value("durationS").toDouble();
    return t;
}

void ScenarioEngine::setName(const QString &n)
{
    if (n == m_name)
        return;
    m_name = n;
    Q_EMIT stepsChanged();
}

void ScenarioEngine::setLoop(bool v)
{
    if (v == m_loop)
        return;
    m_loop = v;
    Q_EMIT stepsChanged();
}

// ---------------------------------------------------------------- editing

void ScenarioEngine::load(const QVariantMap &scenario)
{
    stop();
    m_name = scenario.value("name").toString();
    m_loop = scenario.value("loop").toBool();
    m_steps = scenario.value("steps").toList();
    Q_EMIT stepsChanged();
}

QVariantMap ScenarioEngine::toMap() const
{
    return {{"name", m_name}, {"loop", m_loop}, {"steps", m_steps}};
}

void ScenarioEngine::clear()
{
    stop();
    m_steps.clear();
    m_name = QStringLiteral("Untitled");
    Q_EMIT stepsChanged();
}

void ScenarioEngine::addStep(const QString &type, const QString &preset, double durationS)
{
    m_steps << QVariantMap{{"type", type}, {"preset", preset}, {"durationS", std::max(0.1, durationS)}};
    Q_EMIT stepsChanged();
}

void ScenarioEngine::removeStep(int index)
{
    if (index < 0 || index >= m_steps.size())
        return;
    if (m_state != State::Stopped)
        stop();
    m_steps.removeAt(index);
    Q_EMIT stepsChanged();
}

void ScenarioEngine::moveStep(int index, int delta)
{
    const int to = index + delta;
    if (index < 0 || index >= m_steps.size() || to < 0 || to >= m_steps.size())
        return;
    if (m_state != State::Stopped)
        stop();
    m_steps.move(index, to);
    Q_EMIT stepsChanged();
}

void ScenarioEngine::setStepField(int index, const QString &key, const QVariant &value)
{
    if (index < 0 || index >= m_steps.size())
        return;
    QVariantMap s = m_steps[index].toMap();
    if (key == "durationS")
        s[key] = std::clamp(value.toDouble(), 0.1, 86400.0);
    else
        s[key] = value;
    m_steps[index] = s;
    Q_EMIT stepsChanged();
}

// ---------------------------------------------------------------- transport

void ScenarioEngine::play()
{
    if (m_steps.isEmpty())
        return;
    if (m_state == State::Paused) {
        m_state = State::Playing;
        m_log->log("scenario", QStringLiteral("'%1' resumed").arg(m_name));
    } else if (m_state == State::Stopped) {
        m_state = State::Playing;
        m_totalElapsedMs = 0;
        m_loopCount = 0;
        m_log->log("scenario", QStringLiteral("'%1' started").arg(m_name));
        enterStep(0);
    }
    Q_EMIT stateChanged();
    updateTimer();
}

void ScenarioEngine::pause()
{
    if (m_state != State::Playing)
        return;
    m_state = State::Paused;
    m_log->log("scenario", QStringLiteral("'%1' paused at step %2").arg(m_name).arg(m_current + 1));
    Q_EMIT stateChanged();
    updateTimer();
}

void ScenarioEngine::stop()
{
    if (m_state == State::Stopped)
        return;
    if (m_current >= 0 && m_steps.value(m_current).toMap().value("type").toString() == "fade")
        stopFade(); // abort where it is instead of jumping to the target
    leaveStep();
    m_state = State::Stopped;
    m_current = -1;
    m_stepElapsedMs = 0;
    m_log->log("scenario", QStringLiteral("'%1' stopped").arg(m_name));
    Q_EMIT stateChanged();
    Q_EMIT progressChanged();
    updateTimer();
}

bool ScenarioEngine::presetProfiles(const QString &name, Profile *up, Profile *down) const
{
    const QVariantMap p = m_presets->preset(name);
    if (p.isEmpty())
        return false;
    *up = Profile::fromJson(QJsonObject::fromVariantMap(p.value("uplink").toMap()));
    *down = Profile::fromJson(QJsonObject::fromVariantMap(p.value("downlink").toMap()));
    return true;
}

void ScenarioEngine::enterStep(int index)
{
    m_current = index;
    m_stepElapsedMs = 0;
    const QVariantMap s = m_steps[index].toMap();
    const QString type = s.value("type").toString();
    const QString preset = s.value("preset").toString();
    const double dur = s.value("durationS").toDouble();
    m_log->log("scenario", QStringLiteral("step %1/%2: %3%4 for %5 s")
                               .arg(index + 1).arg(m_steps.size())
                               .arg(type, preset.isEmpty() || type == "outage" ? QString() : " " + preset)
                               .arg(dur));
    if (type == "outage") {
        m_link->setCutSource("scenario", true);
    } else {
        Profile up, down;
        if (!presetProfiles(preset, &up, &down)) {
            m_log->log("error", QStringLiteral("scenario step %1: unknown preset '%2', skipped").arg(index + 1).arg(preset));
            m_stepElapsedMs = dur * 1000; // skip on the next tick
        } else if (type == "fade") {
            startFade(up, down, dur, preset);
        } else {
            m_link->applyProfilesLive(up, down, preset);
        }
    }
    Q_EMIT progressChanged();
}

void ScenarioEngine::leaveStep()
{
    if (m_current < 0 || m_current >= m_steps.size())
        return;
    const QString type = m_steps[m_current].toMap().value("type").toString();
    if (type == "outage")
        m_link->setCutSource("scenario", false);
    if (type == "fade" && m_fade.active) {
        // Land exactly on the target.
        m_link->applyProfilesLive(m_fade.toUp, m_fade.toDown, m_fade.label);
        m_lastFade = m_fade;
        m_fade.active = false;
        Q_EMIT fadeChanged();
    }
}

// ---------------------------------------------------------------- fades

void ScenarioEngine::setFadeRateHz(int hz)
{
    hz = std::clamp(hz, 1, 10);
    if (hz == m_fadeRateHz)
        return;
    m_fadeRateHz = hz;
    Q_EMIT fadeChanged();
}

double ScenarioEngine::fadeProgress() const
{
    if (!m_fade.active || m_fade.durationMs <= 0)
        return 0;
    return std::clamp(m_fade.elapsedMs / m_fade.durationMs, 0.0, 1.0);
}

void ScenarioEngine::startFade(const Profile &toUp, const Profile &toDown, double seconds, const QString &label)
{
    m_fade = {};
    m_fade.active = true;
    m_fade.fromUp = m_link->profile(Direction::Uplink);
    m_fade.fromDown = m_link->profile(Direction::Downlink);
    m_fade.toUp = toUp;
    m_fade.toDown = toDown;
    m_fade.label = label;
    m_fade.durationMs = std::max(100.0, seconds * 1000.0);
    m_log->log("fade", QStringLiteral("fade to '%1' over %2 s at %3 Hz").arg(label).arg(seconds).arg(m_fadeRateHz));
    Q_EMIT fadeChanged();
    updateTimer();
}

void ScenarioEngine::fadeTo(const QString &preset, double seconds)
{
    Profile up, down;
    if (!presetProfiles(preset, &up, &down)) {
        m_log->log("error", QStringLiteral("fade: unknown preset '%1'").arg(preset));
        return;
    }
    startFade(up, down, seconds, preset);
}

void ScenarioEngine::fadeBack()
{
    const Fade &f = m_fade.active ? m_fade : m_lastFade;
    if (f.durationMs <= 0)
        return;
    const Profile up = f.fromUp, down = f.fromDown;
    const double seconds = f.durationMs / 1000.0;
    stopFade();
    startFade(up, down, seconds, QStringLiteral("fade back"));
}

void ScenarioEngine::stopFade()
{
    if (!m_fade.active)
        return;
    m_lastFade = m_fade;
    m_fade.active = false;
    m_log->log("fade", "fade stopped");
    Q_EMIT fadeChanged();
    updateTimer();
}

void ScenarioEngine::advanceFade(double dtMs)
{
    m_fade.elapsedMs += dtMs;
    m_fade.sinceUpdateMs += dtMs;
    const bool done = m_fade.elapsedMs >= m_fade.durationMs;
    if (!done && m_fade.sinceUpdateMs < 1000.0 / m_fadeRateHz - 1)
        return;
    m_fade.sinceUpdateMs = 0;
    const double t = std::clamp(m_fade.elapsedMs / m_fade.durationMs, 0.0, 1.0);
    m_link->applyProfilesLive(interpolate(m_fade.fromUp, m_fade.toUp, t),
                              interpolate(m_fade.fromDown, m_fade.toDown, t),
                              done ? m_fade.label : QStringLiteral("fading"));
    if (done) {
        m_log->log("fade", QStringLiteral("fade to '%1' complete").arg(m_fade.label));
        m_lastFade = m_fade;
        m_fade.active = false;
        Q_EMIT fadeChanged();
    }
}

// ---------------------------------------------------------------- dropouts

double ScenarioEngine::exponential(double mean)
{
    std::exponential_distribution<double> d(1.0 / std::max(0.01, mean));
    return d(m_rng);
}

void ScenarioEngine::scheduleNextDropout()
{
    m_dropoutRemainingMs = exponential(m_meanIntervalS) * 1000.0;
}

void ScenarioEngine::setDropoutsEnabled(bool v)
{
    if (v == m_dropouts)
        return;
    m_dropouts = v;
    if (v) {
        m_dropoutCount = 0;
        scheduleNextDropout();
        m_log->log("dropouts", QStringLiteral("random dropouts on: mean interval %1 s, mean duration %2 s")
                                   .arg(m_meanIntervalS).arg(m_meanDurationS));
    } else {
        if (m_inDropout)
            m_link->setCutSource("dropout", false);
        m_inDropout = false;
        m_log->log("dropouts", "random dropouts off");
    }
    Q_EMIT dropoutsChanged();
    Q_EMIT progressChanged();
    updateTimer();
}

void ScenarioEngine::setMeanIntervalS(double v)
{
    v = std::clamp(v, 0.5, 86400.0);
    if (v == m_meanIntervalS)
        return;
    m_meanIntervalS = v;
    Q_EMIT dropoutsChanged();
}

void ScenarioEngine::setMeanDurationS(double v)
{
    v = std::clamp(v, 0.1, 3600.0);
    if (v == m_meanDurationS)
        return;
    m_meanDurationS = v;
    Q_EMIT dropoutsChanged();
}

// ---------------------------------------------------------------- clock

void ScenarioEngine::updateTimer()
{
    const bool needed = m_state == State::Playing || m_fade.active || m_dropouts;
    if (needed && !m_timer.isActive()) {
        m_clock.start();
        m_timer.start();
    } else if (!needed) {
        m_timer.stop();
    }
}

void ScenarioEngine::tick()
{
    const double dt = double(m_clock.restart());

    // A fade that belongs to the timeline pauses with it; a manual fade does not.
    const bool timelineFade = m_current >= 0 && m_steps.value(m_current).toMap().value("type").toString() == "fade";
    if (m_fade.active && !(timelineFade && m_state == State::Paused))
        advanceFade(dt);

    if (m_dropouts) {
        m_dropoutRemainingMs -= dt;
        if (m_dropoutRemainingMs <= 0) {
            if (m_inDropout) {
                m_inDropout = false;
                m_link->setCutSource("dropout", false);
                scheduleNextDropout();
            } else {
                m_inDropout = true;
                ++m_dropoutCount;
                const double dur = exponential(m_meanDurationS);
                m_dropoutRemainingMs = dur * 1000.0;
                m_log->log("dropouts", QStringLiteral("dropout #%1 for %2 s").arg(m_dropoutCount).arg(dur, 0, 'f', 1));
                m_link->setCutSource("dropout", true);
            }
        }
    }

    if (m_state == State::Playing && m_current >= 0) {
        m_stepElapsedMs += dt;
        m_totalElapsedMs += dt;
        const double dur = m_steps[m_current].toMap().value("durationS").toDouble() * 1000.0;
        if (m_stepElapsedMs >= dur) {
            leaveStep();
            int next = m_current + 1;
            if (next >= m_steps.size()) {
                if (m_loop) {
                    next = 0;
                    ++m_loopCount;
                    m_totalElapsedMs = 0;
                } else {
                    m_log->log("scenario", QStringLiteral("'%1' finished").arg(m_name));
                    m_state = State::Stopped;
                    m_current = -1;
                    Q_EMIT stateChanged();
                    Q_EMIT progressChanged();
                    updateTimer();
                    return;
                }
            }
            enterStep(next);
        }
    }
    Q_EMIT progressChanged();
}
