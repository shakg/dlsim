#include "StatsPoller.h"

#include "EventLog.h"
#include "HelperClient.h"
#include "LinkController.h"

#include <QJsonObject>
#include <QtCharts/QXYSeries>

namespace {
constexpr int kMaxSamples = 24 * 3600;
const char *const kDirs[2] = {"uplink", "downlink"};
} // namespace

StatsPoller::StatsPoller(HelperClient *helper, LinkController *link, EventLog *log, QObject *parent)
    : QObject(parent), m_helper(helper), m_link(link), m_log(log)
{
    m_clock.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &StatsPoller::poll);
    m_timer.start();
}

void StatsPoller::setWindowSeconds(int s)
{
    s = std::clamp(s, 10, 3600);
    if (s == m_window)
        return;
    m_window = s;
    Q_EMIT windowChanged();
}

void StatsPoller::poll()
{
    if (!m_helper->isRunning() || !m_link->isActive() || m_inFlight)
        return;
    m_inFlight = true;
    m_helper->request("read_stats", {}, [this](bool ok, const QJsonObject &res, const QString &err) {
        m_inFlight = false;
        if (!ok) {
            if (err != m_lastError)
                m_log->log("error", QStringLiteral("stats: %1").arg(err));
            m_lastError = err;
            return;
        }
        m_lastError.clear();
        if (!res.value("active").toBool())
            return;
        Sample s;
        s.t = now();
        s.cut = res.value("cut").toBool();
        const Sample *prev = m_samples.isEmpty() ? nullptr : &m_samples.last();
        for (int d = 0; d < 2; ++d) {
            const QJsonObject total = res.value(kDirs[d]).toObject().value("total").toObject();
            Counters &c = s.c[d];
            c.valid = !total.isEmpty();
            c.bytes = total.value("bytes").toDouble();
            c.packets = total.value("packets").toDouble();
            c.drops = total.value("drops").toDouble();
            c.overlimits = total.value("overlimits").toDouble();
            c.backlog = total.value("backlog").toDouble();
            c.qlen = total.value("qlen").toDouble();
            Rates &r = s.r[d];
            r.backlog = c.backlog;
            r.qlen = c.qlen;
            if (prev && prev->c[d].valid && c.valid) {
                const double dt = std::max(0.001, s.t - prev->t);
                auto rate = [&](double now, double before) {
                    // Counters restart from zero when the qdisc tree is rebuilt.
                    return now >= before ? (now - before) / dt : 0.0;
                };
                r.kbps = rate(c.bytes, prev->c[d].bytes) * 8.0 / 1000.0;
                r.pps = rate(c.packets, prev->c[d].packets);
                r.dropsPerSec = rate(c.drops, prev->c[d].drops);
                r.overlimitsPerSec = rate(c.overlimits, prev->c[d].overlimits);
            }
            m_latest[d] = QVariantMap{
                {"dev", res.value(kDirs[d]).toObject().value("dev").toString()},
                {"kbps", r.kbps}, {"pps", r.pps}, {"dropsPerSec", r.dropsPerSec},
                {"overlimitsPerSec", r.overlimitsPerSec}, {"backlog", r.backlog}, {"qlen", r.qlen},
                {"bytes", c.bytes}, {"packets", c.packets}, {"drops", c.drops}, {"overlimits", c.overlimits},
            };
        }
        m_samples << s;
        if (m_samples.size() > kMaxSamples)
            m_samples.remove(0, m_samples.size() - kMaxSamples);
        Q_EMIT sampled();
    });
}

double StatsPoller::value(const Sample &s, const QString &key)
{
    const int d = key.startsWith("up.") ? 0 : 1;
    const QString k = key.section('.', 1);
    const Rates &r = s.r[d];
    if (k == "kbps") return r.kbps;
    if (k == "pps") return r.pps;
    if (k == "dropsPerSec") return r.dropsPerSec;
    if (k == "overlimitsPerSec") return r.overlimitsPerSec;
    if (k == "backlog") return r.backlog / 1024.0; // KiB
    if (k == "qlen") return r.qlen;
    return 0;
}

QVariantMap StatsPoller::updateSeries(QAbstractSeries *series, const QString &key) const
{
    const double xmax = std::max(now(), double(m_window));
    const double xmin = xmax - m_window;
    double ymax = 0;
    QList<QPointF> pts;
    for (const Sample &s : m_samples) {
        if (s.t < xmin)
            continue;
        const double v = value(s, key);
        ymax = std::max(ymax, v);
        pts << QPointF(s.t, v);
    }
    if (auto *xy = qobject_cast<QXYSeries *>(series))
        xy->replace(pts);
    return {{"xmin", xmin}, {"xmax", xmax}, {"ymax", ymax}};
}

void StatsPoller::clear()
{
    m_samples.clear();
    Q_EMIT sampled();
}

bool StatsPoller::exportCsv(const QUrl &file)
{
    QString out = "t_s,cut";
    for (const char *d : {"up", "down"})
        for (const char *k : {"bytes", "packets", "drops", "overlimits", "backlog_bytes", "qlen", "kbps", "pps", "drops_per_s"})
            out += QStringLiteral(",%1_%2").arg(d, k);
    out += '\n';
    for (const Sample &s : m_samples) {
        out += QString::number(s.t, 'f', 3) + ',' + (s.cut ? "1" : "0");
        for (int d = 0; d < 2; ++d) {
            const Counters &c = s.c[d];
            const Rates &r = s.r[d];
            for (double v : {c.bytes, c.packets, c.drops, c.overlimits, c.backlog, c.qlen})
                out += ',' + QString::number(v, 'f', 0);
            for (double v : {r.kbps, r.pps, r.dropsPerSec})
                out += ',' + QString::number(v, 'f', 2);
        }
        out += '\n';
    }
    QString err;
    if (!writeTextFile(file, out, &err)) {
        m_log->log("error", QStringLiteral("stats export failed: %1").arg(err));
        return false;
    }
    m_log->log("info", QStringLiteral("stats exported to %1 (%2 samples)").arg(file.toLocalFile()).arg(m_samples.size()));
    return true;
}
