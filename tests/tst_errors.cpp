// Unit tests for dlsim::explainError: known failures get a cause, fix and action.
#include "ErrorExplainer.h"

#include <QtTest>

using namespace dlsim;

class TestErrors : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void explain_data();
    void explain();
    void unknownFallsBackToFirstLine();
};

void TestErrors::explain_data()
{
    QTest::addColumn<QString>("context");
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("title");  // substring
    QTest::addColumn<QString>("action");

    QTest::newRow("prompt cancelled") << "helper" << "authentication dismissed" << "password prompt was cancelled" << "retry-helper";
    QTest::newRow("not authorized") << "helper" << "not authorized (or pkexec failed)" << "Not authorized" << "retry-helper";
    QTest::newRow("helper missing") << "helper" << "helper not found at /usr/libexec/dlsim/dlsim-helper" << "missing" << "";
    QTest::newRow("helper exited") << "helper" << "helper exited with code 1" << "stopped unexpectedly" << "retry-helper";
    QTest::newRow("helper crashed") << "helper" << "helper crashed" << "stopped unexpectedly" << "retry-helper";
    QTest::newRow("netem missing")
        << "apply"
        << "apply failed, impairment removed: `tc qdisc replace dev eth1 parent 1d:1 handle 10: netem delay 8ms` failed: "
           "Error: Specified qdisc kind is unknown."
        << "no 'netem'" << "copy-command";
    QTest::newRow("prio missing")
        << "apply"
        << "apply failed, impairment removed: `tc qdisc replace dev dla0 root handle 1d: prio bands 2` failed: "
           "Error: Specified qdisc kind is unknown."
        << "no 'prio'" << "copy-command";
    QTest::newRow("device gone") << "bridge" << "Cannot find device \"enp3s0\"" << "enp3s0" << "refresh-interfaces";
    QTest::newRow("not permitted") << "apply" << "RTNETLINK answers: Operation not permitted" << "Not allowed" << "retry-helper";
    QTest::newRow("enslaved") << "bridge" << "`ip link set dev enp2s0 master br-dlsim` failed: Device or resource busy"
                              << "enp2s0" << "copy-command";
    QTest::newRow("helper not running") << "socket" << "helper is not running" << "not running" << "retry-helper";
    QTest::newRow("socket other") << "socket" << "ss: bad filter" << "TCP sockets" << "";
    QTest::newRow("start validation") << "start" << "ground and air interfaces must differ" << "not complete" << "";
}

void TestErrors::explain()
{
    QFETCH(QString, context);
    QFETCH(QString, raw);
    QFETCH(QString, title);
    QFETCH(QString, action);
    const ExplainedError e = explainError(context, raw);
    QVERIFY2(e.title.contains(title), qPrintable(e.title));
    QCOMPARE(e.action, action);
    if (!action.isEmpty())
        QVERIFY(!e.actionLabel.isEmpty());
    if (action == "copy-command")
        QVERIFY(!e.actionArg.isEmpty());
}

void TestErrors::unknownFallsBackToFirstLine()
{
    const ExplainedError e = explainError("apply", "  something odd\nmore text ");
    QCOMPARE(e.title, QStringLiteral("something odd"));
    QVERIFY(e.fix.isEmpty());
    QVERIFY(e.action.isEmpty());
}

QTEST_GUILESS_MAIN(TestErrors)
#include "tst_errors.moc"
