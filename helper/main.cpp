// dlsim-helper: runs as root (via pkexec), applies tc/ip changes on request
// from the unprivileged GUI. Cleans up on stdin EOF, SIGINT, SIGTERM, SIGHUP.
#include "HelperServer.h"

#include <QCoreApplication>
#include <QSocketNotifier>

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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("dlsim-helper");
    QCoreApplication::setApplicationVersion(DLSIM_VERSION);

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_sigFds) != 0) {
        qCritical("socketpair failed");
        return 1;
    }
    struct sigaction sa = {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (int sig : {SIGINT, SIGTERM, SIGHUP})
        sigaction(sig, &sa, nullptr);
    std::signal(SIGPIPE, SIG_IGN); // GUI gone: write() fails, stdin EOF triggers cleanup

    HelperServer server;
    QSocketNotifier sigNotifier(g_sigFds[1], QSocketNotifier::Read);
    QObject::connect(&sigNotifier, &QSocketNotifier::activated, &server, [&] {
        char c = 0;
        [[maybe_unused]] auto n = ::read(g_sigFds[1], &c, 1);
        server.shutdown(QStringLiteral("signal %1").arg(int(c)));
    });

    server.start();
    return app.exec();
}
