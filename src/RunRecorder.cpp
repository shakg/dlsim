#include "RunRecorder.h"

#include "EventLog.h"
#include "LinkController.h"
#include "RunReport.h"
#include "ScenarioEngine.h"
#include "SocketMonitor.h"
#include "StatsPoller.h"
#include "ZipWriter.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace dlsim;

namespace {

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(data) == data.size();
}

QString classSentence(const QVariantMap &c)
{
    const QString proto = c.value("proto").toString();
    const QString protoLabel = proto == "any" ? QStringLiteral("all protocols")
                               : proto == "websocket" ? QStringLiteral("WebSocket") : proto.toUpper();
    const QString where = !c.value("endpoint").toString().isEmpty() ? c.value("endpoint").toString()
                          : !c.value("ip").toString().isEmpty() ? c.value("ip").toString() : QStringLiteral("any IP");
    const int port = c.value("port").toInt();
    const QString mode = c.value("mode").toString();
    return QStringLiteral("%1 · %2 to/from %3%4 · %5")
        .arg(c.value("name").toString(), protoLabel, where,
             port > 0 ? ":" + QString::number(port) : QString(),
             mode == "excluded" ? "never impaired" : mode == "custom" ? "own profile" : "uses link profile");
}

} // namespace

RunRecorder::RunRecorder(ScenarioEngine *scenario, LinkController *link, StatsPoller *stats, SocketMonitor *sockets,
                         EventLog *log, QObject *parent)
    : QObject(parent), m_scenario(scenario), m_link(link), m_stats(stats), m_sockets(sockets), m_log(log)
{
    m_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/dlsim/reports";
    QDir().mkpath(m_dir);

    connect(scenario, &ScenarioEngine::stateChanged, this, [this] {
        const QString st = m_scenario->state();
        if (m_lastState == "stopped" && st == "playing")
            begin();
        else if (st == "stopped" && m_recording)
            // The engine logs "finished" just before stopping by itself.
            finish(m_scenario->totalElapsed() + 0.2 >= m_scenario->totalDuration() && !m_scenario->loop());
        m_lastState = st;
    });
    connect(scenario, &ScenarioEngine::progressChanged, this, &RunRecorder::onProgress);
    connect(link, &LinkController::cutChanged, this, &RunRecorder::onCut);
    connect(stats, &StatsPoller::sampled, this, [this] {
        if (!m_recording)
            return;
        const QVariantMap u = m_stats->uplink(), d = m_stats->downlink();
        m_samples.append(QJsonObject{{"t", now()},
                                     {"upKbps", u.value("kbps").toDouble()}, {"downKbps", d.value("kbps").toDouble()},
                                     {"upDropsPerS", u.value("dropsPerSec").toDouble()},
                                     {"downDropsPerS", d.value("dropsPerSec").toDouble()},
                                     {"upDrops", u.value("drops").toDouble()}, {"downDrops", d.value("drops").toDouble()}});
    });
    connect(sockets, &SocketMonitor::updated, this, [this] {
        if (!m_recording)
            return;
        for (const QVariant &v : m_sockets->sockets()) {
            const QVariantMap s = v.toMap();
            if (s.value("key").toString() != m_sockets->tracked())
                continue;
            m_socketRows.append(QJsonObject{{"t", now()}, {"socket", s.value("key").toString()},
                                            {"state", s.value("state").toString()}, {"rtoMs", s.value("rtoMs").toDouble()},
                                            {"backoff", s.value("backoff").toInt()}, {"retrans", s.value("retransTotal").toInt()},
                                            {"status", s.value("status").toString()}});
        }
    });
    connect(sockets, &SocketMonitor::recovered, this, [this](const QString &socket, double seconds) {
        if (!m_recording || m_outages.isEmpty())
            return;
        QJsonObject o = m_outages.last().toObject();
        QJsonArray rec = o.value("recoveries").toArray();
        rec.append(QJsonObject{{"socket", socket}, {"seconds", seconds}});
        o["recoveries"] = rec;
        m_outages[m_outages.size() - 1] = o;
    });
    connect(log, &EventLog::logged, this, [this](const QString &category, const QString &message) {
        if (m_recording)
            m_logRows.append(QJsonObject{{"t", now()}, {"category", category}, {"message", message}});
    });
}

QJsonObject RunRecorder::configJson() const
{
    QJsonArray classes;
    for (const QVariant &c : m_link->classes())
        if (c.toMap().value("enabled").toBool())
            classes.append(classSentence(c.toMap()));
    const bool bridge = m_link->mode() == "bridge";
    return {{"mode", m_link->mode()},
            {"ports", bridge ? QStringLiteral("%1 on %2 ⇄ %3 on %4").arg(m_link->groundName(), m_link->groundIface(),
                                                                         m_link->vehicleName(), m_link->airIface())
                             : QStringLiteral("%1 via %2").arg(m_link->vehicleName(), m_link->hostIface())},
            {"groundName", m_link->groundName()},
            {"vehicleName", m_link->vehicleName()},
            {"preset", m_link->activePreset()},
            {"classes", classes}};
}

void RunRecorder::begin()
{
    m_clock.start();
    m_started = QDateTime::currentDateTime();
    m_name = m_scenario->name();
    m_config = configJson();
    m_steps = m_outages = m_samples = m_socketRows = m_logRows = QJsonArray();
    m_currentStep = -1;
    m_cut = false;
    m_recording = true;
    Q_EMIT recordingChanged();
    onProgress();
    onCut();
}

void RunRecorder::onProgress()
{
    if (!m_recording)
        return;
    const int cur = m_scenario->currentStep();
    if (cur == m_currentStep)
        return;
    const double t = now();
    if (m_currentStep >= 0 && !m_steps.isEmpty()) {
        QJsonObject last = m_steps.last().toObject();
        last["endS"] = t;
        m_steps[m_steps.size() - 1] = last;
    }
    m_currentStep = cur;
    if (cur >= 0) {
        const QVariantMap s = m_scenario->steps().value(cur).toMap();
        m_steps.append(QJsonObject{{"index", cur}, {"type", s.value("type").toString()},
                                   {"preset", s.value("preset").toString()},
                                   {"plannedS", s.value("durationS").toDouble()}, {"startS", t}, {"endS", t}});
    }
}

void RunRecorder::onCut()
{
    if (!m_recording)
        return;
    const bool cut = m_link->isCut();
    if (cut == m_cut) {
        if (cut)
            for (const QString &s : m_link->cutSources())
                if (!m_cutSources.contains(s))
                    m_cutSources << s;
        return;
    }
    m_cut = cut;
    const double upDrops = m_stats->uplink().value("drops").toDouble(), downDrops = m_stats->downlink().value("drops").toDouble();
    if (cut) {
        m_cutStart = now();
        m_dropsAtCut[0] = upDrops;
        m_dropsAtCut[1] = downDrops;
        m_cutSources = m_link->cutSources();
    } else {
        m_outages.append(QJsonObject{{"startS", m_cutStart}, {"endS", now()},
                                     {"sources", QJsonArray::fromStringList(m_cutSources)},
                                     {"dropsUp", std::max(0.0, upDrops - m_dropsAtCut[0])},
                                     {"dropsDown", std::max(0.0, downDrops - m_dropsAtCut[1])},
                                     {"recoveries", QJsonArray()}});
    }
}

void RunRecorder::finish(bool completed)
{
    const double end = now();
    if (m_cut) { // still cut when stopped: close the outage
        m_cut = false;
        m_outages.append(QJsonObject{{"startS", m_cutStart}, {"endS", end}, {"sources", QJsonArray::fromStringList(m_cutSources)},
                                     {"dropsUp", 0}, {"dropsDown", 0}, {"recoveries", QJsonArray()}});
    }
    if (!m_steps.isEmpty()) {
        QJsonObject last = m_steps.last().toObject();
        last["endS"] = end;
        m_steps[m_steps.size() - 1] = last;
    }
    m_recording = false;
    Q_EMIT recordingChanged();

    const QJsonObject report{{"name", m_name},
                             {"startedAt", m_started.toString(Qt::ISODate)},
                             {"endedAt", QDateTime::currentDateTime().toString(Qt::ISODate)},
                             {"durationS", end},
                             {"completed", completed},
                             {"config", m_config},
                             {"steps", m_steps},
                             {"outages", m_outages},
                             {"samples", m_samples},
                             {"sockets", m_socketRows},
                             {"log", m_logRows}};
    QString safe = m_name;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_. -]")), "_");
    const QString base = m_dir + '/' + m_started.toString("yyyyMMdd-HHmmss") + ' ' + safe;
    if (!writeFile(base + ".json", QJsonDocument(report).toJson(QJsonDocument::Indented))
        || !writeFile(base + ".html", renderReportHtml(report).toUtf8())) {
        m_log->log("error", QStringLiteral("could not save the run report in %1").arg(m_dir));
        return;
    }
    m_log->log("scenario", QStringLiteral("run report saved: %1.html").arg(base));
    Q_EMIT reportsChanged();
    Q_EMIT reportReady(m_name, base + ".html");
}

QVariantList RunRecorder::reports() const
{
    QVariantList out;
    QDir d(m_dir);
    QStringList files = d.entryList({"*.json"}, QDir::Files, QDir::Name);
    std::reverse(files.begin(), files.end());
    for (const QString &f : files.mid(0, 30)) {
        QFile file(d.filePath(f));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject r = QJsonDocument::fromJson(file.readAll()).object();
        out << QVariantMap{{"json", d.filePath(f)},
                           {"html", d.filePath(f.chopped(5) + ".html")},
                           {"name", r.value("name").toString()},
                           {"when", QDateTime::fromString(r.value("startedAt").toString(), Qt::ISODate).toString("yyyy-MM-dd HH:mm")},
                           {"durationS", r.value("durationS").toDouble()},
                           {"completed", r.value("completed").toBool()},
                           {"outages", r.value("outages").toArray().size()}};
    }
    return out;
}

bool RunRecorder::openReport(const QString &htmlPath)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(htmlPath));
}

bool RunRecorder::exportHtml(const QString &htmlPath, const QUrl &file)
{
    QFile::remove(file.toLocalFile());
    const bool ok = QFile::copy(htmlPath, file.toLocalFile());
    m_log->log(ok ? "info" : "error", ok ? QStringLiteral("report saved to %1").arg(file.toLocalFile())
                                         : QStringLiteral("could not save the report to %1").arg(file.toLocalFile()));
    return ok;
}

bool RunRecorder::exportCsvZip(const QString &jsonPath, const QUrl &file)
{
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject r = QJsonDocument::fromJson(f.readAll()).object();
    const QByteArray zip = makeZip({{"stats.csv", reportStatsCsv(r).toUtf8()},
                                    {"sockets.csv", reportSocketsCsv(r).toUtf8()},
                                    {"log.csv", reportLogCsv(r).toUtf8()}});
    const bool ok = writeFile(file.toLocalFile(), zip);
    m_log->log(ok ? "info" : "error", ok ? QStringLiteral("report data saved to %1").arg(file.toLocalFile())
                                         : QStringLiteral("could not write %1").arg(file.toLocalFile()));
    return ok;
}

bool RunRecorder::deleteReport(const QString &jsonPath)
{
    const bool ok = QFile::remove(jsonPath);
    QFile::remove(jsonPath.chopped(5) + ".html");
    Q_EMIT reportsChanged();
    return ok;
}
