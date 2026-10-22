// Pure functions that turn a LinkConfig into `tc` / `ip` argument lists.
// No I/O: the helper executes the result, the GUI shows it as a preview,
// and the unit tests compare it against expected lists.
//
// Tree built on every impaired device:
//
//   root 1d: prio bands N priomap <default band x16>
//     1d:1  -> 10: pfifo           band 0: excluded traffic, passes untouched
//     1d:2  -> 20: netem <link>    band 1: the direction's link profile
//     1d:3  -> 30: netem <custom>  band 2..: one per class with a custom profile
//     filters: flower on parent 1d:, pref 10+2i (dst side) / 11+2i (src side)
//
// Bridge mode: uplink = egress of the air NIC, downlink = egress of the ground NIC.
// Host mode:   uplink = egress of the NIC, downlink = egress of ifb-dlsim, fed by
//              an ingress qdisc + matchall/mirred redirect on the NIC.
#pragma once

#include "Model.h"

#include <QHash>
#include <QJsonArray>

#include <optional>

namespace dlsim {

struct Command {
    QString program; // "tc" or "ip"; the helper maps it to an absolute path
    QStringList args;
    bool ignoreError = false; // used for best-effort deletes

    bool operator==(const Command &) const = default;
    QString toString() const;
};
using CommandList = QList<Command>;

struct Leaf {
    int band = 0;
    int handleMajor = 0; // hex major, e.g. 0x20 -> "20:"
    bool netem = false;
    Profile profile;     // effective profile (cut already applied)
    quint32 limit = 0;   // effective netem limit

    bool operator==(const Leaf &) const = default;
};

struct Filter {
    int pref = 0;
    Proto proto = Proto::Any;
    QString srcIp, dstIp;
    quint16 srcPort = 0, dstPort = 0;
    int band = 0;

    bool operator==(const Filter &) const = default;
};

struct DeviceTree {
    Direction dir = Direction::Uplink;
    QString dev;
    int bands = 2;
    int defaultBand = 1; // where unmatched traffic ("Other") goes
    QList<Leaf> leaves;
    QList<Filter> filters;

    bool operator==(const DeviceTree &) const = default;
};

struct Plan {
    Mode mode = Mode::Bridge;
    QString ingressDev; // host mode: NIC whose ingress is redirected to the IFB
    QList<DeviceTree> trees; // [0] uplink, [1] downlink

    bool operator==(const Plan &) const = default;
    bool sameStructure(const Plan &o) const;
};

enum class DiffLevel { None, Change, Filters, Rebuild };
QString toString(DiffLevel);

struct Diff {
    DiffLevel level = DiffLevel::None;
    CommandList commands;
};

namespace TcCommandBuilder {

QString formatNumber(double v);     // "12.5", no exponent, trailing zeros trimmed
QString handle(int major);          // 0x20 -> "20:"
QString classId(int band);          // band 1 -> "1d:2"

quint32 autoLimit(const Profile &p);
quint32 effectiveLimit(const Profile &p);

// Arguments after `... handle X:` for a netem leaf, starting with "netem".
// explicitZeros adds `reorder 0% corrupt 0% rate 0bit` when those are off:
// `tc qdisc change` keeps old reorder/corrupt/rate values otherwise.
QStringList netemArgs(const Profile &p, bool explicitZeros = false);
QStringList filterMatchArgs(const Filter &f);
QStringList priomap(int defaultBand);

Plan buildPlan(const LinkConfig &cfg);
QString deviceFor(const LinkConfig &cfg, Direction dir);

CommandList buildAll(const Plan &plan);
CommandList teardown(const Plan &plan);
Diff diff(const std::optional<Plan> &applied, const Plan &desired);

CommandList setupBridge(const QString &groundIface, const QString &airIface);
CommandList teardownBridge(const QString &groundIface, const QString &airIface);

// Commands that remove DLSim objects found on the system.
//   links:   `ip -j link show`
//   qdiscs:  `tc -j qdisc show`
//   ingressFilters: dev -> `tc -j filter show dev <dev> parent ffff:` for each dev with an ingress qdisc
CommandList leftoverCleanup(const QJsonArray &links, const QJsonArray &qdiscs,
                            const QHash<QString, QJsonArray> &ingressFilters);
QStringList devicesWithIngress(const QJsonArray &qdiscs);

QString render(const CommandList &cmds);

} // namespace TcCommandBuilder
} // namespace dlsim
