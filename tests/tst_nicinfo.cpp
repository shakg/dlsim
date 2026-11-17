// Unit tests for sysfs NIC reading and the unplug-to-identify detector.
#include "NicInfo.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace dlsim;

class TestNicInfo : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void readsSysfs();
    void rate();
    void unplugFound();
    void unplugAmbiguous();
    void unplugIgnoresNicsWithoutLink();
};

static void put(const QString &path, const QByteArray &v)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(v + "\n");
}

void TestNicInfo::readsSysfs()
{
    QTemporaryDir d;
    put(d.filePath("enp1s0/carrier"), "1");
    put(d.filePath("enp1s0/speed"), "1000");
    put(d.filePath("enp1s0/statistics/rx_bytes"), "123456");
    put(d.filePath("enp1s0/statistics/tx_bytes"), "789");
    QDir().mkpath(d.filePath("enp1s0/device"));
    put(d.filePath("veth0/carrier"), "0");
    put(d.filePath("veth0/speed"), "-1");
    const NicSample a = readNic(d.path(), "enp1s0");
    QCOMPARE(a.carrier, 1);
    QCOMPARE(a.speedMbps, 1000);
    QCOMPARE(a.rxBytes, quint64(123456));
    QCOMPARE(a.txBytes, quint64(789));
    QVERIFY(a.physical);
    const NicSample b = readNic(d.path(), "veth0");
    QCOMPARE(b.carrier, 0);
    QCOMPARE(b.speedMbps, -1);
    QVERIFY(!b.physical);
    QCOMPARE(readNic(d.path(), "gone").carrier, -1); // unreadable: unknown
}

void TestNicInfo::rate()
{
    QCOMPARE(rateKbps(0, 125000, 1000), 1000.0); // 125 kB in 1 s = 1 Mbit/s
    QCOMPARE(rateKbps(500, 100, 1000), 0.0);     // counter reset
}

void TestNicInfo::unplugFound()
{
    UnplugDetector u;
    u.start({{"enp1s0", 1}, {"enp2s0", 1}});
    QCOMPARE(u.update({{"enp1s0", 1}, {"enp2s0", 1}}), UnplugDetector::State::Waiting);
    QCOMPARE(u.update({{"enp1s0", 1}, {"enp2s0", 0}}), UnplugDetector::State::Found);
    QCOMPARE(u.found(), QStringLiteral("enp2s0"));
    // Plugging back in later doesn't change the answer.
    QCOMPARE(u.update({{"enp1s0", 1}, {"enp2s0", 1}}), UnplugDetector::State::Found);
}

void TestNicInfo::unplugAmbiguous()
{
    UnplugDetector u;
    u.start({{"a", 1}, {"b", 1}, {"c", 1}});
    QCOMPARE(u.update({{"a", 0}, {"b", -1}, {"c", 1}}), UnplugDetector::State::Ambiguous);
    QCOMPARE(u.lost(), (QStringList{"a", "b"}));
}

void TestNicInfo::unplugIgnoresNicsWithoutLink()
{
    UnplugDetector u;
    u.start({{"up", 1}, {"down", 0}, {"unknown", -1}});
    QCOMPARE(u.watched(), QStringList{"up"});
    QCOMPARE(u.update({{"up", 1}, {"down", 0}, {"unknown", -1}}), UnplugDetector::State::Waiting);
}

QTEST_GUILESS_MAIN(TestNicInfo)
#include "tst_nicinfo.moc"
