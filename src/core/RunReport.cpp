#include "RunReport.h"

#include <QJsonArray>

#include <algorithm>
#include <functional>

namespace dlsim {

namespace {

QString esc(const QString &s) { return s.toHtmlEscaped(); }
QString num(double v, int prec = 1) { return QString::number(v, 'f', prec); }
QString csv(const QString &s)
{
    return s.contains(',') || s.contains('"') || s.contains('\n') ? '"' + QString(s).replace('"', "\"\"") + '"' : s;
}
QString dur(double s)
{
    const int m = int(s) / 60;
    return m > 0 ? QStringLiteral("%1:%2").arg(m).arg(s - 60 * m, 4, 'f', 1, QLatin1Char('0')) : num(s) + " s";
}

QString stepLabel(const QJsonObject &s)
{
    const QString t = s.value("type").toString();
    if (t == "outage") return QStringLiteral("Drop link");
    if (t == "dropouts") return QStringLiteral("Random dropouts");
    if (t == "fade") return "Gradually change to " + s.value("preset").toString();
    return "Hold " + s.value("preset").toString();
}

constexpr int kW = 900;

// Timeline: step blocks with outages marked underneath.
QString timelineSvg(const QJsonObject &r)
{
    const double total = std::max(1.0, r.value("durationS").toDouble());
    QString svg = QStringLiteral("<svg viewBox='0 0 %1 74' width='100%' role='img' aria-label='Timeline'>").arg(kW);
    for (const auto &v : r.value("steps").toArray()) {
        const QJsonObject s = v.toObject();
        const double x = s.value("startS").toDouble() / total * kW, w = std::max(1.0, (s.value("endS").toDouble() - s.value("startS").toDouble()) / total * kW);
        const QString t = s.value("type").toString();
        const QString fill = t == "outage" ? "#e53935" : t == "fade" ? "#8e5ea2" : t == "dropouts" ? "#9c7a12" : "#2f4b6e";
        svg += QStringLiteral("<rect x='%1' y='0' width='%2' height='36' fill='%3' stroke='#1e2126'><title>%4 · %5</title></rect>")
                   .arg(num(x)).arg(num(w)).arg(fill, esc(stepLabel(s)), dur(s.value("endS").toDouble() - s.value("startS").toDouble()));
        if (w > 60)
            svg += QStringLiteral("<text x='%1' y='22' fill='#fff' font-size='11'>%2</text>").arg(num(x + 4), esc(stepLabel(s).left(int(w / 7))));
    }
    for (const auto &v : r.value("outages").toArray()) {
        const QJsonObject o = v.toObject();
        const double x = o.value("startS").toDouble() / total * kW, w = std::max(2.0, (o.value("endS").toDouble() - o.value("startS").toDouble()) / total * kW);
        svg += QStringLiteral("<rect x='%1' y='42' width='%2' height='10' fill='#ef5350'><title>outage %3</title></rect>")
                   .arg(num(x)).arg(num(w)).arg(dur(o.value("endS").toDouble() - o.value("startS").toDouble()));
    }
    svg += QStringLiteral("<text x='0' y='70' fill='#888' font-size='10'>0</text><text x='%1' y='70' fill='#888' font-size='10' text-anchor='end'>%2</text>")
               .arg(kW).arg(dur(total));
    return svg + "</svg>";
}

// Line chart of several series from rows[key].
QString chartSvg(const QString &title, const QJsonArray &rows, const QStringList &keys, const QStringList &names,
                 const QStringList &colors, double total, const QJsonArray &outages)
{
    const int h = 140, pad = 24;
    double ymax = 0;
    for (const auto &v : rows)
        for (const QString &k : keys)
            ymax = std::max(ymax, v.toObject().value(k).toDouble());
    if (ymax <= 0)
        ymax = 1;
    QString svg = QStringLiteral("<h3>%1</h3><svg viewBox='0 0 %2 %3' width='100%' role='img' aria-label='%1'>")
                      .arg(esc(title)).arg(kW).arg(h + pad);
    for (const auto &v : outages) { // shade outages
        const QJsonObject o = v.toObject();
        svg += QStringLiteral("<rect x='%1' y='0' width='%2' height='%3' fill='#ef5350' opacity='0.15'/>")
                   .arg(num(o.value("startS").toDouble() / total * kW))
                   .arg(num(std::max(1.0, (o.value("endS").toDouble() - o.value("startS").toDouble()) / total * kW)))
                   .arg(h);
    }
    svg += QStringLiteral("<line x1='0' y1='%1' x2='%2' y2='%1' stroke='#555'/>").arg(h).arg(kW);
    for (int i = 0; i < keys.size(); ++i) {
        QStringList pts;
        for (const auto &v : rows) {
            const QJsonObject p = v.toObject();
            pts << num(p.value("t").toDouble() / total * kW) + ',' + num(h - p.value(keys[i]).toDouble() / ymax * (h - 4));
        }
        svg += QStringLiteral("<polyline fill='none' stroke='%1' stroke-width='1.5' points='%2'/>").arg(colors[i], pts.join(' '));
        svg += QStringLiteral("<text x='%1' y='%2' fill='%3' font-size='11'>%4</text>").arg(4 + i * 130).arg(h + 16).arg(colors[i], esc(names[i]));
    }
    svg += QStringLiteral("<text x='%1' y='12' fill='#888' font-size='10' text-anchor='end'>max %2</text>").arg(kW - 2).arg(num(ymax, 2));
    return svg + "</svg>";
}

} // namespace

QString renderReportHtml(const QJsonObject &r)
{
    const QJsonObject cfg = r.value("config").toObject();
    const double total = std::max(1.0, r.value("durationS").toDouble());
    QString h;
    h += QStringLiteral("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                        "<meta name='viewport' content='width=device-width,initial-scale=1'><title>%1 · DLSim run report</title>"
                        "<style>body{font:14px/1.45 system-ui,sans-serif;margin:24px auto;max-width:960px;padding:0 16px;"
                        "background:#fff;color:#1e2126}h1{margin-bottom:0}.sub{color:#666}table{border-collapse:collapse;"
                        "margin:8px 0}td,th{border-bottom:1px solid #ddd;padding:4px 10px;text-align:left}th{color:#555;font-weight:600}"
                        ".bad{color:#c62828}.ok{color:#2e7d32}svg{background:#fafafa;border:1px solid #eee;margin:4px 0}"
                        "@media(prefers-color-scheme:dark){body{background:#1e2126;color:#e6e8eb}td,th{border-color:#3a3f47}"
                        "th,.sub{color:#9aa1ab}svg{background:#16181c;border-color:#3a3f47}}</style></head><body>")
             .arg(esc(r.value("name").toString()));
    h += QStringLiteral("<h1>%1</h1><p class='sub'>DLSim run report · %2 → %3 · %4 · %5</p>")
             .arg(esc(r.value("name").toString()), esc(r.value("startedAt").toString()), esc(r.value("endedAt").toString()),
                  dur(total), r.value("completed").toBool() ? "completed" : "stopped early");
    h += "<h2>Setup</h2><table>";
    const auto row = [&](const QString &k, const QString &v) { h += "<tr><th>" + esc(k) + "</th><td>" + esc(v) + "</td></tr>"; };
    row("Mode", cfg.value("mode").toString() == "host" ? "Host (this PC)" : "Bridge (between two devices)");
    row("Ports", cfg.value("ports").toString());
    row("Starting condition", cfg.value("preset").toString());
    QStringList classes;
    for (const auto &c : cfg.value("classes").toArray())
        classes << c.toString();
    row("Traffic classes", classes.isEmpty() ? QStringLiteral("none: all traffic uses the link profile") : classes.join("; "));
    h += "</table>";

    h += "<h2>Timeline</h2>" + timelineSvg(r);
    h += "<table><tr><th>#</th><th>Step</th><th>Planned</th><th>From</th><th>To</th></tr>";
    for (const auto &v : r.value("steps").toArray()) {
        const QJsonObject s = v.toObject();
        h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                 .arg(s.value("index").toInt() + 1).arg(esc(stepLabel(s)), dur(s.value("plannedS").toDouble()),
                      dur(s.value("startS").toDouble()), dur(s.value("endS").toDouble()));
    }
    h += "</table>";

    const QJsonArray outages = r.value("outages").toArray();
    h += "<h2>Outages</h2>";
    if (outages.isEmpty()) {
        h += "<p>No outages during this run.</p>";
    } else {
        h += "<table><tr><th>Start</th><th>Length</th><th>Cause</th><th>Packets dropped (" + esc(cfg.value("groundName").toString())
             + " → " + esc(cfg.value("vehicleName").toString()) + " / back)</th><th>Recovery after restore</th></tr>";
        for (const auto &v : outages) {
            const QJsonObject o = v.toObject();
            QStringList rec;
            for (const auto &x : o.value("recoveries").toArray()) {
                const QJsonObject rr = x.toObject();
                rec << QStringLiteral("%1: <b>%2</b>").arg(esc(rr.value("socket").toString()), dur(rr.value("seconds").toDouble()));
            }
            QStringList src;
            for (const auto &x : o.value("sources").toArray())
                src << x.toString();
            h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4 / %5</td><td>%6</td></tr>")
                     .arg(dur(o.value("startS").toDouble()), dur(o.value("endS").toDouble() - o.value("startS").toDouble()),
                          esc(src.join(", ")))
                     .arg(qint64(o.value("dropsUp").toDouble())).arg(qint64(o.value("dropsDown").toDouble()))
                     .arg(rec.isEmpty() ? QStringLiteral("<span class='sub'>no tracked TCP socket recovered</span>") : rec.join("<br>"));
        }
        h += "</table>";
    }

    h += "<h2>Charts</h2>";
    const QJsonArray samples = r.value("samples").toArray();
    h += chartSvg("Throughput (kbit/s)", samples, {"upKbps", "downKbps"}, {"uplink", "downlink"}, {"#0288d1", "#ef6c00"}, total, outages);
    h += chartSvg("Drops (packets/s)", samples, {"upDropsPerS", "downDropsPerS"}, {"uplink", "downlink"}, {"#0288d1", "#ef6c00"}, total, outages);
    QJsonArray sock;
    for (const auto &v : r.value("sockets").toArray()) {
        QJsonObject p = v.toObject();
        p["rtoS"] = p.value("rtoMs").toDouble() / 1000.0;
        sock.append(p);
    }
    h += chartSvg("TCP retransmit timeout (s) and backoff", sock, {"rtoS", "backoff"}, {"RTO s", "backoff"}, {"#c62828", "#f9a825"}, total, outages);

    h += "<details><summary>Event log (" + QString::number(r.value("log").toArray().size()) + " entries)</summary><table>";
    for (const auto &v : r.value("log").toArray()) {
        const QJsonObject l = v.toObject();
        h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                 .arg(dur(l.value("t").toDouble()), esc(l.value("category").toString()), esc(l.value("message").toString()));
    }
    h += "</table></details><p class='sub'>Generated by DLSim.</p></body></html>\n";
    return h;
}

QString reportStatsCsv(const QJsonObject &r)
{
    QString out = "t_s,up_kbps,down_kbps,up_drops_per_s,down_drops_per_s,up_drops_total,down_drops_total\n";
    for (const auto &v : r.value("samples").toArray()) {
        const QJsonObject s = v.toObject();
        out += QStringList{num(s.value("t").toDouble(), 3), num(s.value("upKbps").toDouble(), 1), num(s.value("downKbps").toDouble(), 1),
                           num(s.value("upDropsPerS").toDouble(), 2), num(s.value("downDropsPerS").toDouble(), 2),
                           QString::number(qint64(s.value("upDrops").toDouble())), QString::number(qint64(s.value("downDrops").toDouble()))}
                   .join(',') + '\n';
    }
    return out;
}

QString reportSocketsCsv(const QJsonObject &r)
{
    QString out = "t_s,socket,state,rto_ms,backoff,retrans,status\n";
    for (const auto &v : r.value("sockets").toArray()) {
        const QJsonObject s = v.toObject();
        out += QStringList{num(s.value("t").toDouble(), 3), csv(s.value("socket").toString()), s.value("state").toString(),
                           QString::number(s.value("rtoMs").toDouble()), QString::number(s.value("backoff").toInt()),
                           QString::number(s.value("retrans").toInt()), csv(s.value("status").toString())}
                   .join(',') + '\n';
    }
    return out;
}

QString reportLogCsv(const QJsonObject &r)
{
    QString out = "t_s,category,message\n";
    for (const auto &v : r.value("log").toArray()) {
        const QJsonObject l = v.toObject();
        out += num(l.value("t").toDouble(), 3) + ',' + csv(l.value("category").toString()) + ',' + csv(l.value("message").toString()) + '\n';
    }
    return out;
}

} // namespace dlsim
