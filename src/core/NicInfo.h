// Live per-NIC facts from /sys/class/net, readable without root: cable
// (carrier), speed, byte counters, and whether it is a physical device.
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace dlsim {

struct NicSample {
    int carrier = -1;      // 1 cable in / link up, 0 no link, -1 unknown (e.g. interface down)
    int speedMbps = -1;    // -1 unknown
    quint64 rxBytes = 0, txBytes = 0;
    bool physical = false; // has a backing device (not veth, bridge, ifb, ...)
};

// sysRoot: normally "/sys/class/net"; tests pass a fake tree.
NicSample readNic(const QString &sysRoot, const QString &name);

// kbit/s between two counters taken dtMs apart; 0 on counter reset.
double rateKbps(quint64 before, quint64 after, double dtMs);

// "Unplug the cable going to the vehicle": remembers which NICs have a link
// when started, then reports the one that lost it.
class UnplugDetector
{
public:
    enum class State { Idle, Waiting, Found, Ambiguous };
    // carriers: name -> carrier at the start. Only NICs with a link count.
    void start(const QHash<QString, int> &carriers);
    void cancel() { m_state = State::Idle; }
    // Feed the latest carriers; returns the state.
    State update(const QHash<QString, int> &carriers);
    State state() const { return m_state; }
    QString found() const { return m_found; }      // Found: the NIC
    QStringList lost() const { return m_lost; }    // Ambiguous: all that lost it
    QStringList watched() const { return m_watched; }

private:
    State m_state = State::Idle;
    QStringList m_watched, m_lost;
    QString m_found;
};

} // namespace dlsim
