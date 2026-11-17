#include "NicInfo.h"

#include <QDir>
#include <QFile>

namespace dlsim {

namespace {
QString readLine(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(f.readLine()).trimmed();
}
} // namespace

NicSample readNic(const QString &sysRoot, const QString &name)
{
    const QString base = sysRoot + '/' + name;
    NicSample s;
    bool ok = false;
    const int carrier = readLine(base + "/carrier").toInt(&ok); // unreadable while the interface is down
    s.carrier = ok ? (carrier ? 1 : 0) : -1;
    const int speed = readLine(base + "/speed").toInt(&ok);
    s.speedMbps = ok && speed > 0 ? speed : -1;
    s.rxBytes = readLine(base + "/statistics/rx_bytes").toULongLong();
    s.txBytes = readLine(base + "/statistics/tx_bytes").toULongLong();
    s.physical = QFileInfo::exists(base + "/device");
    return s;
}

double rateKbps(quint64 before, quint64 after, double dtMs)
{
    if (after < before || dtMs <= 0)
        return 0;
    return double(after - before) * 8.0 / dtMs; // bytes/ms * 8 = kbit/s
}

void UnplugDetector::start(const QHash<QString, int> &carriers)
{
    m_watched.clear();
    m_lost.clear();
    m_found.clear();
    for (auto it = carriers.cbegin(); it != carriers.cend(); ++it)
        if (it.value() == 1)
            m_watched << it.key();
    m_watched.sort();
    m_state = State::Waiting;
}

UnplugDetector::State UnplugDetector::update(const QHash<QString, int> &carriers)
{
    if (m_state != State::Waiting)
        return m_state;
    QStringList lost;
    for (const QString &n : m_watched)
        if (carriers.value(n, 0) != 1)
            lost << n;
    if (lost.size() == 1) {
        m_found = lost.first();
        m_state = State::Found;
    } else if (lost.size() > 1) {
        m_lost = lost;
        m_state = State::Ambiguous;
    }
    return m_state;
}

} // namespace dlsim
