// Polls `ss -tnoi` (via the helper) at 1 Hz for connections to/from a peer
// ip:port. Tracks state changes, retransmissions, RTO and backoff so the TCP
// stall after a link outage can be seen and exported.
#pragma once

#include "Parsers.h"
#include "SocketHealth.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class QAbstractSeries;
class HelperClient;
class EventLog;

class SocketMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString peerIp READ peerIp WRITE setPeerIp NOTIFY configChanged)
    Q_PROPERTY(int peerPort READ peerPort WRITE setPeerPort NOTIFY configChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QVariantList sockets READ sockets NOTIFY updated)
    Q_PROPERTY(QString lastError READ lastError NOTIFY updated)
    Q_PROPERTY(QString tracked READ tracked NOTIFY updated)
public:
    SocketMonitor(HelperClient *helper, EventLog *log, QObject *parent = nullptr);

    QString peerIp() const { return m_ip; }
    int peerPort() const { return m_port; }
    bool running() const { return m_timer.isActive(); }
    QVariantList sockets() const { return m_rows; }
    QString lastError() const { return m_lastError; }
    QString tracked() const { return m_trackedKey; }
    void setPeerIp(const QString &ip);
    void setPeerPort(int port);

    // Called when the link is cut or restored; a restore starts the recovery clock.
    void setLinkCut(bool cut);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    // key: rto, retrans, backoff, sendq, rtt, timer. Returns {xmin, xmax, ymax}.
    Q_INVOKABLE QVariantMap updateSeries(QAbstractSeries *series, const QString &key, int windowSeconds) const;
    Q_INVOKABLE bool exportCsv(const QUrl &file);

Q_SIGNALS:
    void configChanged();
    void runningChanged();
    void updated();
    void failed(const QString &error); // on the first failure after a success
    void recovered(const QString &socket, double seconds); // data acknowledged again after a restore

private:
    struct Point {
        double t;
        dlsim::TcpSocketInfo s;
        bool present;
    };
    void poll();
    void process(const QList<dlsim::TcpSocketInfo> &socks);

    HelperClient *m_helper;
    EventLog *m_log;
    QString m_ip = QStringLiteral("192.0.2.10");
    int m_port = 4555;
    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_inFlight = false;
    QVariantList m_rows;
    QString m_lastError;
    QHash<QString, QString> m_states; // key -> state
    QHash<QString, int> m_backoff;
    QHash<QString, dlsim::SocketHealth> m_health;
    bool m_linkCut = false;
    double m_restoredAt = -1; // for sockets first seen after the restore
    QString m_trackedKey;
    QList<Point> m_history;                              // tracked socket over time
    QList<QPair<double, dlsim::TcpSocketInfo>> m_allRows; // every socket, every poll, for CSV
};
