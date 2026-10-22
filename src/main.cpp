// DLSim GUI entry point. Runs unprivileged; all tc/ip changes go through
// dlsim-helper, which is started via pkexec.
#include "EventLog.h"
#include "HelperClient.h"
#include "LinkController.h"
#include "PresetStore.h"
#include "ScenarioEngine.h"
#include "SocketMonitor.h"
#include "StatsPoller.h"
#include "SystemInfo.h"

#include <QApplication>
#include <QDir>
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
    const QColor window(0x1e, 0x21, 0x26), base(0x16, 0x18, 0x1c), text(0xe6, 0xe8, 0xeb);
    const QColor button(0x2a, 0x2e, 0x35), highlight(0x3d, 0x8b, 0xfd), mid(0x3a, 0x3f, 0x47);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, button);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x86, 0x90));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Light, QColor(0x45, 0x4b, 0x55));
    p.setColor(QPalette::Midlight, mid);
    p.setColor(QPalette::Mid, mid);
    p.setColor(QPalette::Dark, QColor(0x12, 0x14, 0x17));
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Link, highlight);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor(0x6c, 0x72, 0x7c));
    return p;
}

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

    EventLog log;
    HelperClient helper(&log);
    SystemInfo sys;
    PresetStore presets;
    LinkController link(&helper, &log, &sys);
    ScenarioEngine scenario(&link, &presets, &log);
    StatsPoller stats(&helper, &link, &log);
    SocketMonitor sockets(&helper, &log);

    qmlRegisterSingletonInstance("DLSim", 1, 0, "Log", &log);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Helper", &helper);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Sys", &sys);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Presets", &presets);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Link", &link);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Scenario", &scenario);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Stats", &stats);
    qmlRegisterSingletonInstance("DLSim", 1, 0, "Sockets", &sockets);

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
        auto shoot = std::make_shared<std::function<void(int)>>();
        *shoot = [=](int page) {
            if (page >= 4) {
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

    if (qEnvironmentVariableIntValue("DLSIM_NO_HELPER") != 1)
        helper.start();
    return app.exec();
}
