#include "TcCommandBuilder.h"

#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace dlsim {

namespace {

constexpr int kExcludedBand = 0;
constexpr int kLinkBand = 1;
constexpr int kExcludedHandle = 0x10;
constexpr int kLinkHandle = 0x20;
constexpr int kCustomHandleBase = 0x30;
constexpr int kFilterPrefBase = 10;
constexpr quint32 kPfifoLimit = 1000;

QString pct(double v) { return TcCommandBuilder::formatNumber(v) + '%'; }
QString ms(double v) { return TcCommandBuilder::formatNumber(v) + "ms"; }

Command tc(QStringList args, bool ignoreError = false) { return {"tc", std::move(args), ignoreError}; }
Command ip(QStringList args, bool ignoreError = false) { return {"ip", std::move(args), ignoreError}; }

QString shellQuote(const QString &s)
{
    static const QString safe = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.:/%=@,+");
    if (!s.isEmpty() && std::all_of(s.begin(), s.end(), [](QChar c) { return safe.contains(c); }))
        return s;
    QString q = s;
    q.replace('\'', "'\\''");
    return '\'' + q + '\'';
}

Command rootQdisc(const QString &verb, const DeviceTree &t)
{
    QStringList a{"qdisc", verb, "dev", t.dev, "root", "handle", kRootHandle, "prio",
                  "bands", QString::number(t.bands), "priomap"};
    a << TcCommandBuilder::priomap(t.defaultBand);
    return tc(a);
}

Command leafQdisc(const QString &verb, const QString &dev, const Leaf &l, bool explicitZeros)
{
    QStringList a{"qdisc", verb, "dev", dev, "parent", TcCommandBuilder::classId(l.band),
                  "handle", TcCommandBuilder::handle(l.handleMajor)};
    if (l.netem)
        a << TcCommandBuilder::netemArgs(l.profile, explicitZeros);
    else
        a << "pfifo" << "limit" << QString::number(kPfifoLimit);
    return tc(a);
}

Command filterAdd(const QString &dev, const Filter &f)
{
    QStringList a{"filter", "add", "dev", dev, "parent", kRootHandle, "protocol", "ip",
                  "pref", QString::number(f.pref), "flower"};
    a << TcCommandBuilder::filterMatchArgs(f);
    a << "classid" << TcCommandBuilder::classId(f.band);
    return tc(a);
}

void addFilters(QList<Filter> &out, const TrafficClass &c, int index, int band)
{
    Filter base;
    base.proto = c.proto;
    base.band = band;
    const int pref = kFilterPrefBase + 2 * index;
    if (c.ip.isEmpty() && c.port == 0) {
        base.pref = pref;
        out << base;
        return;
    }
    // Endpoint semantics: the class matches packets to or from ip:port, so the
    // same class definition works for both directions.
    Filter dst = base;
    dst.pref = pref;
    dst.dstIp = c.ip;
    dst.dstPort = c.port;
    Filter src = base;
    src.pref = pref + 1;
    src.srcIp = c.ip;
    src.srcPort = c.port;
    out << dst << src;
}

} // namespace

QString toString(DiffLevel l)
{
    switch (l) {
    case DiffLevel::None: return QStringLiteral("none");
    case DiffLevel::Change: return QStringLiteral("change");
    case DiffLevel::Filters: return QStringLiteral("filters");
    case DiffLevel::Rebuild: return QStringLiteral("rebuild");
    }
    return {};
}

QString Command::toString() const
{
    QStringList parts{program};
    for (const auto &a : args)
        parts << shellQuote(a);
    return parts.join(' ');
}

bool Plan::sameStructure(const Plan &o) const
{
    if (mode != o.mode || ingressDev != o.ingressDev || trees.size() != o.trees.size())
        return false;
    for (int i = 0; i < trees.size(); ++i) {
        const auto &a = trees[i];
        const auto &b = o.trees[i];
        if (a.dev != b.dev || a.bands != b.bands || a.leaves.size() != b.leaves.size())
            return false;
        for (int j = 0; j < a.leaves.size(); ++j) {
            const auto &la = a.leaves[j];
            const auto &lb = b.leaves[j];
            if (la.band != lb.band || la.handleMajor != lb.handleMajor || la.netem != lb.netem)
                return false;
        }
    }
    return true;
}

namespace TcCommandBuilder {

QString formatNumber(double v)
{
    if (!std::isfinite(v))
        v = 0;
    QString s = QString::number(v, 'f', 4);
    if (s.contains('.')) {
        while (s.endsWith('0'))
            s.chop(1);
        if (s.endsWith('.'))
            s.chop(1);
    }
    if (s == "-0")
        s = "0";
    return s;
}

QString handle(int major) { return QString::number(major, 16) + ':'; }
QString classId(int band) { return QStringLiteral("1d:") + QString::number(band + 1, 16); }

quint32 autoLimit(const Profile &p)
{
    // Packets in flight during the delay at the configured rate (1 Gbit/s when
    // unlimited), assuming 1500-byte packets, with 50% headroom.
    const bool limited = p.rateKbit > 0;
    const double rateBps = limited ? double(p.rateKbit) * 1000.0 : 1e9;
    const double seconds = (p.delayMs + 2 * p.jitterMs) / 1000.0;
    const double bdpPkts = rateBps * seconds / (8.0 * 1500.0);
    const double floor = limited ? 100 : 1000; // netem default is 1000
    return quint32(std::clamp(std::ceil(bdpPkts * 1.5), floor, 1'000'000.0));
}

quint32 effectiveLimit(const Profile &p) { return p.limitPkts ? p.limitPkts : autoLimit(p); }

QStringList netemArgs(const Profile &p, bool explicitZeros)
{
    QStringList a{"netem", "limit", QString::number(effectiveLimit(p))};

    if (p.delayMs > 0 || p.jitterMs > 0) {
        a << "delay" << ms(p.delayMs);
        if (p.jitterMs > 0) {
            a << ms(p.jitterMs);
            if (p.delayCorrPct > 0)
                a << pct(p.delayCorrPct);
            if (p.distribution != Distribution::Uniform)
                a << "distribution" << toString(p.distribution);
        }
    }

    if (p.lossModel == LossModel::Random) {
        if (p.lossPct > 0) {
            a << "loss" << "random" << pct(p.lossPct);
            if (p.lossCorrPct > 0)
                a << pct(p.lossCorrPct);
        }
    } else if (p.geP > 0) {
        a << "loss" << "gemodel" << pct(p.geP) << pct(p.geR) << pct(p.geBadLossPct) << pct(p.geGoodLossPct);
    }

    if (p.duplicatePct > 0)
        a << "duplicate" << pct(p.duplicatePct);

    if (p.reorderPct > 0) {
        a << "reorder" << pct(p.reorderPct);
        if (p.reorderCorrPct > 0)
            a << pct(p.reorderCorrPct);
        if (p.reorderGap > 0)
            a << "gap" << QString::number(p.reorderGap);
    } else if (explicitZeros) {
        a << "reorder" << "0%";
    }

    if (p.corruptPct > 0)
        a << "corrupt" << pct(p.corruptPct);
    else if (explicitZeros)
        a << "corrupt" << "0%";

    if (p.rateKbit > 0)
        a << "rate" << QString::number(p.rateKbit) + "kbit";
    else if (explicitZeros)
        a << "rate" << "0bit";

    return a;
}

QStringList filterMatchArgs(const Filter &f)
{
    QStringList a;
    if (f.proto != Proto::Any)
        a << "ip_proto" << toString(f.proto);
    if (!f.srcIp.isEmpty())
        a << "src_ip" << f.srcIp;
    if (!f.dstIp.isEmpty())
        a << "dst_ip" << f.dstIp;
    if (f.srcPort)
        a << "src_port" << QString::number(f.srcPort);
    if (f.dstPort)
        a << "dst_port" << QString::number(f.dstPort);
    return a;
}

QStringList priomap(int defaultBand)
{
    QStringList m;
    for (int i = 0; i < 16; ++i)
        m << QString::number(defaultBand);
    return m;
}

QString deviceFor(const LinkConfig &cfg, Direction dir)
{
    if (cfg.mode == Mode::Bridge)
        return dir == Direction::Uplink ? cfg.airIface : cfg.groundIface;
    return dir == Direction::Uplink ? cfg.hostIface : QString::fromLatin1(kIfbName);
}

Plan buildPlan(const LinkConfig &cfg)
{
    Plan plan;
    plan.mode = cfg.mode;
    if (cfg.mode == Mode::Host)
        plan.ingressDev = cfg.hostIface;

    for (Direction dir : {Direction::Uplink, Direction::Downlink}) {
        DeviceTree t;
        t.dir = dir;
        t.dev = deviceFor(cfg, dir);
        t.defaultBand = cfg.otherMode == ClassMode::Excluded ? kExcludedBand : kLinkBand;

        auto netemLeaf = [&](int band, int handleMajor, const Profile &base) {
            Leaf l;
            l.band = band;
            l.handleMajor = handleMajor;
            l.netem = true;
            l.profile = cfg.cut ? cutProfile(base) : base;
            l.limit = effectiveLimit(l.profile);
            return l;
        };

        Leaf excluded;
        excluded.band = kExcludedBand;
        excluded.handleMajor = kExcludedHandle;
        t.leaves << excluded;
        t.leaves << netemLeaf(kLinkBand, kLinkHandle, dir == Direction::Uplink ? cfg.uplink : cfg.downlink);

        int custom = 0;
        for (int i = 0; i < cfg.classes.size(); ++i) {
            const TrafficClass &c = cfg.classes[i];
            if (!c.enabled)
                continue;
            int band = kLinkBand;
            if (c.mode == ClassMode::Excluded) {
                band = kExcludedBand;
            } else if (c.mode == ClassMode::Custom) {
                band = 2 + custom;
                t.leaves << netemLeaf(band, kCustomHandleBase + custom,
                                      dir == Direction::Uplink ? c.uplink : c.downlink);
                ++custom;
            }
            addFilters(t.filters, c, i, band);
        }
        t.bands = 2 + custom;
        plan.trees << t;
    }
    return plan;
}

CommandList buildAll(const Plan &plan)
{
    CommandList cmds;
    if (plan.mode == Mode::Host) {
        cmds << ip({"link", "add", "name", kIfbName, "type", "ifb"});
        cmds << ip({"link", "set", "dev", kIfbName, "up"});
        cmds << tc({"qdisc", "add", "dev", plan.ingressDev, "handle", "ffff:", "ingress"});
        cmds << tc({"filter", "add", "dev", plan.ingressDev, "parent", "ffff:", "protocol", "all",
                    "pref", QString::number(kIngressFilterPref), "matchall",
                    "action", "mirred", "egress", "redirect", "dev", kIfbName});
    }
    for (const auto &t : plan.trees) {
        cmds << rootQdisc("replace", t);
        for (const auto &l : t.leaves)
            cmds << leafQdisc("add", t.dev, l, false);
        for (const auto &f : t.filters)
            cmds << filterAdd(t.dev, f);
    }
    return cmds;
}

CommandList teardown(const Plan &plan)
{
    CommandList cmds;
    for (const auto &t : plan.trees) {
        if (t.dev == kIfbName)
            continue; // goes away with the device
        cmds << tc({"qdisc", "del", "dev", t.dev, "root"}, true);
    }
    if (plan.mode == Mode::Host) {
        cmds << tc({"qdisc", "del", "dev", plan.ingressDev, "ingress"}, true);
        cmds << ip({"link", "del", "dev", kIfbName}, true);
    }
    return cmds;
}

Diff diff(const std::optional<Plan> &applied, const Plan &desired)
{
    Diff d;
    if (!applied) {
        d.level = DiffLevel::Rebuild;
        d.commands = buildAll(desired);
        return d;
    }
    if (!applied->sameStructure(desired)) {
        d.level = DiffLevel::Rebuild;
        d.commands = teardown(*applied) + buildAll(desired);
        return d;
    }
    auto raise = [&](DiffLevel l) { d.level = std::max(d.level, l); };
    for (int i = 0; i < desired.trees.size(); ++i) {
        const DeviceTree &a = applied->trees[i];
        const DeviceTree &b = desired.trees[i];
        if (a.defaultBand != b.defaultBand) {
            d.commands << rootQdisc("change", b);
            raise(DiffLevel::Filters);
        }
        if (a.filters != b.filters) {
            d.commands << tc({"filter", "del", "dev", b.dev, "parent", kRootHandle}, true);
            for (const auto &f : b.filters)
                d.commands << filterAdd(b.dev, f);
            raise(DiffLevel::Filters);
        }
        for (int j = 0; j < b.leaves.size(); ++j) {
            if (b.leaves[j].netem && a.leaves[j] != b.leaves[j]) {
                d.commands << leafQdisc("change", b.dev, b.leaves[j], true);
                raise(DiffLevel::Change);
            }
        }
    }
    return d;
}

CommandList setupBridge(const QString &groundIface, const QString &airIface)
{
    return {
        ip({"link", "add", "name", kBridgeName, "type", "bridge", "stp_state", "0"}),
        ip({"link", "set", "dev", groundIface, "master", kBridgeName}),
        ip({"link", "set", "dev", airIface, "master", kBridgeName}),
        ip({"link", "set", "dev", groundIface, "up"}),
        ip({"link", "set", "dev", airIface, "up"}),
        ip({"link", "set", "dev", kBridgeName, "up"}),
    };
}

CommandList teardownBridge(const QString &groundIface, const QString &airIface)
{
    CommandList cmds;
    if (!groundIface.isEmpty())
        cmds << ip({"link", "set", "dev", groundIface, "nomaster"}, true);
    if (!airIface.isEmpty())
        cmds << ip({"link", "set", "dev", airIface, "nomaster"}, true);
    cmds << ip({"link", "del", "dev", kBridgeName}, true);
    return cmds;
}

QStringList devicesWithIngress(const QJsonArray &qdiscs)
{
    QStringList devs;
    for (const auto &v : qdiscs) {
        const QJsonObject q = v.toObject();
        if (q.value("kind").toString() == "ingress" && q.value("handle").toString() == "ffff:")
            devs << q.value("dev").toString();
    }
    devs.removeAll(QString());
    devs.removeDuplicates();
    return devs;
}

CommandList leftoverCleanup(const QJsonArray &links, const QJsonArray &qdiscs,
                            const QHash<QString, QJsonArray> &ingressFilters)
{
    QStringList names;
    for (const auto &v : links)
        names << v.toObject().value("ifname").toString();

    CommandList cmds;
    for (const auto &v : qdiscs) {
        const QJsonObject q = v.toObject();
        const QString dev = q.value("dev").toString();
        if (dev.isEmpty() || dev == kIfbName)
            continue;
        if (q.value("root").toBool() && q.value("kind").toString() == "prio"
            && q.value("handle").toString() == kRootHandle)
            cmds << tc({"qdisc", "del", "dev", dev, "root"}, true);
    }
    for (const QString &dev : devicesWithIngress(qdiscs)) {
        if (dev == kIfbName)
            continue;
        const QJsonArray filters = ingressFilters.value(dev);
        const bool ours = std::any_of(filters.begin(), filters.end(), [](const QJsonValue &f) {
            const QJsonObject o = f.toObject();
            return o.value("pref").toInt() == kIngressFilterPref && o.value("kind").toString() == "matchall";
        });
        if (ours)
            cmds << tc({"qdisc", "del", "dev", dev, "ingress"}, true);
    }
    if (names.contains(kIfbName))
        cmds << ip({"link", "del", "dev", kIfbName}, true);
    if (names.contains(kBridgeName))
        cmds << ip({"link", "del", "dev", kBridgeName}, true);
    return cmds;
}

QString render(const CommandList &cmds)
{
    QStringList lines;
    for (const auto &c : cmds)
        lines << c.toString();
    return lines.join('\n');
}

} // namespace TcCommandBuilder
} // namespace dlsim
