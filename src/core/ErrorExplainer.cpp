#include "ErrorExplainer.h"

#include "Model.h"

#include <QRegularExpression>

namespace dlsim {

namespace {

// "... dev enp1s0 ..." or "Cannot find device \"enp1s0\"" -> enp1s0
QString deviceIn(const QString &raw)
{
    static const QRegularExpression dev(QStringLiteral("(?:dev|device)\\s+\"?([A-Za-z0-9_.-]{1,15})\"?"));
    const auto m = dev.match(raw);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString firstLine(const QString &raw)
{
    const QString t = raw.trimmed();
    const int nl = t.indexOf('\n');
    return nl < 0 ? t : t.left(nl);
}

} // namespace

ExplainedError explainError(const QString &context, const QString &raw)
{
    const QString r = raw.toLower();
    const QString dev = deviceIn(raw);
    const QString onDev = dev.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(dev);

    if (context == "helper") {
        if (r.contains("authentication dismissed"))
            return {QStringLiteral("The password prompt was cancelled"),
                    QStringLiteral("DLSim needs administrator rights to change traffic control settings."),
                    "retry-helper", QStringLiteral("Try again"), {}};
        if (r.contains("not authorized"))
            return {QStringLiteral("Not authorized to run the DLSim helper"),
                    QStringLiteral("Your account must be allowed to administer this PC (polkit). "
                                   "Ask an administrator, or check that the polkit policy is installed."),
                    "retry-helper", QStringLiteral("Try again"), {}};
        if (r.contains("helper not found"))
            return {QStringLiteral("The DLSim helper program is missing"),
                    QStringLiteral("Reinstall DLSim. Expected: %1").arg(raw.section(" at ", 1)), {}, {}, {}};
        if (r.contains("failed to start"))
            return {QStringLiteral("Could not start the helper"),
                    QStringLiteral("pkexec may be missing: sudo apt install pkexec"),
                    "retry-helper", QStringLiteral("Try again"), {}};
        if (r.contains("crashed") || r.contains("exited"))
            return {QStringLiteral("The helper stopped unexpectedly"),
                    QStringLiteral("Impairments were removed. Start the helper again to continue."),
                    "retry-helper", QStringLiteral("Start helper"), {}};
    }

    if (r.contains("helper is not running"))
        return {QStringLiteral("The helper is not running"),
                QStringLiteral("Start the helper, then try again."), "retry-helper", QStringLiteral("Start helper"), {}};

    if (r.contains("specified qdisc kind is unknown") || r.contains("unknown qdisc")
        || (r.contains("qdisc") && r.contains("no such file or directory"))) {
        static const QRegularExpression kindRe(QStringLiteral("\\b(netem|prio|ingress|htb|tbf)\\b"));
        // The failing command (in backticks) names the qdisc; netem is the usual one.
        static const QRegularExpression cmdRe(QStringLiteral("`([^`]*)`"));
        const auto cmd = cmdRe.match(r);
        QString kind = QStringLiteral("netem");
        const auto m = kindRe.match(cmd.hasMatch() ? cmd.captured(1) : r);
        if (m.hasMatch())
            kind = m.captured(1);
        return {QStringLiteral("The kernel has no '%1' traffic-control module").arg(kind),
                QStringLiteral("Install the extra kernel modules, then load it:\n"
                               "sudo apt install linux-modules-extra-$(uname -r) && sudo modprobe sch_%1").arg(kind),
                "copy-command", QStringLiteral("Copy command"),
                QStringLiteral("sudo apt install linux-modules-extra-$(uname -r) && sudo modprobe sch_%1").arg(kind)};
    }
    if (r.contains("cannot find device") || r.contains("does not exist") || r.contains("no such device"))
        return {QStringLiteral("Network interface%1 not found").arg(onDev),
                QStringLiteral("It was unplugged, renamed or removed. Refresh the list and pick it again on the Setup tab."),
                "refresh-interfaces", QStringLiteral("Refresh interfaces"), {}};
    if (r.contains("operation not permitted") || r.contains("permission denied"))
        return {QStringLiteral("Not allowed to change network settings%1").arg(onDev),
                QStringLiteral("The helper must run as root. Restart it and approve the password prompt."),
                "retry-helper", QStringLiteral("Restart helper"), {}};
    if (r.contains("device or resource busy") || r.contains("already enslaved") || r.contains("is already a member"))
        return {QStringLiteral("Interface%1 is in use by another bridge or bond").arg(onDev),
                QStringLiteral("Remove it from the other bridge first (ip link set %1 nomaster), then start again.")
                    .arg(dev.isEmpty() ? QStringLiteral("<nic>") : dev),
                "copy-command", QStringLiteral("Copy command"),
                QStringLiteral("sudo ip link set %1 nomaster").arg(dev.isEmpty() ? QStringLiteral("<nic>") : dev)};
    if (r.contains("timed out") || r.contains("timeout"))
        return {QStringLiteral("The helper did not answer in time"),
                QStringLiteral("The system may be busy. Try again; restart the helper if it keeps happening."),
                "retry-helper", QStringLiteral("Restart helper"), {}};

    if (context == "start")
        return {QStringLiteral("The setup is not complete"),
                QStringLiteral("Fix the fields marked in red on the Setup and Link tabs: ") + firstLine(raw), {}, {}, {}};
    if (context == "socket")
        return {QStringLiteral("Could not read TCP sockets"),
                QStringLiteral("Check the peer address and that the helper is running."), {}, {}, {}};
    if (context == "bridge")
        return {QStringLiteral("Could not create the bridge %1").arg(kBridgeName), firstLine(raw), {}, {}, {}};

    return {firstLine(raw), {}, {}, {}, {}};
}

} // namespace dlsim
