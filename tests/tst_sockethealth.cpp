// Unit tests for the plain-language socket status rules, on parsed ss samples.
#include "SocketHealth.h"

#include <QtTest>

using namespace dlsim;

class TestSocketHealth : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void healthy();
    void stuckOnRetransmit();
    void zeroWindow();
    void halfOpen();
    void idle();
    void connectingAndClosing();
    void recoveryTime();
};

static TcpSocketInfo ss(const char *text)
{
    const auto s = parseSs(QString::fromLatin1(text));
    Q_ASSERT(s.size() == 1);
    return s.first();
}

void TestSocketHealth::healthy()
{
    SocketHealth h;
    h.update(ss("ESTAB 0 0 192.0.2.20:43182 192.0.2.10:4555 timer:(keepalive,44sec,0)\n"
                "\t rto:204 rtt:41.7/0.7 cwnd:10 lastsnd:200 lastrcv:64 lastack:64\n"), 10);
    QCOMPARE(h.kind(), QStringLiteral("healthy"));
    QCOMPARE(h.text(), QStringLiteral("Healthy · RTT 42 ms"));
}

void TestSocketHealth::stuckOnRetransmit()
{
    SocketHealth h;
    h.update(ss("ESTAB 0 2896 192.0.2.20:43182 192.0.2.10:4555 timer:(on,12.3sec,6)\n"
                "\t rto:25600 backoff:5 rtt:31.2/4.1 cwnd:1 unacked:2 retrans:1/9 lastsnd:3120 lastrcv:41000 lastack:41000\n"), 50);
    QCOMPARE(h.kind(), QStringLiteral("stuck"));
    QCOMPARE(h.text(), QStringLiteral("Stuck · waiting on retransmit timer, next try in 12.3 s (backoff 5)"));
}

void TestSocketHealth::zeroWindow()
{
    SocketHealth h;
    h.update(ss("ESTAB 5 0 192.0.2.20:51000 192.0.2.10:4555 timer:(persist,3sec,0)\n\t rto:208 rtt:3/1.5\n"), 1);
    QCOMPARE(h.kind(), QStringLiteral("stuck"));
    QVERIFY(h.text().contains("receive window is zero"));
}

void TestSocketHealth::halfOpen()
{
    SocketHealth h;
    h.update(ss("ESTAB 0 120 192.0.2.20:43182 192.0.2.10:4555\n"
                "\t rto:204 rtt:30/2 unacked:1 lastsnd:100 lastrcv:45000 lastack:45000\n"), 100);
    QCOMPARE(h.kind(), QStringLiteral("halfopen"));
    QCOMPARE(h.text(), QStringLiteral("Possibly half-open · ESTAB, no data acknowledged for 45.0 s"));
}

void TestSocketHealth::idle()
{
    SocketHealth h;
    h.update(ss("ESTAB 0 0 a:1 b:2 timer:(keepalive,4sec,0)\n\t rto:204 rtt:3/1 lastsnd:60000 lastrcv:61000 lastack:61000\n"), 100);
    QCOMPARE(h.kind(), QStringLiteral("idle"));
}

void TestSocketHealth::connectingAndClosing()
{
    SocketHealth h;
    h.update(ss("SYN-SENT 0 1 a:1 b:2 timer:(on,2sec,2)\n\t rto:4000 backoff:2\n"), 1);
    QCOMPARE(h.kind(), QStringLiteral("connecting"));
    h.update(ss("FIN-WAIT-1 0 1 a:1 b:2\n\t rto:204\n"), 2);
    QCOMPARE(h.kind(), QStringLiteral("closing"));
    h.update(ss("LISTEN 0 4096 127.0.0.1:4555 0.0.0.0:*\n"), 3);
    QCOMPARE(h.kind(), QStringLiteral("listening"));
    h.update(ss("ESTAB 0 0 a:1 b:2\n\t rto:204 rtt:0.034/0.037 lastsnd:100 lastrcv:100 lastack:100\n"), 4);
    QCOMPARE(h.text(), QStringLiteral("Healthy · RTT 0.03 ms"));
}

void TestSocketHealth::recoveryTime()
{
    SocketHealth h;
    // Link restored at t=100. Still backing off at t=110.
    h.linkRestored(100);
    h.update(ss("ESTAB 0 2896 a:1 b:2 timer:(on,13sec,6)\n\t rto:25600 backoff:5 unacked:2 retrans:1/9 lastack:30000\n"), 110);
    QCOMPARE(h.kind(), QStringLiteral("stuck"));
    QVERIFY(h.waitingForRecovery());
    QVERIFY(h.text().contains("waiting for data"));
    QCOMPARE(h.lastRecoveryS(), -1.0);
    // t=125: last ACK 1.6 s ago -> first ACK at 123.4 -> recovered 23.4 s after the restore.
    h.update(ss("ESTAB 0 0 a:1 b:2\n\t rto:204 rtt:30/2 lastsnd:100 lastrcv:1600 lastack:1600\n"), 125);
    QCOMPARE(h.kind(), QStringLiteral("recovered"));
    QVERIFY(qAbs(h.lastRecoveryS() - 23.4) < 1e-9);
    QCOMPARE(h.text(), QStringLiteral("Recovered 23.4 s after link was restored"));
    QVERIFY(!h.waitingForRecovery());
    // 30 s later: healthy again, the measurement stays.
    h.update(ss("ESTAB 0 0 a:1 b:2\n\t rto:204 rtt:30/2 lastsnd:100 lastrcv:100 lastack:100\n"), 160);
    QCOMPARE(h.kind(), QStringLiteral("healthy"));
    QVERIFY(qAbs(h.lastRecoveryS() - 23.4) < 1e-9);
}

QTEST_GUILESS_MAIN(TestSocketHealth)
#include "tst_sockethealth.moc"
