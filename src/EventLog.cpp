#include "EventLog.h"

#include <QFile>
#include <QTextStream>

namespace {
constexpr int kMaxEntries = 50000;
}

QString csvEscape(const QString &s)
{
    if (!s.contains(',') && !s.contains('"') && !s.contains('\n'))
        return s;
    QString q = s;
    q.replace('"', "\"\"");
    return '"' + q + '"';
}

bool writeTextFile(const QUrl &url, const QString &text, QString *error)
{
    QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    f.write(text.toUtf8());
    return true;
}

EventLog::EventLog(QObject *parent) : QAbstractListModel(parent) {}

void EventLog::log(const QString &category, const QString &message)
{
    if (m_entries.size() >= kMaxEntries) {
        beginRemoveRows({}, 0, 999);
        m_entries.remove(0, 1000);
        endRemoveRows();
    }
    const int row = int(m_entries.size());
    beginInsertRows({}, row, row);
    m_entries.append({QDateTime::currentDateTime(), category, message});
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT logged(category, message);
}

int EventLog::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant EventLog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries[index.row()];
    switch (role) {
    case TimeRole: return e.time.toString("HH:mm:ss.zzz");
    case CategoryRole: return e.category;
    case MessageRole:
    case Qt::DisplayRole: return e.message;
    }
    return {};
}

QHash<int, QByteArray> EventLog::roleNames() const
{
    return {{TimeRole, "time"}, {CategoryRole, "category"}, {MessageRole, "message"}};
}

void EventLog::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
}

bool EventLog::exportCsv(const QUrl &file)
{
    QString out = "timestamp,category,message\n";
    for (const Entry &e : m_entries)
        out += e.time.toString(Qt::ISODateWithMs) + ',' + csvEscape(e.category) + ',' + csvEscape(e.message) + '\n';
    QString err;
    if (!writeTextFile(file, out, &err)) {
        log("error", QStringLiteral("log export failed: %1").arg(err));
        return false;
    }
    log("info", QStringLiteral("log exported to %1").arg(file.toLocalFile()));
    return true;
}
