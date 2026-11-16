// Unit tests for model helpers used by the GUI when editing a config.
#include "Model.h"

#include <QtTest>

using namespace dlsim;

class TestModel : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void classProtoWithoutPortsDropsPort_data();
    void classProtoWithoutPortsDropsPort();
    void classProtoWithPortsKeepsPort();
    void classWithFieldKeepsOtherFields();
};

void TestModel::classProtoWithoutPortsDropsPort_data()
{
    QTest::addColumn<QString>("proto");
    QTest::newRow("any") << "any";
    QTest::newRow("icmp") << "icmp";
}

void TestModel::classProtoWithoutPortsDropsPort()
{
    QFETCH(QString, proto);
    TrafficClass c;
    c.name = "MAVLink";
    c.proto = Proto::Tcp;
    c.ip = "192.0.2.10";
    c.port = 4555;
    QVERIFY(c.validate().isEmpty());

    const TrafficClass n = c.withField("proto", proto);
    QCOMPARE(n.port, quint16(0));
    QVERIFY2(n.validate().isEmpty(), qPrintable(n.validate().join("; ")));

    LinkConfig cfg;
    cfg.groundIface = "eth0";
    cfg.airIface = "eth1";
    cfg.classes = {n};
    QVERIFY2(cfg.validate({"eth0", "eth1"}).isEmpty(), qPrintable(cfg.validate({"eth0", "eth1"}).join("; ")));
}

void TestModel::classProtoWithPortsKeepsPort()
{
    TrafficClass c;
    c.proto = Proto::Tcp;
    c.port = 4555;
    QCOMPARE(c.withField("proto", "udp").port, quint16(4555));
    QVERIFY(c.withField("proto", "udp").proto == Proto::Udp);
}

void TestModel::classWithFieldKeepsOtherFields()
{
    TrafficClass c;
    c.name = "Video";
    c.proto = Proto::Udp;
    c.ip = "192.0.2.10";
    c.port = 5600;
    c.mode = ClassMode::Custom;
    c.uplink.delayMs = 40;
    TrafficClass n = c.withField("name", "RTP video");
    QCOMPARE(n.name, QString("RTP video"));
    n.name = c.name;
    QVERIFY(n == c);
}

QTEST_GUILESS_MAIN(TestModel)
#include "tst_model.moc"
