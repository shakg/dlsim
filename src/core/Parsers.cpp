#include "Parsers.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace dlsim {

namespace {

quint64 u64(const QJsonObject &o, const char *k)
{
    const double d = o.value(QLatin1String(k)).toDouble(0);
    return d > 0 ? quint64(d) : 0;
}

} // namespace

QJsonObject QdiscStats::toJson() const
{
    return QJsonObject{
        {"dev", dev}, {"kind", kind}, {"handle", handle}, {"parent", parent}, {"root", root},
        {"bytes", double(bytes)}, {"packets", double(packets)}, {"drops", double(drops)},
        {"overlimits", double(overlimits)}, {"requeues", double(requeues)},
        {"backlog", double(backlog)}, {"qlen", double(qlen)},
    };
}

QdiscStats QdiscStats::fromJson(const QJsonObject &o)
{
    QdiscStats s;
    s.dev = o.value("dev").toString();
    s.kind = o.value("kind").toString();
    s.handle = o.value("handle").toString();
    s.parent = o.value("parent").toString();
    s.root = o.value("root").toBool();
    s.bytes = u64(o, "bytes");
    s.packets = u64(o, "packets");
    s.drops = u64(o, "drops");
    s.overlimits = u64(o, "overlimits");
    s.requeues = u64(o, "requeues");
    s.backlog = u64(o, "backlog");
    s.qlen = u64(o, "qlen");
    return s;
}

bool parseQdiscStats(const QByteArray &json, QList<QdiscStats> *out, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        if (error)
            *error = pe.error != QJsonParseError::NoError ? pe.errorString() : QStringLiteral("expected a JSON array");
        return false;
    }
    out->clear();
    for (const auto &v : doc.array())
        out->append(QdiscStats::fromJson(v.toObject()));
    return true;
}

// ---------------------------------------------------------------- ss

QJsonObject TcpSocketInfo::toJson() const
{
    return QJsonObject{
        {"state", state}, {"recvQ", double(recvQ)}, {"sendQ", double(sendQ)},
        {"local", local}, {"peer", peer},
        {"timer", timer}, {"timerMs", timerMs}, {"timerRetrans", timerRetrans},
        {"rtoMs", rtoMs}, {"rttMs", rttMs}, {"rttVarMs", rttVarMs},
        {"retransCur", retransCur}, {"retransTotal", retransTotal},
        {"backoff", backoff}, {"unacked", unacked}, {"cwnd", cwnd},
        {"lastSndMs", double(lastSndMs)}, {"lastRcvMs", double(lastRcvMs)}, {"lastAckMs", double(lastAckMs)},
    };
}

TcpSocketInfo TcpSocketInfo::fromJson(const QJsonObject &o)
{
    TcpSocketInfo s;
    s.state = o.value("state").toString();
    s.recvQ = quint32(o.value("recvQ").toDouble());
    s.sendQ = quint32(o.value("sendQ").toDouble());
    s.local = o.value("local").toString();
    s.peer = o.value("peer").toString();
    s.timer = o.value("timer").toString();
    s.timerMs = o.value("timerMs").toDouble(-1);
    s.timerRetrans = o.value("timerRetrans").toInt(-1);
    s.rtoMs = o.value("rtoMs").toDouble(-1);
    s.rttMs = o.value("rttMs").toDouble(-1);
    s.rttVarMs = o.value("rttVarMs").toDouble(-1);
    s.retransCur = o.value("retransCur").toInt(-1);
    s.retransTotal = o.value("retransTotal").toInt(-1);
    s.backoff = o.value("backoff").toInt(0);
    s.unacked = o.value("unacked").toInt(0);
    s.cwnd = o.value("cwnd").toInt(-1);
    s.lastSndMs = qint64(o.value("lastSndMs").toDouble(-1));
    s.lastRcvMs = qint64(o.value("lastRcvMs").toDouble(-1));
    s.lastAckMs = qint64(o.value("lastAckMs").toDouble(-1));
    return s;
}

double parseSsDuration(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^(?:(\\d+)min)?(?:([0-9.]+)(sec|ms))?$"));
    const auto m = re.match(s.trimmed());
    if (!m.hasMatch() || s.trimmed().isEmpty())
        return -1;
    double total = 0;
    if (m.capturedLength(1))
        total += m.captured(1).toDouble() * 60000.0;
    if (m.capturedLength(2)) {
        bool ok = false;
        const double v = m.captured(2).toDouble(&ok);
        if (!ok)
            return -1;
        total += m.captured(3) == "sec" ? v * 1000.0 : v;
    }
    return total;
}

namespace {

void parseTimer(const QString &tok, TcpSocketInfo &s)
{
    // timer:(on,1.204sec,3)
    QString inner = tok.mid(int(qstrlen("timer:(")));
    if (inner.endsWith(')'))
        inner.chop(1);
    const QStringList parts = inner.split(',');
    if (parts.size() >= 1)
        s.timer = parts[0];
    if (parts.size() >= 2)
        s.timerMs = parseSsDuration(parts[1]);
    if (parts.size() >= 3)
        s.timerRetrans = parts[2].toInt();
}

void parseInfo(const QString &line, TcpSocketInfo &s)
{
    const QStringList toks = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &t : toks) {
        const int colon = t.indexOf(':');
        if (colon <= 0)
            continue;
        const QString k = t.left(colon);
        const QString v = t.mid(colon + 1);
        if (k == "rto") {
            s.rtoMs = v.toDouble();
        } else if (k == "rtt") {
            const QStringList p = v.split('/');
            s.rttMs = p.value(0).toDouble();
            if (p.size() > 1)
                s.rttVarMs = p[1].toDouble();
        } else if (k == "retrans") {
            const QStringList p = v.split('/');
            s.retransCur = p.value(0).toInt();
            s.retransTotal = p.size() > 1 ? p[1].toInt() : s.retransCur;
        } else if (k == "backoff") {
            s.backoff = v.toInt();
        } else if (k == "unacked") {
            s.unacked = v.toInt();
        } else if (k == "cwnd") {
            s.cwnd = v.toInt();
        } else if (k == "lastsnd") {
            s.lastSndMs = v.toLongLong();
        } else if (k == "lastrcv") {
            s.lastRcvMs = v.toLongLong();
        } else if (k == "lastack") {
            s.lastAckMs = v.toLongLong();
        } else if (k == "timer" && v.startsWith('(')) {
            parseTimer(t, s);
        }
    }
}

} // namespace

QList<TcpSocketInfo> parseSs(const QString &text)
{
    QList<TcpSocketInfo> out;
    const QStringList lines = text.split('\n');
    for (const QString &raw : lines) {
        if (raw.trimmed().isEmpty())
            continue;
        const bool continuation = raw.front().isSpace();
        if (continuation) {
            if (!out.isEmpty())
                parseInfo(raw, out.last());
            continue;
        }
        const QStringList toks = raw.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (toks.size() < 5 || toks[0] == "State")
            continue;
        TcpSocketInfo s;
        s.state = toks[0];
        s.recvQ = toks[1].toUInt();
        s.sendQ = toks[2].toUInt();
        s.local = toks[3];
        s.peer = toks[4];
        // Remaining tokens: timer:(...) and possibly the info on the same line.
        parseInfo(QStringList(toks.mid(5)).join(' '), s);
        out << s;
    }
    return out;
}

} // namespace dlsim
