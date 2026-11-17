// Unit tests for the run report: HTML content, CSVs and the zip writer.
#include "RunReport.h"
#include "ZipWriter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace dlsim;

class TestReport : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void htmlHasEverything();
    void htmlIsSelfContainedAndEscaped();
    void csvs();
    void crc();
    void zipReadable();
};

static QJsonObject sample()
{
    return QJsonDocument::fromJson(R"({
      "name": "Outage ladder", "startedAt": "2026-11-17T10:00:00", "endedAt": "2026-11-17T10:01:10",
      "durationS": 70, "completed": true,
      "config": {"mode": "bridge", "ports": "Ground on eth0 ⇄ Drone on eth1", "groundName": "Ground", "vehicleName": "Drone",
                 "preset": "Good LOS", "classes": ["Telemetry · TCP to/from Drone:4555 · uses link profile"]},
      "steps": [{"index": 0, "type": "preset", "preset": "Good LOS", "plannedS": 30, "startS": 0, "endS": 30},
                {"index": 1, "type": "outage", "plannedS": 10, "startS": 30, "endS": 40},
                {"index": 2, "type": "preset", "preset": "Good LOS", "plannedS": 30, "startS": 40, "endS": 70}],
      "outages": [{"startS": 30, "endS": 40, "sources": ["scenario"], "dropsUp": 120, "dropsDown": 95,
                   "recoveries": [{"socket": "10.0.0.2:5000->10.0.0.9:4555", "seconds": 23.4}]}],
      "samples": [{"t": 1, "upKbps": 100, "downKbps": 900, "upDropsPerS": 0, "downDropsPerS": 0, "upDrops": 0, "downDrops": 0},
                  {"t": 35, "upKbps": 0, "downKbps": 0, "upDropsPerS": 12, "downDropsPerS": 9.5, "upDrops": 60, "downDrops": 50}],
      "sockets": [{"t": 35, "socket": "a->b", "state": "ESTAB", "rtoMs": 3200, "backoff": 3, "retrans": 4, "status": "Stuck, a, b"}],
      "log": [{"t": 30, "category": "outage", "message": "LINK CUT (scenario) <b>"}]
    })").object();
}

void TestReport::htmlHasEverything()
{
    const QString h = renderReportHtml(sample());
    QVERIFY(h.startsWith("<!doctype html>"));
    for (const char *s : {"Outage ladder", "completed", "Good LOS", "Telemetry · TCP to/from Drone:4555", "Drop link",
                          "120 / 95", "<b>23.4 s</b>", "Throughput", "Drops", "retransmit timeout", "<svg", "1:10.0"})
        QVERIFY2(h.contains(QString::fromUtf8(s)), s);
}

void TestReport::htmlIsSelfContainedAndEscaped()
{
    const QString h = renderReportHtml(sample());
    QVERIFY(!h.contains("http://") && !h.contains("https://") && !h.contains("<script"));
    QVERIFY(h.contains("LINK CUT (scenario) &lt;b&gt;"));
}

void TestReport::csvs()
{
    const QStringList stats = reportStatsCsv(sample()).split('\n', Qt::SkipEmptyParts);
    QCOMPARE(stats.size(), 3);
    QCOMPARE(stats[2], QStringLiteral("35.000,0.0,0.0,12.00,9.50,60,50"));
    QVERIFY(reportSocketsCsv(sample()).contains(",\"Stuck, a, b\""));
    QVERIFY(reportLogCsv(sample()).contains("30.000,outage,LINK CUT (scenario) <b>"));
}

void TestReport::crc()
{
    QCOMPARE(crc32("123456789"), 0xCBF43926u);
    QCOMPARE(crc32(""), 0u);
}

void TestReport::zipReadable()
{
    const QByteArray z = makeZip({{"stats.csv", "a,b\n1,2\n"}, {"log.csv", "x\n"}});
    QVERIFY(z.startsWith("PK\x03\x04"));
    QVERIFY(z.contains("PK\x05\x06"));
    const QString unzip = QStandardPaths::findExecutable("unzip");
    if (unzip.isEmpty())
        QSKIP("unzip not installed");
    QTemporaryDir d;
    QFile f(d.filePath("t.zip"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(z);
    f.close();
    QProcess p;
    p.start(unzip, {"-t", f.fileName()});
    QVERIFY(p.waitForFinished());
    QVERIFY2(p.exitCode() == 0, p.readAll().constData());
}

QTEST_GUILESS_MAIN(TestReport)
#include "tst_report.moc"
