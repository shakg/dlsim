#include "SystemInfo.h"

#include "Model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

namespace {

QByteArray runTool(const QString &program, const QStringList &args)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(3000)) {
        p.kill();
        return {};
    }
    return p.readAllStandardOutput();
}

QString stripPort(QString addr)
{
    // "192.0.2.20:22" or "[fe80::1%eth0]:22"
    const int colon = addr.lastIndexOf(':');
    if (colon > 0)
        addr.truncate(colon);
    if (addr.startsWith('[') && addr.endsWith(']'))
        addr = addr.mid(1, addr.size() - 2);
    const int pct = addr.indexOf('%');
    if (pct > 0)
        addr.truncate(pct);
    return addr;
}

} // namespace

SystemInfo::SystemInfo(QObject *parent) : QObject(parent)
{
    refresh();
    m_sampleTimer.setInterval(1000);
    connect(&m_sampleTimer, &QTimer::timeout, this, &SystemInfo::sample);
    m_sampleTimer.start();
    sample();
}

void SystemInfo::sample()
{
    const QString root = qEnvironmentVariable("DLSIM_SYSFS_NET", QStringLiteral("/sys/class/net"));
    const double dt = m_sampleClock.isValid() ? double(m_sampleClock.restart()) : 0;
    if (!m_sampleClock.isValid())
        m_sampleClock.start();
    QHash<QString, int> carriers;
    QVariantMap live;
    for (const QString &n : m_names) {
        const dlsim::NicSample s = dlsim::readNic(root, n);
        const auto prev = m_last.constFind(n);
        const bool have = prev != m_last.cend() && dt > 0;
        live.insert(n, QVariantMap{{"carrier", s.carrier},
                                   {"speedMbps", s.speedMbps},
                                   {"rxKbps", have ? dlsim::rateKbps(prev->rxBytes, s.rxBytes, dt) : 0.0},
                                   {"txKbps", have ? dlsim::rateKbps(prev->txBytes, s.txBytes, dt) : 0.0},
                                   {"physical", s.physical}});
        m_last.insert(n, s);
        carriers.insert(n, s.carrier);
    }
    m_live = live;
    Q_EMIT liveChanged();
    if (m_unplug.state() == dlsim::UnplugDetector::State::Waiting
        && m_unplug.update(carriers) != dlsim::UnplugDetector::State::Waiting)
        Q_EMIT identifyChanged();
}

QString SystemInfo::identifyState() const
{
    switch (m_unplug.state()) {
    case dlsim::UnplugDetector::State::Idle: return QStringLiteral("idle");
    case dlsim::UnplugDetector::State::Waiting: return QStringLiteral("waiting");
    case dlsim::UnplugDetector::State::Found: return QStringLiteral("found");
    case dlsim::UnplugDetector::State::Ambiguous: return QStringLiteral("ambiguous");
    }
    return {};
}

void SystemInfo::startIdentify(const QStringList &candidates)
{
    QHash<QString, int> carriers;
    for (auto it = m_live.cbegin(); it != m_live.cend(); ++it)
        if (candidates.contains(it.key()))
            carriers.insert(it.key(), it.value().toMap().value("carrier").toInt());
    m_unplug.start(carriers);
    Q_EMIT identifyChanged();
}

void SystemInfo::cancelIdentify()
{
    m_unplug.cancel();
    Q_EMIT identifyChanged();
}

void SystemInfo::refresh()
{
    m_interfaces.clear();
    m_names.clear();
    m_defaultRoute.clear();
    m_ssh.clear();
    m_addresses.clear();
    m_nmManaged.clear();

    // nmcli -t: "enp1s0:connected", "enp2s0:unmanaged". Missing nmcli = no NetworkManager.
    for (const QString &line : QString::fromUtf8(runTool("nmcli", {"-t", "-f", "DEVICE,STATE", "device", "status"}))
                                   .split('\n', Qt::SkipEmptyParts)) {
        const QString dev = line.section(':', 0, 0), state = line.section(':', 1);
        if (!dev.isEmpty() && state != "unmanaged")
            m_nmManaged << dev;
    }

    // Local addresses that carry an SSH session: from our own environment and
    // from established connections to a local sshd.
    QStringList sshLocalAddrs;
    const QStringList conn = qEnvironmentVariable("SSH_CONNECTION").split(' ', Qt::SkipEmptyParts);
    if (conn.size() == 4)
        sshLocalAddrs << conn[2];
    const QString ssOut = QString::fromUtf8(runTool("ss", {"-t", "-n", "-H", "state", "established", "sport", "=", ":22"}));
    for (const QString &line : ssOut.split('\n', Qt::SkipEmptyParts)) {
        const QStringList t = line.split(' ', Qt::SkipEmptyParts);
        if (t.size() >= 4)
            sshLocalAddrs << stripPort(t[2]);
    }

    const QJsonArray routes = QJsonDocument::fromJson(runTool("ip", {"-j", "route", "show", "default"})).array();
    for (const auto &r : routes)
        m_defaultRoute << r.toObject().value("dev").toString();
    m_defaultRoute.removeDuplicates();

    const QJsonArray links = QJsonDocument::fromJson(runTool("ip", {"-j", "-d", "addr", "show"})).array();
    for (const auto &v : links) {
        const QJsonObject l = v.toObject();
        const QString name = l.value("ifname").toString();
        if (name.isEmpty())
            continue;
        QStringList addrs;
        for (const auto &a : l.value("addr_info").toArray()) {
            const QJsonObject ai = a.toObject();
            addrs << QStringLiteral("%1/%2").arg(ai.value("local").toString()).arg(ai.value("prefixlen").toInt());
            if (sshLocalAddrs.contains(ai.value("local").toString()))
                m_ssh << name;
        }
        m_addresses.insert(name, addrs);
        const QString kind = l.value("linkinfo").toObject().value("info_kind").toString();
        m_names << name;
        m_interfaces << QVariantMap{
            {"name", name},
            {"state", l.value("operstate").toString()},
            {"mac", l.value("address").toString()},
            {"master", l.value("master").toString()},
            {"kind", kind.isEmpty() ? QStringLiteral("ether") : kind},
            {"addresses", addrs.join(", ")},
            {"defaultRoute", m_defaultRoute.contains(name)},
            {"usable", name != "lo" && name != dlsim::kBridgeName && name != dlsim::kIfbName},
        };
    }
    m_ssh.removeDuplicates();
    // Fix up flags that depend on lists built during the loop.
    for (auto &v : m_interfaces) {
        QVariantMap m = v.toMap();
        m["ssh"] = m_ssh.contains(m["name"].toString());
        v = m;
    }
    Q_EMIT changed();
}

QStringList SystemInfo::warningsFor(const QString &mode, const QString &ifaceA, const QString &ifaceB) const
{
    QStringList w;
    QStringList ifaces{ifaceA};
    if (mode == "bridge" && ifaceB != ifaceA)
        ifaces << ifaceB;
    for (const QString &i : ifaces) {
        if (i.isEmpty())
            continue;
        if (!m_names.contains(i)) {
            w << QStringLiteral("%1 does not exist.").arg(i);
            continue;
        }
        const bool route = m_defaultRoute.contains(i);
        const bool ssh = m_ssh.contains(i);
        if (mode == "bridge") {
            if (route || ssh)
                w << QStringLiteral("%1 carries %2. Adding it to %3 takes its IP configuration out of use: "
                                    "that connectivity will break.")
                         .arg(i, route && ssh ? "the default route and an SSH session" : route ? "the default route" : "an SSH session",
                              dlsim::kBridgeName);
            else if (!m_addresses.value(i).isEmpty())
                w << QStringLiteral("%1 has addresses (%2) that stop working while it is a bridge port.")
                         .arg(i, m_addresses.value(i).join(", "));
            if (m_nmManaged.contains(i))
                w << QStringLiteral("%1 is managed by NetworkManager, which may undo the bridge. "
                                    "Stop managing it while testing: nmcli device set %1 managed no").arg(i);
        } else {
            if (route)
                w << QStringLiteral("%1 carries the default route. With 'Other' traffic impaired, everything on "
                                    "this PC is affected, and CUT LINK cuts it.").arg(i);
            if (ssh)
                w << QStringLiteral("%1 carries an SSH session. Keep 'Other' excluded (or add an excluded class for "
                                    "TCP 22) or CUT LINK will cut your session.").arg(i);
        }
    }
    return w;
}
