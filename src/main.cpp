// DLSim GUI entry point. Runs unprivileged; all tc/ip changes go through
// dlsim-helper, which is started via pkexec.
#include "EventLog.h"
#include "HelperClient.h"
#include "LinkController.h"
#include "Notifications.h"
#include "PresetStore.h"
#include "RunRecorder.h"
#include "ScenarioEngine.h"
#include "SocketMonitor.h"
#include "StatsPoller.h"
#include "SystemInfo.h"
#include "UiSettings.h"

#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QIcon>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSocketNotifier>
#include <QStyleFactory>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int g_sigFds[2] = {-1, -1};

void onSignal(int sig)
{
    const char c = char(sig);
    [[maybe_unused]] auto n = ::write(g_sigFds[0], &c, 1);
}

void installSignalHandlers(QApplication &app)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_sigFds) != 0)
        return;
    struct sigaction sa = {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (int sig : {SIGINT, SIGTERM, SIGHUP})
        sigaction(sig, &sa, nullptr);
    auto *n = new QSocketNotifier(g_sigFds[1], QSocketNotifier::Read, &app);
    QObject::connect(n, &QSocketNotifier::activated, &app, [] {
        char c;
        [[maybe_unused]] auto r = ::read(g_sigFds[1], &c, 1);
        QCoreApplication::quit(); // aboutToQuit stops the helper, which cleans up
    });
}

QPalette darkPalette()
{
    QPalette p;
    const QColor window(0x0b, 0x0c, 0x13), base(0x0b, 0x0c, 0x13), text(0xdf, 0xe2, 0xee);
    const QColor button(0x15, 0x17, 0x22), highlight(0x6e, 0xab, 0x93), mid(0x28, 0x2b, 0x3d);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(0x16, 0x18, 0x22));
    p.setColor(QPalette::ToolTipBase, button);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x67, 0x6e, 0x8c));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Light, QColor(0x3a, 0x3f, 0x56));
    p.setColor(QPalette::Midlight, mid);
    p.setColor(QPalette::Mid, mid);
    p.setColor(QPalette::Dark, QColor(0x0b, 0x0c, 0x13));
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Link, highlight);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor(0x67, 0x6e, 0x8c));
    return p;
}

// Esc in a text field leaves the field (text fields ignore Esc), so window
// shortcuts like Space = CUT work again. Not inside popups: there Esc closes.
class EscapeLeavesTextField : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() != QEvent::KeyPress || static_cast<QKeyEvent *>(ev)->key() != Qt::Key_Escape)
            return false;
        auto *window = qobject_cast<QQuickWindow *>(obj);
        auto *item = window ? window->activeFocusItem() : nullptr;
        if (!item || !item->inherits("QQuickTextInput"))
            return false;
        for (QQuickItem *p = item; p; p = p->parentItem())
            if (p->inherits("QQuickOverlay"))
                return false;
        item->setFocus(false); // focusing an ancestor would keep the field's focus
        return true;
    }
};

} // namespace

int main(int argc, char *argv[])
{
    // Qt Charts in QML needs a QApplication.
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("dlsim");
    QCoreApplication::setApplicationName("dlsim");
    QCoreApplication::setApplicationVersion(DLSIM_VERSION);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setPalette(darkPalette());
    QQuickStyle::setStyle("Fusion");
    installSignalHandlers(app);
    app.installEventFilter(new EscapeLeavesTextField(&app));

    EventLog log;
    HelperClient helper(&log);
    SystemInfo sys;
    PresetStore presets;
    LinkController link(&helper, &log, &sys);
    ScenarioEngine scenario(&link, &presets, &log);
    StatsPoller stats(&helper, &link, &log);
    SocketMonitor sockets(&helper, &log);
    UiSettings uiSettings;
    Notifications notifications;
    RunRecorder recorder(&scenario, &link, &stats, &sockets, &log);
    QObject::connect(&recorder, &RunRecorder::reportReady, &notifications, [&](const QString &name, const QString &html) {
        notifications.add("info", QStringLiteral("Run report ready: %1").arg(name),
                          QStringLiteral("Timeline, outages, recovery times and charts in one page."), {},
                          "open-report", QStringLiteral("Open report"), html);
    });

    // Failures become notifications with a cause and a fix.
    QObject::connect(&link, &LinkController::errorOccurred, &notifications,
                     [&](const QString &context, const QString &msg) { notifications.report(context, msg); });
    QObject::connect(&helper, &HelperClient::stateChanged, &notifications, [&] {
        if (helper.state() == HelperClient::Failed)
            notifications.report("helper", helper.statusText());
    });
    QObject::connect(&sockets, &SocketMonitor::failed, &notifications,
                     [&](const QString &err) { notifications.report("socket", err); });
    QObject::connect(&link, &LinkController::cutChanged, &sockets, [&] { sockets.setLinkCut(link.isCut()); });
    // NetworkManager can take a bridge port back; say so once the bridge is up.
    QObject::connect(&link, &LinkController::appliedChanged, &notifications, [&] {
        if (!link.bridgeUp())
            return;
        for (const QString &nic : {link.groundIface(), link.airIface()})
            if (sys.managedByNetworkManager(nic))
                notifications.add("warning",
                                  QStringLiteral("%1 is managed by NetworkManager and may undo the bridge").arg(nic),
                                  QStringLiteral("Stop NetworkManager from managing it while you test. "
                                                 "Undo later with: nmcli device set %1 managed yes").arg(nic),
                                  {}, "copy-command", QStringLiteral("Copy command"),
                                  QStringLiteral("nmcli device set %1 managed no").arg(nic));
    });

    qmlRegisterSingletonInstance("DLSim", 1, 0, "Log", &log);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Helper", &helper);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Sys", &sys);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Presets", &presets);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Link", &link);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Scenario", &scenario);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Stats", &stats);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Sockets", &sockets);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "UiSettings", &uiSettings);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Notifications", &notifications);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Reports", &recorder);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&] {
        scenario.stop();
        scenario.setDropoutsEnabled(false);
        sockets.stop();
        log.log("helper", "exiting: removing all impairments");
        helper.stop();
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Development aid: DLSIM_SCREENSHOT=<dir> saves a PNG of every page and exits.
    const QString shotDir = qEnvironmentVariable("DLSIM_SCREENSHOT");
    if (!shotDir.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QObject *pages = window->findChild<QObject *>("pages");
        if (qEnvironmentVariableIntValue("DLSIM_SCREENSHOT_DIALOG") == 1) {
            QTimer::singleShot(500, window, [=] {
                QMetaObject::invokeMethod(window->findChild<QObject *>("shortcutsDialog"), "open");
                QTimer::singleShot(700, window, [=] {
                    window->grabWindow().save(QDir(shotDir).filePath(QStringLiteral("dialog.png")));
                    QCoreApplication::quit();
                });
            });
            return app.exec();
        }
        auto shoot = std::make_shared<std::function<void(int)>>();
        *shoot = [=](int page) {
            if (page >= 5) {
                QCoreApplication::quit();
                return;
            }
            pages->setProperty("currentIndex", page);
            QTimer::singleShot(1500, window, [=] {
                window->grabWindow().save(QDir(shotDir).filePath(QStringLiteral("page%1.png").arg(page)));
                (*shoot)(page + 1);
            });
        };
        QTimer::singleShot(2500, window, [=] { (*shoot)(0); });
    }

    // Setup lives in the main app, so first launch follows the same helper path
    // as every later launch instead of gating it behind onboarding.
    if (qEnvironmentVariableIntValue("DLSIM_NO_HELPER") != 1)
        helper.start();
    return app.exec();
}
