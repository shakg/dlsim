// Errors and warnings of this session, newest first. An error stays until
// dismissed; the list keeps everything (also dismissed ones) for the session.
#pragma once

#include <QDateTime>
#include <QObject>
#include <QVariantList>

class Notifications : public QObject
{
    Q_OBJECT
    // [{id, time, level ("error"/"warning"), title, fix, action, actionLabel, actionArg, details, count, dismissed}]
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    // The newest error not dismissed yet; empty map when none. Shown as a banner.
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(int unread READ unread NOTIFY changed)
public:
    using QObject::QObject;

    QVariantList items() const;
    QVariantMap current() const;
    int unread() const { return m_unread; }

    // Explains raw (see dlsim::explainError) and adds it. A repeat of an item that
    // is still open (or was dismissed less than 10 s ago) only bumps its count.
    Q_INVOKABLE void report(const QString &context, const QString &raw, const QString &level = QStringLiteral("error"));
    // Adds an already worded item.
    void add(const QString &level, const QString &title, const QString &fix, const QString &details,
             const QString &action = {}, const QString &actionLabel = {}, const QString &actionArg = {});
    Q_INVOKABLE void dismiss(int id);
    Q_INVOKABLE void markRead();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void copyToClipboard(const QString &text);

Q_SIGNALS:
    void changed();

private:
    struct Item {
        int id;
        QDateTime time;
        QString level, title, fix, action, actionLabel, actionArg, details;
        int count = 1;
        bool dismissed = false;
    };
    QList<Item> m_items; // oldest first
    int m_nextId = 1;
    int m_unread = 0;
};
