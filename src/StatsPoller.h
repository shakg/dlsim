// Polls `tc -s -j qdisc show` (via the helper) at 1 Hz and turns the
// counters into per-direction rates for the Monitor charts.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class QAbstractSeries;
class HelperClient;
class LinkController;
class EventLog;

class StatsPoller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap uplink READ uplink NOTIFY sampled)
    Q_PROPERTY(QVariantMap downlink READ downlink NOTIFY sampled)
    Q_PROPERTY(double now READ now NOTIFY sampled)
    Q_PROPERTY(int windowSeconds READ windowSeconds WRITE setWindowSeconds NOTIFY windowChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY sampled)
public:
    StatsPoller(HelperClient *helper, LinkController *link, EventLog *log, QObject *parent = nullptr);

    QVariantMap uplink() const { return m_latest[0]; }
    QVariantMap downlink() const { return m_latest[1]; }
    double now() const { return m_clock.elapsed() / 1000.0; }
    int windowSeconds() const { return m_window; }
    void setWindowSeconds(int s);
    int sampleCount() const { return int(m_samples.size()); }

    // key: "up.kbps", "down.dropsPerSec", "up.backlog", ... Returns {xmin, xmax, ymax}.
    Q_INVOKABLE QVariantMap updateSeries(QAbstractSeries *series, const QString &key) const;
    Q_INVOKABLE bool exportCsv(const QUrl &file);
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void sampled();
    void windowChanged();

private:
    struct Counters {
        bool valid = false;
        double bytes = 0, packets = 0, drops = 0, overlimits = 0, backlog = 0, qlen = 0;
    };
    struct Rates {
        double kbps = 0, pps = 0, dropsPerSec = 0, overlimitsPerSec = 0, backlog = 0, qlen = 0;
    };
    struct Sample {
        double t = 0;
        bool cut = false;
        Counters c[2];
        Rates r[2];
    };

    void poll();
    static double value(const Sample &s, const QString &key);

    HelperClient *m_helper;
    LinkController *m_link;
    EventLog *m_log;
    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_inFlight = false;
    QList<Sample> m_samples;
    QVariantMap m_latest[2];
    int m_window = 120;
    QString m_lastError;
};
