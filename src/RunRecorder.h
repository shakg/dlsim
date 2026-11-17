// Records a scenario run (steps, outages, stats, sockets, log) and, when it
// finishes or is stopped, saves a report: JSON + HTML in
// ~/.config/dlsim/reports/. See dlsim::renderReportHtml for the content.
#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <QVariantList>

class EventLog;
class LinkController;
class ScenarioEngine;
class SocketMonitor;
class StatsPoller;

class RunRecorder : public QObject
{
    Q_OBJECT
    // Newest first: [{json, html, name, when, durationS, completed}]
    Q_PROPERTY(QVariantList reports READ reports NOTIFY reportsChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
public:
    RunRecorder(ScenarioEngine *scenario, LinkController *link, StatsPoller *stats, SocketMonitor *sockets,
                EventLog *log, QObject *parent = nullptr);

    QVariantList reports() const;
    bool recording() const { return m_recording; }
    QString dir() const { return m_dir; }

    Q_INVOKABLE bool openReport(const QString &htmlPath);
    Q_INVOKABLE bool exportHtml(const QString &htmlPath, const QUrl &file);
    // stats.csv, sockets.csv and log.csv in one zip.
    Q_INVOKABLE bool exportCsvZip(const QString &jsonPath, const QUrl &file);
    Q_INVOKABLE bool deleteReport(const QString &jsonPath);

Q_SIGNALS:
    void reportsChanged();
    void recordingChanged();
    void reportReady(const QString &name, const QString &htmlPath);

private:
    void begin();
    void finish(bool completed);
    void onProgress();
    void onCut();
    double now() const { return m_clock.elapsed() / 1000.0; }
    QJsonObject configJson() const;

    ScenarioEngine *m_scenario;
    LinkController *m_link;
    StatsPoller *m_stats;
    SocketMonitor *m_sockets;
    EventLog *m_log;
    QString m_dir;

    bool m_recording = false;
    QString m_lastState = QStringLiteral("stopped");
    QElapsedTimer m_clock;
    QDateTime m_started;
    QString m_name;
    QJsonObject m_config;
    QJsonArray m_steps, m_outages, m_samples, m_socketRows, m_logRows;
    int m_currentStep = -1;
    double m_stepStart = 0;
    bool m_cut = false;
    double m_cutStart = 0;
    double m_dropsAtCut[2] = {0, 0};
    QStringList m_cutSources;
};
