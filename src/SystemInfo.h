// Unprivileged read-only view of the host: interfaces, default route, SSH.
// Used for the interface picker and the "you are about to cut your own
// session" warnings.
#pragma once

#include "NicInfo.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QVariantList>

class SystemInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList interfaces READ interfaces NOTIFY changed)
    Q_PROPERTY(QStringList interfaceNames READ interfaceNames NOTIFY changed)
    Q_PROPERTY(QStringList defaultRouteIfaces READ defaultRouteIfaces NOTIFY changed)
    Q_PROPERTY(QStringList sshIfaces READ sshIfaces NOTIFY changed)
    // name -> {carrier (1/0/-1), speedMbps, rxKbps, txKbps, physical}; 1 Hz from /sys, no root.
    Q_PROPERTY(QVariantMap live READ live NOTIFY liveChanged)
    // Unplug-to-identify: "idle", "waiting", "found", "ambiguous".
    Q_PROPERTY(QString identifyState READ identifyState NOTIFY identifyChanged)
    Q_PROPERTY(QString identified READ identified NOTIFY identifyChanged)
    Q_PROPERTY(QStringList identifyWatched READ identifyWatched NOTIFY identifyChanged)
    Q_PROPERTY(QStringList identifyLost READ identifyLost NOTIFY identifyChanged)
public:
    explicit SystemInfo(QObject *parent = nullptr);

    QVariantList interfaces() const { return m_interfaces; }
    QStringList interfaceNames() const { return m_names; }
    QStringList defaultRouteIfaces() const { return m_defaultRoute; }
    QStringList sshIfaces() const { return m_ssh; }
    QVariantMap live() const { return m_live; }
    QString identifyState() const;
    QString identified() const { return m_unplug.found(); }
    QStringList identifyWatched() const { return m_unplug.watched(); }
    QStringList identifyLost() const { return m_unplug.lost(); }
    // Start watching every NIC with a link; the one that loses it is the one unplugged.
    // candidates: the ports that may be the one (the others are ignored).
    Q_INVOKABLE void startIdentify(const QStringList &candidates);
    Q_INVOKABLE void cancelIdentify();

    Q_INVOKABLE void refresh();
    // Human-readable warnings for impairing the given interfaces in the given mode.
    Q_INVOKABLE QStringList warningsFor(const QString &mode, const QString &ifaceA, const QString &ifaceB = {}) const;
    // True if NetworkManager manages the interface (it may undo DLSim's bridge).
    Q_INVOKABLE bool managedByNetworkManager(const QString &iface) const { return m_nmManaged.contains(iface); }

Q_SIGNALS:
    void changed();
    void liveChanged();
    void identifyChanged();

private:
    QVariantList m_interfaces;
    QStringList m_names;
    QStringList m_defaultRoute;
    QStringList m_ssh;
    QHash<QString, QStringList> m_addresses;
    QStringList m_nmManaged;
    void sample();
    QTimer m_sampleTimer;
    QElapsedTimer m_sampleClock;
    QHash<QString, dlsim::NicSample> m_last;
    QVariantMap m_live;
    dlsim::UnplugDetector m_unplug;
};
