// Unit tests for the simple-mode mapping onto netem profiles.
#include "SimpleMode.h"

#include <QtTest>

using namespace dlsim;

class TestSimple : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void occasionalLoss();
    void burstyLossRoundTrip_data();
    void burstyLossRoundTrip();
    void noneClearsLoss();
    void keepsAdvancedFields();
    void advancedOnlyFields();
    void summary();
    void mapRoundTrip();
};

void TestSimple::occasionalLoss()
{
    SimpleSettings s;
    s.latencyMs = 40;
    s.variationMs = 5;
    s.loss = SimpleSettings::Loss::Occasional;
    s.lossPct = 1;
    s.rateKbit = 2000;
    const Profile p = applySimple({}, s);
    QCOMPARE(p.delayMs, 40.0);
    QCOMPARE(p.jitterMs, 5.0);
    QVERIFY(p.lossModel == LossModel::Random);
    QCOMPARE(p.lossPct, 1.0);
    QCOMPARE(p.rateKbit, quint64(2000));
    QVERIFY(p.validate().isEmpty());
}

void TestSimple::burstyLossRoundTrip_data()
{
    QTest::addColumn<double>("lossPct");
    QTest::addColumn<double>("burst");
    QTest::newRow("5% bursts of 10") << 5.0 << 10.0;
    QTest::newRow("1% bursts of 4") << 1.0 << 4.0;
    QTest::newRow("30% bursts of 50") << 30.0 << 50.0;
}

void TestSimple::burstyLossRoundTrip()
{
    QFETCH(double, lossPct);
    QFETCH(double, burst);
    SimpleSettings s;
    s.loss = SimpleSettings::Loss::Bursty;
    s.lossPct = lossPct;
    s.burstPackets = burst;
    const Profile p = applySimple({}, s);
    QVERIFY(p.lossModel == LossModel::GilbertElliott);
    QCOMPARE(p.geBadLossPct, 100.0);
    QCOMPARE(p.geGoodLossPct, 0.0);
    QVERIFY(qFuzzyCompare(p.geR, 100.0 / burst));
    QVERIFY(qAbs(averageLossPct(p) - lossPct) < 1e-9);
    QVERIFY(p.validate().isEmpty());
    const SimpleSettings back = toSimple(p);
    QVERIFY(back.loss == SimpleSettings::Loss::Bursty);
    QVERIFY(qAbs(back.lossPct - lossPct) < 1e-9);
    QVERIFY(qAbs(back.burstPackets - burst) < 1e-9);
    QVERIFY(dlsim::advancedOnlyFields(p).isEmpty());
}

void TestSimple::noneClearsLoss()
{
    Profile p;
    p.lossPct = 3;
    SimpleSettings s = toSimple(p);
    s.loss = SimpleSettings::Loss::None;
    const Profile q = applySimple(p, s);
    QCOMPARE(averageLossPct(q), 0.0);
    QVERIFY(toSimple(q).loss == SimpleSettings::Loss::None);
}

void TestSimple::keepsAdvancedFields()
{
    Profile p;
    p.jitterMs = 10;
    p.delayCorrPct = 25;
    p.duplicatePct = 0.5;
    p.reorderPct = 1;
    p.limitPkts = 500;
    SimpleSettings s = toSimple(p);
    s.latencyMs = 70;
    const Profile q = applySimple(p, s);
    QCOMPARE(q.delayMs, 70.0);
    QCOMPARE(q.delayCorrPct, 25.0);
    QCOMPARE(q.duplicatePct, 0.5);
    QCOMPARE(q.reorderPct, 1.0);
    QCOMPARE(q.limitPkts, quint32(500));
}

void TestSimple::advancedOnlyFields()
{
    Profile p;
    QVERIFY(dlsim::advancedOnlyFields(p).isEmpty());
    p.jitterMs = 5;
    p.distribution = Distribution::Pareto;
    p.corruptPct = 0.1;
    const QStringList f = dlsim::advancedOnlyFields(p);
    QCOMPARE(f.size(), 2);
    QVERIFY(f.join(",").contains("pareto"));
    QVERIFY(f.join(",").contains("corruption"));
}

void TestSimple::summary()
{
    Profile up, down;
    up.delayMs = 40;
    down.delayMs = 40;
    up.lossPct = down.lossPct = 1;
    up.rateKbit = down.rateKbit = 2000;
    QCOMPARE(summarize(up, down), QStringLiteral("≈ 80 ms round trip · about 1 in 100 packets lost · max 2 Mbit/s"));
    down.lossPct = 0;
    down.rateKbit = 8000;
    QCOMPARE(summarize(up, down), QStringLiteral("≈ 80 ms round trip · uplink about 1 in 100 packets lost, "
                                                 "downlink no loss · max 2 Mbit/s up, 8 Mbit/s down"));
    QCOMPARE(summarize({}, {}), QStringLiteral("≈ 0 ms round trip · no loss · no rate limit"));
}

void TestSimple::mapRoundTrip()
{
    SimpleSettings s;
    s.latencyMs = 12;
    s.loss = SimpleSettings::Loss::Bursty;
    s.lossPct = 2;
    s.burstPackets = 8;
    s.rateKbit = 500;
    const SimpleSettings t = SimpleSettings::fromMap(s.toMap());
    QCOMPARE(t.latencyMs, 12.0);
    QVERIFY(t.loss == SimpleSettings::Loss::Bursty);
    QCOMPARE(t.lossPct, 2.0);
    QCOMPARE(t.burstPackets, 8.0);
    QCOMPARE(t.rateKbit, quint64(500));
}

QTEST_GUILESS_MAIN(TestSimple)
#include "tst_simple.moc"
