// Privileged side of DLSim. Reads line-delimited JSON requests on stdin,
// writes one JSON response line per request on stdout.
//
//   request:  {"id": 7, "op": "apply_profile", "params": {...}}
//   response: {"id": 7, "ok": true, "result": {...}, "commands": [...]}
//             {"id": 7, "ok": false, "error": "...", "commands": [...]}
//   event:    {"event": "ready", ...}
//
// Only the operations in the whitelist are accepted. External tools are run
// with QProcess argument lists, never through a shell.
#pragma once

#include "Model.h"
#include "TcCommandBuilder.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <optional>

class QSocketNotifier;

class HelperServer : public QObject
{
    Q_OBJECT
public:
    explicit HelperServer(QObject *parent = nullptr);

    void start();                          // leftover cleanup, "ready" event, start reading stdin
    void shutdown(const QString &reason);  // full cleanup, then quit the event loop

private:
    struct RunResult {
        bool ok = false;
        int exitCode = -1;
        QByteArray out;
        QByteArray err;
    };

    void onStdinReadable();
    void handleLine(const QByteArray &line);
    void send(const QJsonObject &obj);

    // Operations. Each returns the result object or sets *error.
    QJsonObject opApplyProfile(const QJsonObject &params, QString *error);
    QJsonObject opCut(bool cut, QString *error);
    QJsonObject opReset(QString *error);
    QJsonObject opReadStats(QString *error);
    QJsonObject opReadSockets(const QJsonObject &params, QString *error);
    QJsonObject opSetupBridge(const QJsonObject &params, QString *error);
    QJsonObject opTeardown(QString *error);

    QJsonObject applyConfig(const dlsim::LinkConfig &cfg, QString *error);
    dlsim::CommandList cleanupCommands();
    dlsim::CommandList scanLeftovers();

    RunResult run(const QString &program, const QStringList &args);
    QJsonArray runJson(const QString &program, const QStringList &args, bool *ok = nullptr);
    bool execute(const dlsim::CommandList &cmds, QString *firstError);
    QString resolve(const QString &program) const;
    QJsonArray links();
    QStringList linkNames();
    QStringList bridgePorts();

    QSocketNotifier *m_stdin = nullptr;
    QByteArray m_buffer;
    QJsonArray m_executed; // commands run while handling the current request
    std::optional<dlsim::Plan> m_applied;
    std::optional<dlsim::LinkConfig> m_config;
    QString m_bridgeGround, m_bridgeAir;
    bool m_shuttingDown = false;
};
