// Owns the privileged dlsim-helper process (started through pkexec) and
// exchanges line-delimited JSON with it. Requests carry ids; responses are
// matched back to their callbacks.
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

#include <functional>
#include <memory>

class QTemporaryDir;

class EventLog;

class HelperClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString helperPath READ helperPath CONSTANT)
public:
    enum State { Stopped, Starting, Running, Failed };
    Q_ENUM(State)

    using Callback = std::function<void(bool ok, const QJsonObject &result, const QString &error)>;

    explicit HelperClient(EventLog *log, QObject *parent = nullptr);
    ~HelperClient() override;

    State state() const { return m_state; }
    bool isRunning() const { return m_state == Running; }
    QString statusText() const { return m_statusText; }
    QString helperPath() const;
    int pendingCount() const { return int(m_pending.size()); }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    // Returns false (and calls cb with an error) when the helper is not running.
    bool request(const QString &op, const QJsonObject &params = {}, Callback cb = {});

Q_SIGNALS:
    void stateChanged();
    void ready();
    void lost(const QString &reason);
    void commandsExecuted(const QString &op, const QJsonArray &commands);

private:
    QString stageAppImageHelper(QString *error);
    void setState(State s, const QString &text);
    void onReadyRead();
    void onStderr();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void handleMessage(const QJsonObject &msg);
    void failAllPending(const QString &reason);

    struct Pending {
        QString op;
        Callback cb;
    };

    EventLog *m_log;
    QProcess *m_proc = nullptr;
    State m_state = Stopped;
    QString m_statusText = QStringLiteral("not started");
    QHash<qint64, Pending> m_pending;
    qint64 m_nextId = 1;
    QByteArray m_buffer;
    bool m_stopping = false;
    std::unique_ptr<QTemporaryDir> m_stage; // helper copied out of an AppImage
};
