// Unprivileged read-only view of the host: interfaces, default route, SSH.
// Used for the interface picker and the "you are about to cut your own
// session" warnings.
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class SystemInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList interfaces READ interfaces NOTIFY changed)
    Q_PROPERTY(QStringList interfaceNames READ interfaceNames NOTIFY changed)
    Q_PROPERTY(QStringList defaultRouteIfaces READ defaultRouteIfaces NOTIFY changed)
    Q_PROPERTY(QStringList sshIfaces READ sshIfaces NOTIFY changed)
public:
    explicit SystemInfo(QObject *parent = nullptr);

    QVariantList interfaces() const { return m_interfaces; }
    QStringList interfaceNames() const { return m_names; }
    QStringList defaultRouteIfaces() const { return m_defaultRoute; }
    QStringList sshIfaces() const { return m_ssh; }

    Q_INVOKABLE void refresh();
    // Human-readable warnings for impairing the given interfaces in the given mode.
    Q_INVOKABLE QStringList warningsFor(const QString &mode, const QString &ifaceA, const QString &ifaceB = {}) const;

Q_SIGNALS:
    void changed();

private:
    QVariantList m_interfaces;
    QStringList m_names;
    QStringList m_defaultRoute;
    QStringList m_ssh;
    QHash<QString, QStringList> m_addresses;
};
