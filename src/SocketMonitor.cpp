#include "SocketMonitor.h"

#include "EventLog.h"
#include "HelperClient.h"
#include "Model.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtCharts/QXYSeries>

using namespace dlsim;

namespace {
constexpr int kMaxHistory = 24 * 3600;
}

SocketMonitor::SocketMonitor(HelperClient *helper, EventLog *log, QObject *parent)
    : QObject(parent), m_helper(helper), m_log(log)
{
    m_clock.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &SocketMonitor::poll);
}

void SocketMonitor::setPeerIp(const QString &ip)
{
    if (ip.trimmed() == m_ip)
        return;
    m_ip = ip.trimmed();
    Q_EMIT configChanged();
}

void SocketMonitor::setPeerPort(int port)
{
    if (port == m_port)
        return;
    m_port = port;
    Q_EMIT configChanged();
}

void SocketMonitor::start()
{
    if (!isValidIp(m_ip) || m_port < 1 || m_port > 65535) {
        m_lastError = QStringLiteral("enter a valid IPv4 address and port");
        Q_EMIT updated();
        return;
    }
    m_states.clear();
    m_backoff.clear();
    m_log->log("socket", QStringLiteral("monitoring TCP %1:%2").arg(m_ip).arg(m_port));
    m_timer.start();
    Q_EMIT runningChanged();
    poll();
}

void SocketMonitor::stop()
{
    if (!m_timer.isActive())
        return;
    m_timer.stop();
    m_log->log("socket", "socket monitor stopped");
    Q_EMIT runningChanged();
}

void SocketMonitor::clear()
{
    m_history.clear();
    m_allRows.clear();
    Q_EMIT updated();
}

void SocketMonitor::poll()
{
    if (!m_helper->isRunning() || m_inFlight)
        return;
    m_inFlight = true;
    m_helper->request("read_sockets", {{"ip", m_ip}, {"port", m_port}},
                      [this](bool ok, const QJsonObject &res, const QString &err) {
                          m_inFlight = false;
                          if (!ok) {
                              m_lastError = err;
                              Q_EMIT updated();
                              return;
                          }
                          m_lastError.clear();
                          QList<TcpSocketInfo> socks;
                          for (const auto &v : res.value("sockets").toArray())
                              socks << TcpSocketInfo::fromJson(v.toObject());
                          process(socks);
                      });
}

void SocketMonitor::process(const QList<TcpSocketInfo> &socks)
{
    const double t = m_clock.elapsed() / 1000.0;
    QHash<QString, QString> now;
    m_rows.clear();
    for (const auto &s : socks) {
        now.insert(s.key(), s.state);
        m_allRows << qMakePair(t, s);
        QVariantMap row = s.toJson().toVariantMap();
        row["key"] = s.key();
        m_rows << row;

        const QString prev = m_states.value(s.key());
        if (prev.isEmpty())
            m_log->log("socket", QStringLiteral("%1 %2 (rto %3 ms, timer %4)")
                                     .arg(s.key(), s.state).arg(s.rtoMs).arg(s.timer.isEmpty() ? "-" : s.timer));
        else if (prev != s.state)
            m_log->log("socket", QStringLiteral("%1 %2 -> %3").arg(s.key(), prev, s.state));
        const int prevBackoff = m_backoff.value(s.key(), 0);
        if (s.backoff != prevBackoff)
            m_log->log("socket", QStringLiteral("%1 backoff %2 -> %3, rto %4 ms, retrans %5")
                                     .arg(s.key()).arg(prevBackoff).arg(s.backoff).arg(s.rtoMs).arg(s.retransTotal));
        m_backoff.insert(s.key(), s.backoff);
    }
    for (auto it = m_states.cbegin(); it != m_states.cend(); ++it) {
        if (!now.contains(it.key())) {
            m_log->log("socket", QStringLiteral("%1 gone (was %2)").arg(it.key(), it.value()));
            m_backoff.remove(it.key());
        }
    }
    m_states = now;

    // Track one socket for the charts: keep the current one while it exists,
    // otherwise prefer an established connection.
    if (!now.contains(m_trackedKey)) {
        m_trackedKey.clear();
        for (const auto &s : socks)
            if (s.state == "ESTAB") {
                m_trackedKey = s.key();
                break;
            }
        if (m_trackedKey.isEmpty() && !socks.isEmpty())
            m_trackedKey = socks.first().key();
    }
    Point p{t, {}, false};
    for (const auto &s : socks)
        if (s.key() == m_trackedKey) {
            p.s = s;
            p.present = true;
        }
    m_history << p;
    if (m_history.size() > kMaxHistory)
        m_history.remove(0, m_history.size() - kMaxHistory);
    if (m_allRows.size() > 10 * kMaxHistory)
        m_allRows.remove(0, m_allRows.size() - 10 * kMaxHistory);
    Q_EMIT updated();
}

QVariantMap SocketMonitor::updateSeries(QAbstractSeries *series, const QString &key, int windowSeconds) const
{
    const double now = m_clock.elapsed() / 1000.0;
    const double xmax = std::max(now, double(windowSeconds));
    const double xmin = xmax - windowSeconds;
    double ymax = 0;
    QList<QPointF> pts;
    for (const Point &p : m_history) {
        if (p.t < xmin || !p.present)
            continue;
        double v = 0;
        if (key == "rto") v = p.s.rtoMs / 1000.0;
        else if (key == "retrans") v = std::max(0, p.s.retransTotal);
        else if (key == "backoff") v = p.s.backoff;
        else if (key == "sendq") v = p.s.sendQ / 1024.0;
        else if (key == "rtt") v = std::max(0.0, p.s.rttMs);
        else if (key == "timer") v = std::max(0.0, p.s.timerMs / 1000.0);
        ymax = std::max(ymax, v);
        pts << QPointF(p.t, v);
    }
    if (auto *xy = qobject_cast<QXYSeries *>(series))
        xy->replace(pts);
    return {{"xmin", xmin}, {"xmax", xmax}, {"ymax", ymax}};
}

bool SocketMonitor::exportCsv(const QUrl &file)
{
    QString out = "t_s,socket,state,recv_q,send_q,timer,timer_ms,timer_retrans,rto_ms,rtt_ms,rttvar_ms,"
                  "retrans_cur,retrans_total,backoff,unacked,cwnd,lastsnd_ms,lastrcv_ms,lastack_ms\n";
    for (const auto &[t, s] : m_allRows) {
        out += QStringList{
            QString::number(t, 'f', 3), csvEscape(s.key()), s.state, QString::number(s.recvQ), QString::number(s.sendQ),
            s.timer, QString::number(s.timerMs), QString::number(s.timerRetrans), QString::number(s.rtoMs),
            QString::number(s.rttMs), QString::number(s.rttVarMs), QString::number(s.retransCur),
            QString::number(s.retransTotal), QString::number(s.backoff), QString::number(s.unacked),
            QString::number(s.cwnd), QString::number(s.lastSndMs), QString::number(s.lastRcvMs),
            QString::number(s.lastAckMs)}.join(',');
        out += '\n';
    }
    QString err;
    if (!writeTextFile(file, out, &err)) {
        m_log->log("error", QStringLiteral("socket export failed: %1").arg(err));
        return false;
    }
    m_log->log("info", QStringLiteral("socket history exported to %1").arg(file.toLocalFile()));
    return true;
}
