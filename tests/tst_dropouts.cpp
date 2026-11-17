// Unit tests for the seeded random-dropout schedule.
#include "Dropouts.h"

#include <QtTest>

using namespace dlsim;

class TestDropouts : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sameSeedSameSchedule();
    void differentSeedDifferentSchedule();
    void windowsInsideAndOrdered();
    void meansRoughlyRight();
    void shortStepMayHaveNone();
};

void TestDropouts::sameSeedSameSchedule()
{
    const auto a = dropoutSchedule(42, 10, 2, 300);
    const auto b = dropoutSchedule(42, 10, 2, 300);
    QVERIFY(!a.isEmpty());
    QCOMPARE(a.size(), b.size());
    for (int i = 0; i < a.size(); ++i) {
        QCOMPARE(a[i].startS, b[i].startS);
        QCOMPARE(a[i].lengthS, b[i].lengthS);
    }
}

void TestDropouts::differentSeedDifferentSchedule()
{
    const auto a = dropoutSchedule(1, 10, 2, 300);
    const auto b = dropoutSchedule(2, 10, 2, 300);
    QVERIFY(a.isEmpty() || b.isEmpty() || a.first().startS != b.first().startS);
}

void TestDropouts::windowsInsideAndOrdered()
{
    const auto w = dropoutSchedule(7, 5, 3, 120);
    double prevEnd = 0;
    for (const auto &d : w) {
        QVERIFY(d.startS > prevEnd); // a gap before every window
        QVERIFY(d.lengthS > 0);
        QVERIFY(d.endS() <= 120.0 + 1e-9);
        prevEnd = d.endS();
    }
}

void TestDropouts::meansRoughlyRight()
{
    const double total = 200000;
    const auto w = dropoutSchedule(123, 20, 4, total);
    double down = 0;
    for (const auto &d : w)
        down += d.lengthS;
    // Cycle = gap + length: 24 s on average, so ~8333 windows and ~1/6 of the time down.
    QVERIFY2(qAbs(w.size() - total / 24) < total / 24 * 0.05, qPrintable(QString::number(w.size())));
    QVERIFY2(qAbs(down / total - 4.0 / 24) < 0.01, qPrintable(QString::number(down / total)));
}

void TestDropouts::shortStepMayHaveNone()
{
    QVERIFY(dropoutSchedule(5, 1000, 1, 0.5).isEmpty() || dropoutSchedule(5, 1000, 1, 0.5).first().endS() <= 0.5);
}

QTEST_GUILESS_MAIN(TestDropouts)
#include "tst_dropouts.moc"
