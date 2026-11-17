// Unit tests for the scenario delay/loss curve.
#include "ScenarioCurve.h"
#include "SimpleMode.h"

#include <QJsonDocument>
#include <QtTest>

using namespace dlsim;

class TestCurve : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void holdOutageFade();
    void unknownPresetKeepsPrevious();
    void dropoutsAverage();
};

static bool lookup(const QString &name, Profile *up, Profile *down)
{
    if (name == "A") {
        *up = Profile{};
        up->delayMs = 10;
        up->lossPct = 1;
        *down = *up;
        return true;
    }
    if (name == "B") {
        *up = Profile{};
        up->delayMs = 110;
        up->lossPct = 5;
        *down = *up;
        down->delayMs = 50;
        return true;
    }
    return false;
}

static QJsonArray steps(const char *json) { return QJsonDocument::fromJson(json).array(); }

void TestCurve::holdOutageFade()
{
    const auto c = scenarioCurve(steps(R"([{"type":"preset","preset":"A","durationS":10},
                                          {"type":"outage","durationS":5},
                                          {"type":"fade","preset":"B","durationS":20}])"),
                                 lookup, {}, {}, 4);
    QCOMPARE(c.first().t, 0.0);
    QCOMPARE(c.first().delayUpMs, 10.0);
    QCOMPARE(c[2].lossUpPct, 100.0); // outage
    QCOMPARE(c[2].t, 10.0);
    QCOMPARE(c.last().t, 35.0);
    QCOMPARE(c.last().delayUpMs, 110.0);
    QCOMPARE(c.last().delayDownMs, 50.0);
    // Midpoint of the fade = the engine's interpolate() at 0.5.
    Profile a, b, x;
    lookup("A", &a, &x);
    lookup("B", &b, &x);
    const Profile mid = interpolate(a, b, 0.5);
    const CurvePoint &m = c[4 + 2]; // 2 hold + 2 outage points, then fade samples 0..4
    QCOMPARE(m.t, 25.0);
    QCOMPARE(m.delayUpMs, mid.delayMs);
    QCOMPARE(m.lossUpPct, averageLossPct(mid));
}

void TestCurve::unknownPresetKeepsPrevious()
{
    const auto c = scenarioCurve(steps(R"([{"type":"preset","preset":"A","durationS":1},
                                          {"type":"preset","preset":"gone","durationS":1}])"), lookup, {}, {});
    QCOMPARE(c.last().delayUpMs, 10.0);
}

void TestCurve::dropoutsAverage()
{
    const auto c = scenarioCurve(steps(R"([{"type":"dropouts","durationS":60,"meanIntervalS":9,"meanDurationS":1}])"),
                                 lookup, {}, {});
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].lossUpPct, 10.0);
}

QTEST_GUILESS_MAIN(TestCurve)
#include "tst_curve.moc"
