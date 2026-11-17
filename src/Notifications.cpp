#include "Notifications.h"

#include "ErrorExplainer.h"

#include <QClipboard>
#include <QGuiApplication>

namespace {

QVariantMap toMap(int id, const QDateTime &time, const QString &level, const QString &title, const QString &fix,
                  const QString &action, const QString &actionLabel, const QString &actionArg, const QString &details,
                  int count, bool dismissed)
{
    return {{"id", id}, {"time", time.toString("HH:mm:ss")}, {"level", level}, {"title", title}, {"fix", fix},
            {"action", action}, {"actionLabel", actionLabel}, {"actionArg", actionArg}, {"details", details},
            {"count", count}, {"dismissed", dismissed}};
}

} // namespace

QVariantList Notifications::items() const
{
    QVariantList out;
    for (auto it = m_items.crbegin(); it != m_items.crend(); ++it)
        out << toMap(it->id, it->time, it->level, it->title, it->fix, it->action, it->actionLabel, it->actionArg,
                     it->details, it->count, it->dismissed);
    return out;
}

QVariantMap Notifications::current() const
{
    for (auto it = m_items.crbegin(); it != m_items.crend(); ++it)
        if (!it->dismissed && it->level == "error")
            return toMap(it->id, it->time, it->level, it->title, it->fix, it->action, it->actionLabel, it->actionArg,
                         it->details, it->count, it->dismissed);
    return {};
}

void Notifications::report(const QString &context, const QString &raw, const QString &level)
{
    const dlsim::ExplainedError e = dlsim::explainError(context, raw);
    add(level, e.title, e.fix, raw.trimmed(), e.action, e.actionLabel, e.actionArg);
}

void Notifications::add(const QString &level, const QString &title, const QString &fix, const QString &details,
                        const QString &action, const QString &actionLabel, const QString &actionArg)
{
    const QDateTime now = QDateTime::currentDateTime();
    // The same problem again: bump the open item (or one dismissed in the last 10 s).
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it) {
        if (it->title != title || it->level != level)
            continue;
        if (!it->dismissed || it->time.secsTo(now) < 10) {
            Item same = *it;
            m_items.erase(std::next(it).base());
            ++same.count;
            same.time = now;
            same.details = details;
            same.dismissed = false;
            m_items << same; // newest again
            ++m_unread;
            Q_EMIT changed();
            return;
        }
        break;
    }
    m_items << Item{m_nextId++, now, level, title, fix, action, actionLabel, actionArg, details};
    if (m_items.size() > 200)
        m_items.removeFirst();
    ++m_unread;
    Q_EMIT changed();
}

void Notifications::dismiss(int id)
{
    for (Item &i : m_items)
        if (i.id == id && !i.dismissed) {
            i.dismissed = true;
            Q_EMIT changed();
            return;
        }
}

void Notifications::markRead()
{
    if (m_unread == 0)
        return;
    m_unread = 0;
    Q_EMIT changed();
}

void Notifications::clear()
{
    m_items.clear();
    m_unread = 0;
    Q_EMIT changed();
}

void Notifications::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}
