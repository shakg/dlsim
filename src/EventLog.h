// Timestamped list of everything that happened: applied changes, outages,
// socket state changes, helper messages. Shown in the Log panel.
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QUrl>

QString csvEscape(const QString &s);
bool writeTextFile(const QUrl &url, const QString &text, QString *error = nullptr);

class EventLog : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { TimeRole = Qt::UserRole + 1, CategoryRole, MessageRole };

    explicit EventLog(QObject *parent = nullptr);

    void log(const QString &category, const QString &message);
    int count() const { return int(m_entries.size()); }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();
    Q_INVOKABLE bool exportCsv(const QUrl &file);

Q_SIGNALS:
    void countChanged();
    void logged(const QString &category, const QString &message);

private:
    struct Entry {
        QDateTime time;
        QString category;
        QString message;
    };
    QList<Entry> m_entries;
};
