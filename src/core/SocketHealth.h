// Plain-language status of one TCP socket, from successive `ss` samples.
//
// Rules, first match wins:
//  1. Not ESTAB: LISTEN -> "listening"; SYN-SENT/SYN-RECV -> "connecting";
//     anything else -> "closing".
//  2. Retransmit timer running ("on") with backoff > 0 or a retransmission in
//     flight -> "stuck": waiting on the retransmit timer (next try, backoff).
//  3. Persist timer -> "stuck": the peer's receive window is zero.
//  4. Data waiting (Send-Q or unacked > 0) and nothing acknowledged for
//     >= 40 s -> "halfopen": possibly half-open.
//  5. Recovery: after the link is restored (linkRestored()), the first sample
//     whose last ACK came after the restore, with backoff 0 and no retransmit
//     timer, measures the recovery time = ACK time - restore time. The
//     socket then shows "recovered" for 30 s.
//  6. Nothing sent or received for >= 40 s and nothing queued -> "idle".
//  7. Otherwise "healthy" (with the RTT).
#pragma once

#include "Parsers.h"

namespace dlsim {

class SocketHealth
{
public:
    static constexpr double kHalfOpenS = 40;
    static constexpr double kShowRecoveredS = 30;

    // The link was restored (outage ended) at nowS.
    void linkRestored(double nowS) { m_restoredAt = nowS; }
    // A new sample of this socket, taken at nowS (seconds, any monotonic clock).
    void update(const TcpSocketInfo &s, double nowS);

    QString kind() const { return m_kind; }   // listening, connecting, closing, stuck, halfopen, recovered, idle, healthy
    QString text() const { return m_text; }
    double lastRecoveryS() const { return m_recoveryS; } // -1 until measured
    bool waitingForRecovery() const { return m_restoredAt >= 0; }

private:
    QString m_kind, m_text;
    double m_restoredAt = -1;
    double m_recoveryS = -1;
    double m_recoveredAt = -1;
};

} // namespace dlsim
