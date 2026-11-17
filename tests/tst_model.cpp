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
    void issuePaths();
    void endpoints();
    void oldConfigWithoutEndpoints();
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
    QCOMPARE(c.withField("proto", "websocket").port, quint16(4555));
    QVERIFY(c.withField("proto", "websocket").proto == Proto::WebSocket);
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

void TestModel::issuePaths()
{
    LinkConfig cfg;
    cfg.mode = Mode::Bridge;
    cfg.groundIface = "eth0";
    cfg.airIface = "eth0";
    cfg.uplink.delayMs = 70000;
    TrafficClass ok;
    ok.name = "ok";
    TrafficClass bad;
    bad.name = "bad";
    bad.ip = "999.1..3";
    bad.mode = ClassMode::Custom;
    bad.downlink.lossPct = 150;
    cfg.classes = {ok, bad};

    QMap<QString, QString> byPath;
    for (const Issue &i : cfg.issues({"eth0", "eth1"})) {
        QVERIFY2(!i.message.isEmpty() && !i.text.isEmpty(), qPrintable(i.path));
        byPath.insert(i.path, i.message);
    }
    QCOMPARE(byPath.keys(), (QStringList{"airIface", "classes.1.downlink.lossPct", "classes.1.ip", "uplink.delayMs"}));
    QVERIFY(byPath["airIface"].contains("differ"));
    QVERIFY(cfg.validate({"eth0", "eth1"}).join("; ").contains("class 'bad': downlink.lossPct must be within"));

    cfg.airIface.clear();
    QCOMPARE(cfg.issues({"eth0"}).first().path, QStringLiteral("airIface"));
    QCOMPARE(cfg.issues({"eth0"}).first().message, QStringLiteral("choose an interface"));
}

void TestModel::endpoints()
{
    LinkConfig cfg;
    cfg.mode = Mode::Host;
    cfg.hostIface = "eth0";
    TrafficClass tel;
    tel.name = "Telemetry";
    tel.proto = Proto::Tcp;
    tel.port = 4555;
    tel.endpoint = "Vehicle";
    TrafficClass lit;
    lit.name = "Literal";
    lit.ip = "198.51.100.7";
    cfg.classes = {tel, lit};
    // No IP on the endpoint yet: the class can't be matched.
    QVERIFY(cfg.validate({"eth0"}).join(";").contains("has no IP yet"));
    cfg.setEndpointIp("Vehicle", "192.0.2.10");
    QCOMPARE(cfg.classes[0].ip, QStringLiteral("192.0.2.10"));
    QCOMPARE(cfg.classes[1].ip, QStringLiteral("198.51.100.7"));
    QVERIFY2(cfg.validate({"eth0"}).isEmpty(), qPrintable(cfg.validate({"eth0"}).join(";")));
    cfg.setEndpointIp("Vehicle", "192.0.2.99");
    QCOMPARE(cfg.classes[0].ip, QStringLiteral("192.0.2.99"));
    QVERIFY(cfg.renameEndpoint("Vehicle", "Drone"));
    QCOMPARE(cfg.classes[0].endpoint, QStringLiteral("Drone"));
    cfg.endpoints << Endpoint{"Base", "192.0.2.1"};
    QVERIFY(!cfg.renameEndpoint("Drone", "Base"));
    cfg.removeEndpoint("Drone");
    QVERIFY(cfg.classes[0].endpoint.isEmpty());
    QCOMPARE(cfg.classes[0].ip, QStringLiteral("192.0.2.99")); // kept as a literal
    cfg.endpoints[0].ip = "300.1.1.1";
    QCOMPARE(cfg.issues({"eth0"}).first().path, QStringLiteral("endpoints.0.ip"));
    // Round trip
    const LinkConfig back = LinkConfig::fromJson(cfg.toJson());
    QVERIFY(back == cfg);
}

void TestModel::oldConfigWithoutEndpoints()
{
    const LinkConfig c = LinkConfig::fromJson(QJsonObject{
        {"mode", "bridge"}, {"groundIface", "eth0"}, {"airIface", "eth1"},
        {"classes", QJsonArray{QJsonObject{{"name", "MAVLink"}, {"proto", "tcp"}, {"ip", "192.0.2.10"}, {"port", 4555}}}}});
    QCOMPARE(c.classes[0].ip, QStringLiteral("192.0.2.10"));
    QVERIFY(c.classes[0].endpoint.isEmpty());
    QCOMPARE(c.endpoints.size(), 1);
    QCOMPARE(c.endpoints[0].name, QStringLiteral("Vehicle"));
    QVERIFY(c.validate({"eth0", "eth1"}).isEmpty());
}

QTEST_GUILESS_MAIN(TestModel)
#include "tst_model.moc"
