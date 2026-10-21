// Unit tests for the tc JSON and ss text parsers.
#include "Parsers.h"

#include <QtTest>

using namespace dlsim;

class TestParsers : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void qdiscStats();
    void qdiscStatsMalformed();
    void durations_data();
    void durations();
    void ssEstablished();
    void ssRetransmitting();
    void ssMixed();
    void socketJsonRoundTrip();
};

void TestParsers::qdiscStats()
{
    const QByteArray json = R"([
      {"kind":"prio","handle":"1d:","root":true,"refcnt":2,"options":{"bands":2},
       "bytes":123456,"packets":789,"drops":12,"overlimits":3,"requeues":0,"backlog":1514,"qlen":1},
      {"kind":"netem","handle":"20:","parent":"1d:2","options":{"limit":1000},
       "bytes":100000,"packets":700,"drops":12,"overlimits":0,"requeues":0,"backlog":1514,"qlen":1}])";
    QList<QdiscStats> s;
    QVERIFY(parseQdiscStats(json, &s));
    QCOMPARE(s.size(), 2);
    QCOMPARE(s[0].kind, QString("prio"));
    QVERIFY(s[0].root);
    QCOMPARE(s[0].bytes, quint64(123456));
    QCOMPARE(s[0].packets, quint64(789));
    QCOMPARE(s[0].drops, quint64(12));
    QCOMPARE(s[0].overlimits, quint64(3));
    QCOMPARE(s[0].backlog, quint64(1514));
    QCOMPARE(s[1].parent, QString("1d:2"));
    QVERIFY(!s[1].root);
    QCOMPARE(QdiscStats::fromJson(s[1].toJson()).bytes, quint64(100000));
}

void TestParsers::qdiscStatsMalformed()
{
    QList<QdiscStats> s;
    QString err;
    QVERIFY(!parseQdiscStats("not json", &s, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!parseQdiscStats("{}", &s, &err));
    QVERIFY(parseQdiscStats("[]", &s, &err));
    QVERIFY(s.isEmpty());
}

void TestParsers::durations_data()
{
    QTest::addColumn<QString>("in");
    QTest::addColumn<double>("ms");
    QTest::newRow("ms") << "200ms" << 200.0;
    QTest::newRow("frac ms") << "4.476ms" << 4.476;
    QTest::newRow("sec") << "1.204sec" << 1204.0;
    QTest::newRow("int sec") << "44sec" << 44000.0;
    QTest::newRow("min sec") << "1min12sec" << 72000.0;
    QTest::newRow("min") << "2min" << 120000.0;
    QTest::newRow("bad") << "soon" << -1.0;
    QTest::newRow("empty") << "" << -1.0;
}

void TestParsers::durations()
{
    QFETCH(QString, in);
    QFETCH(double, ms);
    QCOMPARE(parseSsDuration(in), ms);
}

void TestParsers::ssEstablished()
{
    const QString out =
        "ESTAB 0      0      192.0.2.20:43182 192.0.2.10:4555 timer:(keepalive,44sec,0)\n"
        "\t ts sack cubic wscale:7,10 rto:204 rtt:0.679/0.763 ato:40 mss:1448 cwnd:10 bytes_sent:1 "
        "lastsnd:25088 lastrcv:64 lastack:64 pacing_rate 1bps\n";
    const auto socks = parseSs(out);
    QCOMPARE(socks.size(), 1);
    const auto &s = socks[0];
    QCOMPARE(s.state, QString("ESTAB"));
    QCOMPARE(s.local, QString("192.0.2.20:43182"));
    QCOMPARE(s.peer, QString("192.0.2.10:4555"));
    QCOMPARE(s.timer, QString("keepalive"));
    QCOMPARE(s.timerMs, 44000.0);
    QCOMPARE(s.timerRetrans, 0);
    QCOMPARE(s.rtoMs, 204.0);
    QCOMPARE(s.rttMs, 0.679);
    QCOMPARE(s.rttVarMs, 0.763);
    QCOMPARE(s.cwnd, 10);
    QCOMPARE(s.retransTotal, -1);
    QCOMPARE(s.backoff, 0);
    QCOMPARE(s.lastSndMs, qint64(25088));
    QCOMPARE(s.lastRcvMs, qint64(64));
    QCOMPARE(s.key(), QString("192.0.2.20:43182->192.0.2.10:4555"));
}

void TestParsers::ssRetransmitting()
{
    // A link outage: unacked data, exponential backoff, retransmit timer running.
    const QString out =
        "ESTAB 0      2896   192.0.2.20:43182 192.0.2.10:4555 timer:(on,12sec,6)\n"
        "\t cubic wscale:7,7 rto:25600 backoff:6 rtt:31.2/4.1 mss:1448 cwnd:1 ssthresh:7 unacked:2 "
        "retrans:1/9 lost:2 lastsnd:3120 lastrcv:41000 lastack:41000\n";
    const auto socks = parseSs(out);
    QCOMPARE(socks.size(), 1);
    const auto &s = socks[0];
    QCOMPARE(s.sendQ, 2896u);
    QCOMPARE(s.timer, QString("on"));
    QCOMPARE(s.timerMs, 12000.0);
    QCOMPARE(s.timerRetrans, 6);
    QCOMPARE(s.rtoMs, 25600.0);
    QCOMPARE(s.backoff, 6);
    QCOMPARE(s.unacked, 2);
    QCOMPARE(s.retransCur, 1);
    QCOMPARE(s.retransTotal, 9);
    QCOMPARE(s.cwnd, 1);
}

void TestParsers::ssMixed()
{
    const QString out =
        "SYN-SENT 0 1 192.0.2.20:50000 192.0.2.10:4555 timer:(on,3.000sec,2)\n"
        "\t cubic rto:4000 backoff:2 mss:524 cwnd:1 unacked:1 retrans:1/2\n"
        "TIME-WAIT 0 0 192.0.2.20:49000 192.0.2.10:4555 timer:(timewait,59sec,0)\n"
        "ESTAB 5 0 192.0.2.20:51000 192.0.2.10:4555 timer:(persist,1min3sec,0)\n"
        "\t rto:208 rtt:3/1.5\n"
        "\n";
    const auto socks = parseSs(out);
    QCOMPARE(socks.size(), 3);
    QCOMPARE(socks[0].state, QString("SYN-SENT"));
    QCOMPARE(socks[0].rtoMs, 4000.0);
    QCOMPARE(socks[0].retransTotal, 2);
    QCOMPARE(socks[1].state, QString("TIME-WAIT"));
    QCOMPARE(socks[1].timer, QString("timewait"));
    QCOMPARE(socks[1].rtoMs, -1.0);
    QCOMPARE(socks[2].recvQ, 5u);
    QCOMPARE(socks[2].timer, QString("persist"));
    QCOMPARE(socks[2].timerMs, 63000.0);
    QCOMPARE(socks[2].rttMs, 3.0);
}

void TestParsers::socketJsonRoundTrip()
{
    const auto socks = parseSs("ESTAB 0 0 a:1 b:2 timer:(on,1sec,1)\n\t rto:300 retrans:0/4 backoff:1\n");
    const auto r = TcpSocketInfo::fromJson(socks[0].toJson());
    QCOMPARE(r.rtoMs, 300.0);
    QCOMPARE(r.retransTotal, 4);
    QCOMPARE(r.backoff, 1);
    QCOMPARE(r.timerMs, 1000.0);
    QCOMPARE(r.key(), socks[0].key());
}

QTEST_GUILESS_MAIN(TestParsers)
#include "tst_parsers.moc"
