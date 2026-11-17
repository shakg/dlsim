#include "SocketHealth.h"

namespace dlsim {

namespace {
QString secs(double s) { return QString::number(s, 'f', s < 100 ? 1 : 0) + " s"; }
} // namespace

void SocketHealth::update(const TcpSocketInfo &s, double nowS)
{
    const bool retransmitting = s.timer == "on" && (s.backoff > 0 || s.retransCur > 0);
    const double sinceAckS = s.lastAckMs >= 0 ? s.lastAckMs / 1000.0 : -1;

    // Recovery: data acknowledged after the restore, and no longer backing off.
    if (m_restoredAt >= 0 && s.state == "ESTAB" && !retransmitting && s.backoff == 0 && sinceAckS >= 0) {
        const double ackAt = nowS - sinceAckS;
        if (ackAt >= m_restoredAt) {
            m_recoveryS = std::max(0.0, ackAt - m_restoredAt);
            m_recoveredAt = nowS;
            m_restoredAt = -1;
        }
    }

    if (s.state != "ESTAB") {
        if (s.state == "LISTEN" || s.state == "UNCONN") {
            m_kind = "listening";
            m_text = QStringLiteral("Listening for connections");
        } else if (s.state.startsWith("SYN")) {
            m_kind = "connecting";
            m_text = QStringLiteral("Connecting · %1").arg(s.state);
        } else {
            m_kind = "closing";
            m_text = QStringLiteral("Closing · %1").arg(s.state);
        }
    } else if (retransmitting) {
        m_kind = "stuck";
        m_text = QStringLiteral("Stuck · waiting on retransmit timer, next try in %1 (backoff %2)")
                     .arg(secs(std::max(0.0, s.timerMs) / 1000.0)).arg(s.backoff);
    } else if (s.timer == "persist") {
        m_kind = "stuck";
        m_text = QStringLiteral("Stuck · the peer's receive window is zero, next probe in %1")
                     .arg(secs(std::max(0.0, s.timerMs) / 1000.0));
    } else if ((s.sendQ > 0 || s.unacked > 0) && sinceAckS >= kHalfOpenS) {
        m_kind = "halfopen";
        m_text = QStringLiteral("Possibly half-open · ESTAB, no data acknowledged for %1").arg(secs(sinceAckS));
    } else if (m_recoveredAt >= 0 && nowS - m_recoveredAt < kShowRecoveredS) {
        m_kind = "recovered";
        m_text = QStringLiteral("Recovered %1 after link was restored").arg(secs(m_recoveryS));
    } else if (s.sendQ == 0 && s.unacked == 0 && s.lastSndMs >= kHalfOpenS * 1000 && s.lastRcvMs >= kHalfOpenS * 1000) {
        m_kind = "idle";
        m_text = QStringLiteral("Idle · nothing sent or received for %1").arg(secs(std::min(s.lastSndMs, s.lastRcvMs) / 1000.0));
    } else {
        m_kind = "healthy";
        m_text = s.rttMs >= 0 ? QStringLiteral("Healthy · RTT %1 ms").arg(s.rttMs < 1 ? QString::number(s.rttMs, 'f', 2)
                                                                                     : QString::number(qRound(s.rttMs)))
                              : QStringLiteral("Healthy");
    }
    if (m_restoredAt >= 0 && m_kind != "connecting" && m_kind != "closing" && m_kind != "listening")
        m_text += QStringLiteral(" · link restored %1 ago, waiting for data").arg(secs(nowS - m_restoredAt));
}

} // namespace dlsim
