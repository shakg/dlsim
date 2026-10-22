// Timeline playback, profile fades and random (Poisson) dropouts.
// Driven by one 100 ms QTimer; fade updates are pushed at fadeRateHz.
#pragma once

#include "Model.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <random>

class EventLog;
class LinkController;
class PresetStore;

class ScenarioEngine : public QObject
{
    Q_OBJECT
    // Timeline
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY stepsChanged)
    Q_PROPERTY(QVariantList steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY stepsChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged) // stopped, playing, paused
    Q_PROPERTY(int currentStep READ currentStep NOTIFY progressChanged)
    Q_PROPERTY(double stepElapsed READ stepElapsed NOTIFY progressChanged)
    Q_PROPERTY(double totalElapsed READ totalElapsed NOTIFY progressChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY stepsChanged)
    Q_PROPERTY(int loopCount READ loopCount NOTIFY progressChanged)
    // Random dropouts
    Q_PROPERTY(bool dropoutsEnabled READ dropoutsEnabled WRITE setDropoutsEnabled NOTIFY dropoutsChanged)
    Q_PROPERTY(double meanIntervalS READ meanIntervalS WRITE setMeanIntervalS NOTIFY dropoutsChanged)
    Q_PROPERTY(double meanDurationS READ meanDurationS WRITE setMeanDurationS NOTIFY dropoutsChanged)
    Q_PROPERTY(bool inDropout READ inDropout NOTIFY progressChanged)
    Q_PROPERTY(double dropoutCountdown READ dropoutCountdown NOTIFY progressChanged)
    Q_PROPERTY(int dropoutCount READ dropoutCount NOTIFY progressChanged)
    // Fade
    Q_PROPERTY(bool fading READ fading NOTIFY fadeChanged)
    Q_PROPERTY(double fadeProgress READ fadeProgress NOTIFY progressChanged)
    Q_PROPERTY(int fadeRateHz READ fadeRateHz WRITE setFadeRateHz NOTIFY fadeChanged)

public:
    ScenarioEngine(LinkController *link, PresetStore *presets, EventLog *log, QObject *parent = nullptr);

    QString name() const { return m_name; }
    QVariantList steps() const { return m_steps; }
    bool loop() const { return m_loop; }
    QString state() const;
    int currentStep() const { return m_current; }
    double stepElapsed() const { return m_stepElapsedMs / 1000.0; }
    double totalElapsed() const { return m_totalElapsedMs / 1000.0; }
    double totalDuration() const;
    int loopCount() const { return m_loopCount; }
    void setName(const QString &n);
    void setLoop(bool v);

    bool dropoutsEnabled() const { return m_dropouts; }
    double meanIntervalS() const { return m_meanIntervalS; }
    double meanDurationS() const { return m_meanDurationS; }
    bool inDropout() const { return m_inDropout; }
    double dropoutCountdown() const { return m_dropoutRemainingMs / 1000.0; }
    int dropoutCount() const { return m_dropoutCount; }
    void setDropoutsEnabled(bool v);
    void setMeanIntervalS(double v);
    void setMeanDurationS(double v);

    bool fading() const { return m_fade.active; }
    double fadeProgress() const;
    int fadeRateHz() const { return m_fadeRateHz; }
    void setFadeRateHz(int hz);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

    Q_INVOKABLE void load(const QVariantMap &scenario);
    Q_INVOKABLE QVariantMap toMap() const;
    Q_INVOKABLE void clear();
    Q_INVOKABLE void addStep(const QString &type, const QString &preset, double durationS);
    Q_INVOKABLE void removeStep(int index);
    Q_INVOKABLE void moveStep(int index, int delta);
    Q_INVOKABLE void setStepField(int index, const QString &key, const QVariant &value);

    // Fade the live link from its current profiles to the named preset.
    Q_INVOKABLE void fadeTo(const QString &preset, double seconds);
    Q_INVOKABLE void fadeBack(); // reverse the last fade
    Q_INVOKABLE void stopFade();

Q_SIGNALS:
    void stepsChanged();
    void stateChanged();
    void progressChanged();
    void dropoutsChanged();
    void fadeChanged();

private:
    enum class State { Stopped, Playing, Paused };
    struct Fade {
        bool active = false;
        dlsim::Profile fromUp, fromDown, toUp, toDown;
        QString label;
        double durationMs = 0;
        double elapsedMs = 0;
        double sinceUpdateMs = 1e9;
    };

    void tick();
    void enterStep(int index);
    void leaveStep();
    void scheduleNextDropout();
    void startFade(const dlsim::Profile &toUp, const dlsim::Profile &toDown, double seconds, const QString &label);
    void advanceFade(double dtMs);
    bool presetProfiles(const QString &name, dlsim::Profile *up, dlsim::Profile *down) const;
    double exponential(double mean);
    void updateTimer();

    LinkController *m_link;
    PresetStore *m_presets;
    EventLog *m_log;

    QString m_name = QStringLiteral("Untitled");
    QVariantList m_steps;
    bool m_loop = false;
    State m_state = State::Stopped;
    int m_current = -1;
    double m_stepElapsedMs = 0;
    double m_totalElapsedMs = 0;
    int m_loopCount = 0;

    bool m_dropouts = false;
    double m_meanIntervalS = 30;
    double m_meanDurationS = 3;
    bool m_inDropout = false;
    double m_dropoutRemainingMs = 0;
    int m_dropoutCount = 0;

    Fade m_fade;
    Fade m_lastFade;
    int m_fadeRateHz = 10;

    QTimer m_timer;
    QElapsedTimer m_clock;
    std::mt19937 m_rng{std::random_device{}()};
};
