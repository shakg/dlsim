#include "HelperClient.h"

#include "EventLog.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>

#include <unistd.h>

HelperClient::HelperClient(EventLog *log, QObject *parent) : QObject(parent), m_log(log) {}

HelperClient::~HelperClient()
{
    stop();
}

QString HelperClient::helperPath() const
{
    const QString env = qEnvironmentVariable("DLSIM_HELPER");
    if (!env.isEmpty())
        return QFileInfo(env).absoluteFilePath();
    const QString appDir = QCoreApplication::applicationDirPath();
    // Running from a build tree: use the helper built next to the GUI.
    const QFileInfo built(appDir + "/../helper/dlsim-helper");
    if (built.isExecutable())
        return built.canonicalFilePath();
    return QStringLiteral(DLSIM_HELPER_INSTALL_PATH);
}

void HelperClient::setState(State s, const QString &text)
{
    m_state = s;
    m_statusText = text;
    Q_EMIT stateChanged();
}

void HelperClient::start()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        return;
    delete m_proc;
    m_proc = new QProcess(this);
    m_buffer.clear();
    m_stopping = false;
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &HelperClient::onReadyRead);
    connect(m_proc, &QProcess::readyReadStandardError, this, &HelperClient::onStderr);
    connect(m_proc, &QProcess::finished, this, &HelperClient::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_log->log("helper", QStringLiteral("failed to start: %1").arg(m_proc->errorString()));
            setState(Failed, QStringLiteral("failed to start: %1").arg(m_proc->errorString()));
            Q_EMIT lost(m_statusText);
        }
    });

    const QString helper = helperPath();
    if (!QFileInfo(helper).isExecutable()) {
        setState(Failed, QStringLiteral("helper not found at %1").arg(helper));
        m_log->log("helper", m_statusText);
        return;
    }
    // Already root (or explicitly asked): run directly, otherwise through pkexec.
    const bool direct = ::geteuid() == 0 || qEnvironmentVariableIntValue("DLSIM_NO_PKEXEC") == 1;
    setState(Starting, direct ? QStringLiteral("starting helper") : QStringLiteral("waiting for authentication"));
    m_log->log("helper", QStringLiteral("starting %1%2").arg(direct ? QString() : "pkexec ", helper));
    if (direct)
        m_proc->start(helper, {});
    else
        m_proc->start(QStringLiteral("pkexec"), {helper});
}

void HelperClient::stop()
{
    if (!m_proc || m_proc->state() == QProcess::NotRunning)
        return;
    m_stopping = true;
    // The helper runs as root, so it cannot be killed from here. Closing its
    // stdin makes it clean up and exit.
    m_proc->closeWriteChannel();
    if (!m_proc->waitForFinished(8000))
        m_log->log("helper", "helper did not exit within 8 s");
}

bool HelperClient::request(const QString &op, const QJsonObject &params, Callback cb)
{
    if (m_state != Running || !m_proc) {
        if (cb)
            cb(false, {}, QStringLiteral("helper is not running"));
        return false;
    }
    const qint64 id = m_nextId++;
    m_pending.insert(id, {op, std::move(cb)});
    QByteArray line = QJsonDocument(QJsonObject{{"id", double(id)}, {"op", op}, {"params", params}})
                          .toJson(QJsonDocument::Compact);
    line.append('\n');
    m_proc->write(line);
    return true;
}

void HelperClient::onReadyRead()
{
    m_buffer.append(m_proc->readAllStandardOutput());
    qsizetype nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject())
            handleMessage(doc.object());
        else if (!line.trimmed().isEmpty())
            m_log->log("helper", QStringLiteral("unparsable output: %1").arg(QString::fromUtf8(line)));
    }
}

void HelperClient::onStderr()
{
    const QString text = QString::fromUtf8(m_proc->readAllStandardError()).trimmed();
    for (const QString &l : text.split('\n', Qt::SkipEmptyParts))
        m_log->log("helper", l);
}

void HelperClient::handleMessage(const QJsonObject &msg)
{
    if (msg.contains("event")) {
        const QString ev = msg.value("event").toString();
        if (ev == "ready") {
            const QJsonArray cleaned = msg.value("cleaned").toArray();
            if (!cleaned.isEmpty()) {
                QStringList cmds;
                for (const auto &c : cleaned)
                    cmds << c.toString();
                m_log->log("cleanup", QStringLiteral("removed leftovers from a previous run: %1").arg(cmds.join("; ")));
            }
            if (!msg.value("root").toBool())
                m_log->log("helper", "warning: helper is not running as root; tc/ip changes will fail");
            setState(Running, QStringLiteral("helper %1 running").arg(msg.value("version").toString()));
            m_log->log("helper", m_statusText);
            Q_EMIT ready();
        } else {
            m_log->log("helper", QStringLiteral("%1: %2").arg(ev, msg.value("error").toString()));
        }
        return;
    }
    const qint64 id = qint64(msg.value("id").toDouble(-1));
    const auto it = m_pending.find(id);
    if (it == m_pending.end()) {
        m_log->log("helper", QStringLiteral("response for unknown request: %1").arg(msg.value("error").toString()));
        return;
    }
    Pending p = std::move(it.value());
    m_pending.erase(it);
    const QJsonArray cmds = msg.value("commands").toArray();
    if (!cmds.isEmpty())
        Q_EMIT commandsExecuted(p.op, cmds);
    if (p.cb)
        p.cb(msg.value("ok").toBool(), msg.value("result").toObject(), msg.value("error").toString());
}

void HelperClient::failAllPending(const QString &reason)
{
    const auto pending = std::exchange(m_pending, {});
    for (const Pending &p : pending)
        if (p.cb)
            p.cb(false, {}, reason);
}

void HelperClient::onFinished(int exitCode, QProcess::ExitStatus status)
{
    QString reason;
    if (m_stopping)
        reason = QStringLiteral("helper stopped");
    else if (status == QProcess::CrashExit)
        reason = QStringLiteral("helper crashed");
    else if (exitCode == 126)
        reason = QStringLiteral("authentication dismissed");
    else if (exitCode == 127)
        reason = QStringLiteral("not authorized (or pkexec failed)");
    else
        reason = QStringLiteral("helper exited with code %1").arg(exitCode);
    failAllPending(reason);
    m_log->log("helper", reason);
    setState(m_stopping ? Stopped : Failed, reason);
    if (!m_stopping)
        Q_EMIT lost(reason);
}
