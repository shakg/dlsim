// Unit tests for TcCommandBuilder. Pure functions only: no root needed.
#include "Model.h"
#include "TcCommandBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace dlsim;
namespace B = dlsim::TcCommandBuilder;

namespace {

QStringList words(const char *s) { return QString::fromLatin1(s).split(' ', Qt::SkipEmptyParts); }

LinkConfig bridgeConfig()
{
    LinkConfig c;
    c.mode = Mode::Bridge;
    c.groundIface = "enp1s0";
    c.airIface = "enp2s0";
    c.uplink.delayMs = 20;
    c.downlink.delayMs = 30;
    return c;
}

LinkConfig hostConfig()
{
    LinkConfig c;
    c.mode = Mode::Host;
    c.hostIface = "eth0";
    c.otherMode = ClassMode::Excluded;
    c.uplink.delayMs = 20;
    c.downlink.delayMs = 30;
    return c;
}

TrafficClass mavlinkClass(ClassMode mode = ClassMode::Link)
{
    TrafficClass t;
    t.name = "MAVLink TCP 4555";
    t.proto = Proto::Tcp;
    t.ip = "192.0.2.10";
    t.port = 4555;
    t.mode = mode;
    return t;
}

QStringList rendered(const CommandList &cmds)
{
    QStringList out;
    for (const auto &c : cmds)
        out << c.toString();
    return out;
}

} // namespace

class TestTcCommandBuilder : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void formatNumber_data();
    void formatNumber();
    void handles();
    void netemEmpty();
    void netemSingleOptions_data();
    void netemSingleOptions();
    void netemAllCombinations();
    void netemExplicitZeros();
    void autoLimit();
    void bridgeBuildAll();
    void hostBuildAll();
    void classesAndFilters();
    void filterMatchVariants_data();
    void filterMatchVariants();
    void otherExcludedPriomap();
    void cutAppliesFullLoss();
    void diffNone();
    void diffChange();
    void diffCutIsChange();
    void diffFilters();
    void diffDefaultBand();
    void diffRebuildOnStructure();
    void diffRebuildOnModeSwitch();
    void diffFromNothing();
    void teardownCommands();
    void bridgeCommands();
    void leftovers();
    void validation_data();
    void validation();
    void jsonRoundTrip();
    void interpolation();
    void shellRendering();
};

void TestTcCommandBuilder::formatNumber_data()
{
    QTest::addColumn<double>("in");
    QTest::addColumn<QString>("out");
    QTest::newRow("int") << 20.0 << "20";
    QTest::newRow("frac") << 12.5 << "12.5";
    QTest::newRow("small") << 0.001 << "0.001";
    QTest::newRow("zero") << 0.0 << "0";
    QTest::newRow("round") << 1.23456789 << "1.2346";
    QTest::newRow("big") << 1e7 << "10000000";
}

void TestTcCommandBuilder::formatNumber()
{
    QFETCH(double, in);
    QFETCH(QString, out);
    QCOMPARE(B::formatNumber(in), out);
}

void TestTcCommandBuilder::handles()
{
    QCOMPARE(B::handle(0x20), QString("20:"));
    QCOMPARE(B::handle(0x3a), QString("3a:"));
    QCOMPARE(B::classId(0), QString("1d:1"));
    QCOMPARE(B::classId(1), QString("1d:2"));
    QCOMPARE(B::classId(9), QString("1d:a"));
    QCOMPARE(B::priomap(1), QStringList(16, "1"));
}

void TestTcCommandBuilder::netemEmpty()
{
    QCOMPARE(B::netemArgs(Profile{}), words("netem limit 1000"));
}

void TestTcCommandBuilder::netemSingleOptions_data()
{
    QTest::addColumn<QJsonObject>("profile");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("delay") << QJsonObject{{"delayMs", 100}} << words("netem limit 12500 delay 100ms");
    QTest::newRow("delay+jitter normal")
        << QJsonObject{{"delayMs", 100}, {"jitterMs", 10}}
        << words("netem limit 15000 delay 100ms 10ms distribution normal");
    QTest::newRow("delay+jitter+corr pareto")
        << QJsonObject{{"delayMs", 100}, {"jitterMs", 10}, {"delayCorrPct", 25}, {"distribution", "pareto"}}
        << words("netem limit 15000 delay 100ms 10ms 25% distribution pareto");
    QTest::newRow("paretonormal")
        << QJsonObject{{"delayMs", 50}, {"jitterMs", 5}, {"distribution", "paretonormal"}}
        << words("netem limit 7500 delay 50ms 5ms distribution paretonormal");
    QTest::newRow("uniform omits distribution")
        << QJsonObject{{"delayMs", 50}, {"jitterMs", 5}, {"distribution", "uniform"}}
        << words("netem limit 7500 delay 50ms 5ms");
    QTest::newRow("distribution ignored without jitter")
        << QJsonObject{{"delayMs", 50}, {"distribution", "pareto"}, {"delayCorrPct", 30}}
        << words("netem limit 6250 delay 50ms");
    QTest::newRow("loss random") << QJsonObject{{"lossPct", 2.5}} << words("netem limit 1000 loss random 2.5%");
    QTest::newRow("loss random corr")
        << QJsonObject{{"lossPct", 2.5}, {"lossCorrPct", 30}} << words("netem limit 1000 loss random 2.5% 30%");
    QTest::newRow("loss gemodel")
        << QJsonObject{{"lossModel", "gemodel"}, {"geP", 1}, {"geR", 10}, {"geBadLossPct", 70}, {"geGoodLossPct", 0.1}}
        << words("netem limit 1000 loss gemodel 1% 10% 70% 0.1%");
    QTest::newRow("gemodel p=0 means no loss")
        << QJsonObject{{"lossModel", "gemodel"}, {"geP", 0}, {"lossPct", 50}} << words("netem limit 1000");
    QTest::newRow("duplicate") << QJsonObject{{"duplicatePct", 1}} << words("netem limit 1000 duplicate 1%");
    QTest::newRow("reorder")
        << QJsonObject{{"delayMs", 10}, {"reorderPct", 25}} << words("netem limit 1250 delay 10ms reorder 25%");
    QTest::newRow("reorder corr gap")
        << QJsonObject{{"delayMs", 10}, {"reorderPct", 25}, {"reorderCorrPct", 50}, {"reorderGap", 5}}
        << words("netem limit 1250 delay 10ms reorder 25% 50% gap 5");
    QTest::newRow("corrupt") << QJsonObject{{"corruptPct", 0.1}} << words("netem limit 1000 corrupt 0.1%");
    QTest::newRow("rate") << QJsonObject{{"rateKbit", 2000}} << words("netem limit 100 rate 2000kbit");
    QTest::newRow("rate+delay auto limit")
        << QJsonObject{{"rateKbit", 100000}, {"delayMs", 200}} << words("netem limit 2500 delay 200ms rate 100000kbit");
    QTest::newRow("explicit limit")
        << QJsonObject{{"rateKbit", 2000}, {"limitPkts", 300}} << words("netem limit 300 rate 2000kbit");
}

void TestTcCommandBuilder::netemSingleOptions()
{
    QFETCH(QJsonObject, profile);
    QFETCH(QStringList, expected);
    QCOMPARE(B::netemArgs(Profile::fromJson(profile)), expected);
}

// Every combination of option groups: the output must be the concatenation of
// each enabled group's fragment, in tc's canonical order.
void TestTcCommandBuilder::netemAllCombinations()
{
    struct Group {
        const char *name;
        void (*apply)(Profile &);
        QStringList fragment;
    };
    const QList<Group> delayGroups{
        {"none", [](Profile &) {}, {}},
        {"delay", [](Profile &p) { p.delayMs = 40; }, words("delay 40ms")},
        {"jitter", [](Profile &p) { p.delayMs = 40; p.jitterMs = 8; p.distribution = Distribution::Uniform; },
         words("delay 40ms 8ms")},
        {"jitter+corr+normal",
         [](Profile &p) { p.delayMs = 40; p.jitterMs = 8; p.delayCorrPct = 20; p.distribution = Distribution::Normal; },
         words("delay 40ms 8ms 20% distribution normal")},
        {"jitter+pareto", [](Profile &p) { p.delayMs = 40; p.jitterMs = 8; p.distribution = Distribution::Pareto; },
         words("delay 40ms 8ms distribution pareto")},
        {"jitter+paretonormal",
         [](Profile &p) { p.delayMs = 40; p.jitterMs = 8; p.distribution = Distribution::ParetoNormal; },
         words("delay 40ms 8ms distribution paretonormal")},
    };
    const QList<Group> lossGroups{
        {"none", [](Profile &) {}, {}},
        {"random", [](Profile &p) { p.lossPct = 3; }, words("loss random 3%")},
        {"random+corr", [](Profile &p) { p.lossPct = 3; p.lossCorrPct = 25; }, words("loss random 3% 25%")},
        {"gemodel",
         [](Profile &p) { p.lossModel = LossModel::GilbertElliott; p.geP = 2; p.geR = 20; p.geBadLossPct = 80; p.geGoodLossPct = 1; },
         words("loss gemodel 2% 20% 80% 1%")},
    };
    const QList<Group> dupGroups{
        {"none", [](Profile &) {}, {}},
        {"dup", [](Profile &p) { p.duplicatePct = 0.5; }, words("duplicate 0.5%")},
    };
    const QList<Group> reorderGroups{
        {"none", [](Profile &) {}, {}},
        {"reorder", [](Profile &p) { p.reorderPct = 10; }, words("reorder 10%")},
        {"reorder+corr+gap", [](Profile &p) { p.reorderPct = 10; p.reorderCorrPct = 40; p.reorderGap = 3; },
         words("reorder 10% 40% gap 3")},
    };
    const QList<Group> corruptGroups{
        {"none", [](Profile &) {}, {}},
        {"corrupt", [](Profile &p) { p.corruptPct = 0.2; }, words("corrupt 0.2%")},
    };
    const QList<Group> rateGroups{
        {"none", [](Profile &) {}, {}},
        {"rate", [](Profile &p) { p.rateKbit = 512; p.limitPkts = 64; }, words("rate 512kbit")},
    };

    int count = 0;
    for (const auto &dg : delayGroups)
        for (const auto &lg : lossGroups)
            for (const auto &du : dupGroups)
                for (const auto &rg : reorderGroups)
                    for (const auto &cg : corruptGroups)
                        for (const auto &ra : rateGroups)
                            for (bool zeros : {false, true}) {
                                Profile p;
                                p.limitPkts = 777;
                                for (const Group *g : {&dg, &lg, &du, &rg, &cg, &ra})
                                    g->apply(p);
                                QStringList expected{"netem", "limit", QString::number(p.limitPkts)};
                                expected << dg.fragment << lg.fragment << du.fragment;
                                expected << (rg.fragment.isEmpty() && zeros ? words("reorder 0%") : rg.fragment);
                                expected << (cg.fragment.isEmpty() && zeros ? words("corrupt 0%") : cg.fragment);
                                expected << (ra.fragment.isEmpty() && zeros ? words("rate 0bit") : ra.fragment);
                                const QStringList got = B::netemArgs(p, zeros);
                                if (got != expected) {
                                    qWarning() << dg.name << lg.name << du.name << rg.name << cg.name << ra.name << zeros;
                                    QCOMPARE(got, expected);
                                }
                                ++count;
                            }
    QCOMPARE(count, 6 * 4 * 2 * 3 * 2 * 2 * 2);
}

void TestTcCommandBuilder::netemExplicitZeros()
{
    QCOMPARE(B::netemArgs(Profile{}, true), words("netem limit 1000 reorder 0% corrupt 0% rate 0bit"));
}

void TestTcCommandBuilder::autoLimit()
{
    Profile p;
    QCOMPARE(B::autoLimit(p), 1000u); // netem default floor when unlimited
    p.delayMs = 500; // 1 Gbit/s * 0.5 s / 12000 bit = 41667 pkts * 1.5
    QCOMPARE(B::autoLimit(p), 62500u);
    p = {};
    p.rateKbit = 2000;
    p.delayMs = 100; // 2 Mbit/s * 0.1 s / 12000 = 16.7 * 1.5 = 25 -> floor 100
    QCOMPARE(B::autoLimit(p), 100u);
    p.delayMs = 1000; // 166.7 * 1.5 = 250
    QCOMPARE(B::autoLimit(p), 250u);
    p.jitterMs = 250; // (1000 + 500) ms -> 375
    QCOMPARE(B::autoLimit(p), 375u);
    p.limitPkts = 42;
    QCOMPARE(B::effectiveLimit(p), 42u);
    p = {};
    p.delayMs = 60000;
    QCOMPARE(B::autoLimit(p), 1000000u); // capped
}

void TestTcCommandBuilder::bridgeBuildAll()
{
    const Plan plan = B::buildPlan(bridgeConfig());
    QCOMPARE(plan.trees.size(), 2);
    QCOMPARE(int(plan.trees[0].dir), int(Direction::Uplink));
    QCOMPARE(plan.trees[0].dev, QString("enp2s0")); // uplink leaves via the air NIC
    QCOMPARE(plan.trees[1].dev, QString("enp1s0"));
    const QStringList expected{
        "tc qdisc replace dev enp2s0 root handle 1d: prio bands 2 priomap 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1",
        "tc qdisc add dev enp2s0 parent 1d:1 handle 10: pfifo limit 1000",
        "tc qdisc add dev enp2s0 parent 1d:2 handle 20: netem limit 2500 delay 20ms",
        "tc qdisc replace dev enp1s0 root handle 1d: prio bands 2 priomap 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1",
        "tc qdisc add dev enp1s0 parent 1d:1 handle 10: pfifo limit 1000",
        "tc qdisc add dev enp1s0 parent 1d:2 handle 20: netem limit 3750 delay 30ms",
    };
    QCOMPARE(rendered(B::buildAll(plan)), expected);
}

void TestTcCommandBuilder::hostBuildAll()
{
    LinkConfig c = hostConfig();
    c.classes << mavlinkClass();
    const QStringList expected{
        "ip link add name ifb-dlsim type ifb",
        "ip link set dev ifb-dlsim up",
        "tc qdisc add dev eth0 handle ffff: ingress",
        "tc filter add dev eth0 parent ffff: protocol all pref 4919 matchall action mirred egress redirect dev ifb-dlsim",
        "tc qdisc replace dev eth0 root handle 1d: prio bands 2 priomap 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "tc qdisc add dev eth0 parent 1d:1 handle 10: pfifo limit 1000",
        "tc qdisc add dev eth0 parent 1d:2 handle 20: netem limit 2500 delay 20ms",
        "tc filter add dev eth0 parent 1d: protocol ip pref 10 flower ip_proto tcp dst_ip 192.0.2.10 dst_port 4555 classid 1d:2",
        "tc filter add dev eth0 parent 1d: protocol ip pref 11 flower ip_proto tcp src_ip 192.0.2.10 src_port 4555 classid 1d:2",
        "tc qdisc replace dev ifb-dlsim root handle 1d: prio bands 2 priomap 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "tc qdisc add dev ifb-dlsim parent 1d:1 handle 10: pfifo limit 1000",
        "tc qdisc add dev ifb-dlsim parent 1d:2 handle 20: netem limit 3750 delay 30ms",
        "tc filter add dev ifb-dlsim parent 1d: protocol ip pref 10 flower ip_proto tcp dst_ip 192.0.2.10 dst_port 4555 classid 1d:2",
        "tc filter add dev ifb-dlsim parent 1d: protocol ip pref 11 flower ip_proto tcp src_ip 192.0.2.10 src_port 4555 classid 1d:2",
    };
    QCOMPARE(rendered(B::buildAll(B::buildPlan(c))), expected);
}

void TestTcCommandBuilder::classesAndFilters()
{
    LinkConfig c = bridgeConfig();
    TrafficClass mav = mavlinkClass(ClassMode::Custom);
    mav.uplink.delayMs = 5;
    mav.downlink.lossPct = 1;
    TrafficClass video;
    video.name = "Video UDP 5600";
    video.proto = Proto::Udp;
    video.port = 5600;
    video.mode = ClassMode::Link;
    TrafficClass ssh;
    ssh.name = "SSH";
    ssh.proto = Proto::Tcp;
    ssh.port = 22;
    ssh.mode = ClassMode::Excluded;
    TrafficClass disabled = mavlinkClass(ClassMode::Custom);
    disabled.enabled = false;
    TrafficClass icmp;
    icmp.name = "ICMP";
    icmp.proto = Proto::Icmp;
    icmp.mode = ClassMode::Custom;
    icmp.uplink.delayMs = 1;
    c.classes = {mav, video, ssh, disabled, icmp};

    const Plan plan = B::buildPlan(c);
    const DeviceTree &up = plan.trees[0];
    QCOMPARE(up.bands, 4);
    QCOMPARE(up.leaves.size(), 4);
    QCOMPARE(up.leaves[2].handleMajor, 0x30);
    QCOMPARE(up.leaves[2].profile.delayMs, 5.0);
    QCOMPARE(up.leaves[3].handleMajor, 0x31);
    QCOMPARE(up.leaves[3].band, 3);
    QCOMPARE(plan.trees[1].leaves[2].profile.lossPct, 1.0);

    const QStringList expected{
        "tc qdisc replace dev enp2s0 root handle 1d: prio bands 4 priomap 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1",
        "tc qdisc add dev enp2s0 parent 1d:1 handle 10: pfifo limit 1000",
        "tc qdisc add dev enp2s0 parent 1d:2 handle 20: netem limit 2500 delay 20ms",
        "tc qdisc add dev enp2s0 parent 1d:3 handle 30: netem limit 1000 delay 5ms",
        "tc qdisc add dev enp2s0 parent 1d:4 handle 31: netem limit 1000 delay 1ms",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 10 flower ip_proto tcp dst_ip 192.0.2.10 dst_port 4555 classid 1d:3",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 11 flower ip_proto tcp src_ip 192.0.2.10 src_port 4555 classid 1d:3",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 12 flower ip_proto udp dst_port 5600 classid 1d:2",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 13 flower ip_proto udp src_port 5600 classid 1d:2",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 14 flower ip_proto tcp dst_port 22 classid 1d:1",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 15 flower ip_proto tcp src_port 22 classid 1d:1",
        "tc filter add dev enp2s0 parent 1d: protocol ip pref 18 flower ip_proto icmp classid 1d:4",
    };
    QCOMPARE(rendered(B::buildAll(plan)).mid(0, expected.size()), expected);
}

void TestTcCommandBuilder::filterMatchVariants_data()
{
    QTest::addColumn<int>("proto");
    QTest::addColumn<QString>("ip");
    QTest::addColumn<int>("port");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("any, no match") << int(Proto::Any) << "" << 0 << QStringList{"pref 10 flower classid 1d:2"};
    QTest::newRow("any ip") << int(Proto::Any) << "10.0.0.1" << 0
                            << QStringList{"pref 10 flower dst_ip 10.0.0.1 classid 1d:2",
                                           "pref 11 flower src_ip 10.0.0.1 classid 1d:2"};
    QTest::newRow("udp port") << int(Proto::Udp) << "" << 14550
                              << QStringList{"pref 10 flower ip_proto udp dst_port 14550 classid 1d:2",
                                             "pref 11 flower ip_proto udp src_port 14550 classid 1d:2"};
    QTest::newRow("tcp only") << int(Proto::Tcp) << "" << 0
                              << QStringList{"pref 10 flower ip_proto tcp classid 1d:2"};
    QTest::newRow("tcp ip port") << int(Proto::Tcp) << "192.0.2.10" << 4555
                                 << QStringList{"pref 10 flower ip_proto tcp dst_ip 192.0.2.10 dst_port 4555 classid 1d:2",
                                                "pref 11 flower ip_proto tcp src_ip 192.0.2.10 src_port 4555 classid 1d:2"};
    QTest::newRow("icmp ip") << int(Proto::Icmp) << "192.0.2.10" << 0
                             << QStringList{"pref 10 flower ip_proto icmp dst_ip 192.0.2.10 classid 1d:2",
                                            "pref 11 flower ip_proto icmp src_ip 192.0.2.10 classid 1d:2"};
}

void TestTcCommandBuilder::filterMatchVariants()
{
    QFETCH(int, proto);
    QFETCH(QString, ip);
    QFETCH(int, port);
    QFETCH(QStringList, expected);
    LinkConfig c = bridgeConfig();
    TrafficClass t;
    t.name = "x";
    t.proto = Proto(proto);
    t.ip = ip;
    t.port = quint16(port);
    c.classes << t;
    QVERIFY(c.validate().isEmpty());
    const CommandList cmds = B::buildAll(B::buildPlan(c));
    QStringList filters;
    for (const auto &cmd : cmds)
        if (cmd.args.value(0) == "filter" && cmd.args.contains("enp2s0"))
            filters << cmd.toString().section(" protocol ip ", 1);
    QCOMPARE(filters, expected);
}

void TestTcCommandBuilder::otherExcludedPriomap()
{
    LinkConfig c = bridgeConfig();
    c.otherMode = ClassMode::Excluded;
    const Plan plan = B::buildPlan(c);
    QCOMPARE(plan.trees[0].defaultBand, 0);
    QCOMPARE(B::buildAll(plan).first().args.mid(11), QStringList(16, "0"));
}

void TestTcCommandBuilder::cutAppliesFullLoss()
{
    LinkConfig c = bridgeConfig();
    c.uplink.lossModel = LossModel::GilbertElliott;
    c.uplink.geP = 5;
    c.uplink.rateKbit = 1000;
    c.classes << mavlinkClass(ClassMode::Custom);
    c.cut = true;
    const Plan plan = B::buildPlan(c);
    for (const auto &t : plan.trees)
        for (const auto &l : t.leaves)
            if (l.netem)
                QVERIFY(B::netemArgs(l.profile).join(' ').contains("loss random 100%"));
    QCOMPARE(B::netemArgs(plan.trees[0].leaves[1].profile),
             words("netem limit 100 delay 20ms loss random 100% rate 1000kbit"));
    // Excluded band keeps passing traffic during a cut.
    QVERIFY(!plan.trees[0].leaves[0].netem);
}

void TestTcCommandBuilder::diffNone()
{
    const Plan p = B::buildPlan(bridgeConfig());
    const Diff d = B::diff(p, p);
    QCOMPARE(int(d.level), int(DiffLevel::None));
    QVERIFY(d.commands.isEmpty());
}

void TestTcCommandBuilder::diffChange()
{
    LinkConfig a = bridgeConfig();
    LinkConfig b = a;
    b.downlink.delayMs = 80;
    b.downlink.lossPct = 2;
    const Diff d = B::diff(B::buildPlan(a), B::buildPlan(b));
    QCOMPARE(int(d.level), int(DiffLevel::Change));
    QCOMPARE(rendered(d.commands),
             QStringList{"tc qdisc change dev enp1s0 parent 1d:2 handle 20: netem limit 10000 delay 80ms "
                         "loss random 2% reorder 0% corrupt 0% rate 0bit"});
}

void TestTcCommandBuilder::diffCutIsChange()
{
    LinkConfig a = bridgeConfig();
    LinkConfig b = a;
    b.cut = true;
    const Diff d = B::diff(B::buildPlan(a), B::buildPlan(b));
    QCOMPARE(int(d.level), int(DiffLevel::Change));
    QCOMPARE(d.commands.size(), 2);
    for (const auto &c : d.commands) {
        QCOMPARE(c.args.value(1), QString("change"));
        QVERIFY(c.args.join(' ').contains("loss random 100%"));
    }
    // And back.
    const Diff r = B::diff(B::buildPlan(b), B::buildPlan(a));
    QCOMPARE(int(r.level), int(DiffLevel::Change));
    QVERIFY(!r.commands[0].args.join(' ').contains("loss"));
}

void TestTcCommandBuilder::diffFilters()
{
    LinkConfig a = bridgeConfig();
    a.classes << mavlinkClass();
    LinkConfig b = a;
    b.classes[0].port = 5760;
    const Diff d = B::diff(B::buildPlan(a), B::buildPlan(b));
    QCOMPARE(int(d.level), int(DiffLevel::Filters));
    const QStringList r = rendered(d.commands);
    QCOMPARE(r.size(), 6);
    QCOMPARE(r[0], QString("tc filter del dev enp2s0 parent 1d:"));
    QVERIFY(d.commands[0].ignoreError);
    QVERIFY(r[1].contains("dst_port 5760"));
    QCOMPARE(r[3], QString("tc filter del dev enp1s0 parent 1d:"));
}

void TestTcCommandBuilder::diffDefaultBand()
{
    LinkConfig a = bridgeConfig();
    LinkConfig b = a;
    b.otherMode = ClassMode::Excluded;
    const Diff d = B::diff(B::buildPlan(a), B::buildPlan(b));
    QCOMPARE(int(d.level), int(DiffLevel::Filters));
    QCOMPARE(rendered(d.commands).first(),
             QString("tc qdisc change dev enp2s0 root handle 1d: prio bands 2 priomap 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0"));
}

void TestTcCommandBuilder::diffRebuildOnStructure()
{
    LinkConfig a = bridgeConfig();
    a.classes << mavlinkClass(ClassMode::Link);
    LinkConfig b = a;
    b.classes[0].mode = ClassMode::Custom; // adds a band
    const Diff d = B::diff(B::buildPlan(a), B::buildPlan(b));
    QCOMPARE(int(d.level), int(DiffLevel::Rebuild));
    const QStringList r = rendered(d.commands);
    QCOMPARE(r[0], QString("tc qdisc del dev enp2s0 root"));
    QCOMPARE(r[1], QString("tc qdisc del dev enp1s0 root"));
    QVERIFY(r[2].startsWith("tc qdisc replace dev enp2s0 root handle 1d: prio bands 3"));
}

void TestTcCommandBuilder::diffRebuildOnModeSwitch()
{
    const Diff d = B::diff(B::buildPlan(hostConfig()), B::buildPlan(bridgeConfig()));
    QCOMPARE(int(d.level), int(DiffLevel::Rebuild));
    const QStringList r = rendered(d.commands);
    QCOMPARE(r.mid(0, 3), (QStringList{"tc qdisc del dev eth0 root", "tc qdisc del dev eth0 ingress",
                                       "ip link del dev ifb-dlsim"}));
    LinkConfig other = hostConfig();
    other.hostIface = "eth1";
    QCOMPARE(int(B::diff(B::buildPlan(hostConfig()), B::buildPlan(other)).level), int(DiffLevel::Rebuild));
}

void TestTcCommandBuilder::diffFromNothing()
{
    const Plan p = B::buildPlan(hostConfig());
    const Diff d = B::diff(std::nullopt, p);
    QCOMPARE(int(d.level), int(DiffLevel::Rebuild));
    QCOMPARE(d.commands, B::buildAll(p));
}

void TestTcCommandBuilder::teardownCommands()
{
    QCOMPARE(rendered(B::teardown(B::buildPlan(bridgeConfig()))),
             (QStringList{"tc qdisc del dev enp2s0 root", "tc qdisc del dev enp1s0 root"}));
    const CommandList host = B::teardown(B::buildPlan(hostConfig()));
    QCOMPARE(rendered(host), (QStringList{"tc qdisc del dev eth0 root", "tc qdisc del dev eth0 ingress",
                                          "ip link del dev ifb-dlsim"}));
    for (const auto &c : host)
        QVERIFY(c.ignoreError);
}

void TestTcCommandBuilder::bridgeCommands()
{
    QCOMPARE(rendered(B::setupBridge("enp1s0", "enp2s0")),
             (QStringList{"ip link add name br-dlsim type bridge stp_state 0",
                          "ip link set dev enp1s0 master br-dlsim", "ip link set dev enp2s0 master br-dlsim",
                          "ip link set dev enp1s0 up", "ip link set dev enp2s0 up", "ip link set dev br-dlsim up"}));
    QCOMPARE(rendered(B::teardownBridge("enp1s0", "enp2s0")),
             (QStringList{"ip link set dev enp1s0 nomaster", "ip link set dev enp2s0 nomaster",
                          "ip link del dev br-dlsim"}));
}

void TestTcCommandBuilder::leftovers()
{
    const QJsonArray links = QJsonDocument::fromJson(R"([
        {"ifindex":1,"ifname":"lo"},{"ifindex":2,"ifname":"eth0"},{"ifindex":3,"ifname":"eth1","master":"br-dlsim"},
        {"ifindex":4,"ifname":"br-dlsim"},{"ifindex":5,"ifname":"ifb-dlsim"},{"ifindex":6,"ifname":"eth2"}])").array();
    const QJsonArray qdiscs = QJsonDocument::fromJson(R"([
        {"kind":"noqueue","handle":"0:","dev":"lo","root":true},
        {"kind":"prio","handle":"1d:","dev":"eth0","root":true},
        {"kind":"netem","handle":"20:","dev":"eth0","parent":"1d:2"},
        {"kind":"ingress","handle":"ffff:","dev":"eth0","parent":"ffff:fff1"},
        {"kind":"prio","handle":"1:","dev":"eth1","root":true},
        {"kind":"ingress","handle":"ffff:","dev":"eth2","parent":"ffff:fff1"},
        {"kind":"prio","handle":"1d:","dev":"ifb-dlsim","root":true}])").array();
    QCOMPARE(B::devicesWithIngress(qdiscs), (QStringList{"eth0", "eth2"}));
    QHash<QString, QJsonArray> filters;
    filters["eth0"] = QJsonDocument::fromJson(R"([{"protocol":"all","pref":4919,"kind":"matchall","chain":0}])").array();
    filters["eth2"] = QJsonDocument::fromJson(R"([{"protocol":"all","pref":1,"kind":"matchall","chain":0}])").array();
    QCOMPARE(rendered(B::leftoverCleanup(links, qdiscs, filters)),
             (QStringList{"tc qdisc del dev eth0 root", "tc qdisc del dev eth0 ingress", "ip link del dev ifb-dlsim",
                          "ip link del dev br-dlsim"}));
    QVERIFY(B::leftoverCleanup(QJsonArray{}, QJsonArray{}, {}).isEmpty());
}

void TestTcCommandBuilder::validation_data()
{
    QTest::addColumn<QJsonObject>("config");
    QTest::addColumn<QString>("error"); // substring; empty = valid
    const QJsonObject base{{"mode", "bridge"}, {"groundIface", "eth0"}, {"airIface", "eth1"}};
    auto with = [&](QJsonObject o, const QJsonObject &extra) {
        for (auto it = extra.begin(); it != extra.end(); ++it)
            o.insert(it.key(), it.value());
        return o;
    };
    QTest::newRow("valid") << base << "";
    QTest::newRow("same ifaces") << with(base, {{"airIface", "eth0"}}) << "must differ";
    QTest::newRow("unknown iface") << with(base, {{"airIface", "eth9"}}) << "does not exist";
    QTest::newRow("bad iface name") << with(base, {{"airIface", "eth0;reboot"}}) << "invalid";
    QTest::newRow("lo") << with(base, {{"airIface", "lo"}}) << "cannot be used";
    QTest::newRow("our bridge") << with(base, {{"airIface", "br-dlsim"}}) << "cannot be used";
    QTest::newRow("host ok") << QJsonObject{{"mode", "host"}, {"hostIface", "eth1"}} << "";
    QTest::newRow("delay range") << with(base, {{"uplink", QJsonObject{{"delayMs", 70000}}}}) << "uplink.delayMs";
    QTest::newRow("loss range") << with(base, {{"downlink", QJsonObject{{"lossPct", 101}}}}) << "downlink.lossPct";
    QTest::newRow("negative") << with(base, {{"downlink", QJsonObject{{"jitterMs", -1}}}}) << "jitterMs";
    QTest::newRow("tiny rate") << with(base, {{"uplink", QJsonObject{{"rateKbit", 4}}}}) << "rateKbit";
    QTest::newRow("bad ip") << with(base, {{"classes", QJsonArray{QJsonObject{{"name", "a"}, {"ip", "300.1.1.1"}}}}})
                            << "invalid IPv4";
    QTest::newRow("port needs tcp/udp")
        << with(base, {{"classes", QJsonArray{QJsonObject{{"name", "a"}, {"port", 22}}}}}) << "needs protocol";
    QTest::newRow("custom profile validated")
        << with(base, {{"classes", QJsonArray{QJsonObject{{"name", "a"}, {"mode", "custom"},
                                                          {"uplink", QJsonObject{{"corruptPct", 200}}}}}}})
        << "corruptPct";
    QJsonArray many;
    for (int i = 0; i < 15; ++i)
        many.append(QJsonObject{{"name", QString::number(i)}, {"mode", "custom"}});
    QTest::newRow("too many custom") << with(base, {{"classes", many}}) << "at most 14";
    QTest::newRow("other custom") << with(base, {{"otherMode", "custom"}}) << "Other";
}

void TestTcCommandBuilder::validation()
{
    QFETCH(QJsonObject, config);
    QFETCH(QString, error);
    const QStringList errs = LinkConfig::fromJson(config).validate({"lo", "eth0", "eth1", "br-dlsim"});
    if (error.isEmpty()) {
        QVERIFY2(errs.isEmpty(), qPrintable(errs.join("; ")));
    } else {
        QVERIFY2(errs.join("; ").contains(error), qPrintable(errs.join("; ")));
    }
}

void TestTcCommandBuilder::jsonRoundTrip()
{
    LinkConfig c = hostConfig();
    c.uplink.lossModel = LossModel::GilbertElliott;
    c.uplink.geP = 1.5;
    c.uplink.distribution = Distribution::ParetoNormal;
    c.downlink.rateKbit = 1234;
    c.downlink.reorderGap = 7;
    c.classes << mavlinkClass(ClassMode::Custom);
    c.classes[0].uplink.corruptPct = 0.5;
    c.cut = true;
    QCOMPARE(LinkConfig::fromJson(c.toJson()), c);
}

void TestTcCommandBuilder::interpolation()
{
    Profile a;
    a.delayMs = 10;
    a.lossPct = 0;
    a.rateKbit = 0;
    Profile b;
    b.delayMs = 110;
    b.lossPct = 20;
    b.rateKbit = 1000;
    QCOMPARE(interpolate(a, b, 0), a);
    QCOMPARE(interpolate(a, b, 1), b);
    const Profile m = interpolate(a, b, 0.5);
    QCOMPARE(m.delayMs, 60.0);
    QCOMPARE(m.lossPct, 10.0);
    QCOMPARE(m.rateKbit, quint64(500500)); // unlimited counts as 1 Gbit/s
    QCOMPARE(interpolate(b, a, 1).rateKbit, quint64(0));
    // Different loss models pass through zero loss at the midpoint.
    Profile g;
    g.lossModel = LossModel::GilbertElliott;
    g.geP = 4;
    QCOMPARE(interpolate(b, g, 0.25).lossPct, 10.0);
    QCOMPARE(interpolate(b, g, 0.5).geP, 0.0);
    QCOMPARE(interpolate(b, g, 0.75).geP, 2.0);
    QCOMPARE(int(interpolate(b, g, 0.75).lossModel), int(LossModel::GilbertElliott));
}

void TestTcCommandBuilder::shellRendering()
{
    const Command c{"tc", {"filter", "it's", "a b", ""}, false};
    QCOMPARE(c.toString(), QString("tc filter 'it'\\''s' 'a b' ''"));
}

QTEST_GUILESS_MAIN(TestTcCommandBuilder)
#include "tst_tccommandbuilder.moc"
