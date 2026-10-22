// Parsers for `tc -s -j qdisc show` and `ss -tnoiH` output. Pure, testable.
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace dlsim {

struct QdiscStats {
    QString dev, kind, handle, parent;
    bool root = false;
    quint64 bytes = 0, packets = 0, drops = 0, overlimits = 0, requeues = 0, backlog = 0, qlen = 0;

    QJsonObject toJson() const;
    static QdiscStats fromJson(const QJsonObject &o);
};

// Returns false and sets *error on malformed JSON.
bool parseQdiscStats(const QByteArray &json, QList<QdiscStats> *out, QString *error = nullptr);

struct TcpSocketInfo {
    QString state;      // ESTAB, SYN-SENT, ...
    quint32 recvQ = 0, sendQ = 0;
    QString local, peer;
    QString timer;      // on, keepalive, persist, timewait, ... or empty
    double timerMs = -1; // remaining time on the timer
    int timerRetrans = -1;
    double rtoMs = -1, rttMs = -1, rttVarMs = -1;
    int retransCur = -1, retransTotal = -1;
    int backoff = 0;    // ss prints it only when non-zero
    int unacked = 0;
    int cwnd = -1;
    qint64 lastSndMs = -1, lastRcvMs = -1, lastAckMs = -1;

    QString key() const { return local + "->" + peer; }
    QJsonObject toJson() const;
    static TcpSocketInfo fromJson(const QJsonObject &o);
};

// Parse ss output produced with -t -n -o -i -H. Info lines (indented) belong
// to the socket line above them.
QList<TcpSocketInfo> parseSs(const QString &text);

// "200ms", "1.204sec", "1min12sec", "2min" -> milliseconds; -1 on failure.
double parseSsDuration(const QString &s);

} // namespace dlsim
