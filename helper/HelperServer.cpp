#include "HelperServer.h"

#include "Parsers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSocketNotifier>

#include <cerrno>
#include <unistd.h>

using namespace dlsim;

namespace {

constexpr qsizetype kMaxLine = 4 * 1024 * 1024;
constexpr int kCommandTimeoutMs = 10000;

const QStringList kOps{"apply_profile", "cut", "restore", "reset", "read_stats",
                       "read_sockets", "setup_bridge", "teardown"};

} // namespace

HelperServer::HelperServer(QObject *parent) : QObject(parent) {}

void HelperServer::start()
{
    QString err;
    const CommandList leftovers = scanLeftovers();
    execute(leftovers, &err);
    QJsonArray cleaned;
    for (const auto &c : leftovers)
        cleaned.append(c.toString());
    send(QJsonObject{
        {"event", "ready"},
        {"version", DLSIM_VERSION},
        {"root", geteuid() == 0},
        {"cleaned", cleaned},
    });
    m_executed = {};

    m_stdin = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(m_stdin, &QSocketNotifier::activated, this, &HelperServer::onStdinReadable);
}

void HelperServer::shutdown(const QString &reason)
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    if (m_stdin)
        m_stdin->setEnabled(false);
    QString err;
    execute(cleanupCommands(), &err);
    qInfo("dlsim-helper: cleaned up (%s)", qPrintable(reason));
    QCoreApplication::exit(0);
}

void HelperServer::onStdinReadable()
{
    char buf[65536];
    const ssize_t n = ::read(STDIN_FILENO, buf, sizeof buf);
    if (n <= 0) {
        if (n < 0 && (errno == EINTR || errno == EAGAIN))
            return;
        shutdown(QStringLiteral("stdin closed"));
        return;
    }
    m_buffer.append(buf, n);
    qsizetype nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl).trimmed();
        m_buffer.remove(0, nl + 1);
        if (!line.isEmpty())
            handleLine(line);
    }
    if (m_buffer.size() > kMaxLine) {
        m_buffer.clear();
        send({{"event", "error"}, {"error", "request line too long"}});
    }
}

void HelperServer::send(const QJsonObject &obj)
{
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    qsizetype off = 0;
    while (off < data.size()) {
        const ssize_t w = ::write(STDOUT_FILENO, data.constData() + off, size_t(data.size() - off));
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        off += w;
    }
}

void HelperServer::handleLine(const QByteArray &line)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        send({{"ok", false}, {"error", "malformed request"}});
        return;
    }
    const QJsonObject req = doc.object();
    const QJsonValue id = req.value("id");
    const QString op = req.value("op").toString();
    const QJsonObject params = req.value("params").toObject();

    m_executed = {};
    QString error;
    QJsonObject result;
    if (!kOps.contains(op)) {
        error = QStringLiteral("operation '%1' is not allowed").arg(op);
    } else if (op == "apply_profile") {
        result = opApplyProfile(params, &error);
    } else if (op == "cut" || op == "restore") {
        result = opCut(op == "cut", &error);
    } else if (op == "reset") {
        result = opReset(&error);
    } else if (op == "read_stats") {
        result = opReadStats(&error);
    } else if (op == "read_sockets") {
        result = opReadSockets(params, &error);
    } else if (op == "setup_bridge") {
        result = opSetupBridge(params, &error);
    } else if (op == "teardown") {
        result = opTeardown(&error);
    }

    QJsonObject resp{{"id", id}, {"op", op}, {"ok", error.isEmpty()}, {"commands", m_executed}};
    if (error.isEmpty())
        resp.insert("result", result);
    else
        resp.insert("error", error);
    send(resp);
}

// ---------------------------------------------------------------- operations

QJsonObject HelperServer::opApplyProfile(const QJsonObject &params, QString *error)
{
    const LinkConfig cfg = LinkConfig::fromJson(params.value("config").toObject());
    const QStringList errs = cfg.validate(linkNames());
    if (!errs.isEmpty()) {
        *error = errs.join("; ");
        return {};
    }
    if (cfg.mode == Mode::Bridge) {
        const QStringList ports = bridgePorts();
        if (!ports.contains(cfg.groundIface) || !ports.contains(cfg.airIface)) {
            *error = QStringLiteral("bridge %1 with ports %2 and %3 is not set up (run setup_bridge first)")
                         .arg(kBridgeName, cfg.groundIface, cfg.airIface);
            return {};
        }
    }
    return applyConfig(cfg, error);
}

QJsonObject HelperServer::opCut(bool cut, QString *error)
{
    if (!m_config) {
        *error = QStringLiteral("no profile applied");
        return {};
    }
    LinkConfig cfg = *m_config;
    cfg.cut = cut;
    return applyConfig(cfg, error);
}

QJsonObject HelperServer::applyConfig(const LinkConfig &cfg, QString *error)
{
    const Plan desired = TcCommandBuilder::buildPlan(cfg);
    const Diff d = TcCommandBuilder::diff(m_applied, desired);
    DiffLevel level = d.level;
    QString err;
    bool ok = execute(d.commands, &err);
    if (!ok && m_applied && d.level != DiffLevel::Rebuild) {
        // Partial update failed: rebuild from scratch.
        level = DiffLevel::Rebuild;
        ok = execute(TcCommandBuilder::teardown(*m_applied) + TcCommandBuilder::buildAll(desired), &err);
    }
    if (!ok) {
        QString ignored;
        if (m_applied)
            execute(TcCommandBuilder::teardown(*m_applied), &ignored);
        execute(TcCommandBuilder::teardown(desired), &ignored);
        m_applied.reset();
        m_config.reset();
        *error = QStringLiteral("apply failed, impairment removed: %1").arg(err);
        return {};
    }
    m_applied = desired;
    m_config = cfg;
    return {{"level", toString(level)}, {"cut", cfg.cut}};
}

QJsonObject HelperServer::opReset(QString *error)
{
    if (!execute(cleanupCommands(), error))
        return {};
    return {{"reset", true}};
}

QJsonObject HelperServer::opTeardown(QString *error)
{
    CommandList cmds;
    if (m_applied && m_applied->mode == Mode::Bridge) {
        cmds << TcCommandBuilder::teardown(*m_applied);
        m_applied.reset();
        m_config.reset();
    }
    QStringList ports = bridgePorts();
    cmds << TcCommandBuilder::teardownBridge(ports.value(0, m_bridgeGround), ports.value(1, m_bridgeAir));
    m_bridgeGround.clear();
    m_bridgeAir.clear();
    if (!execute(cmds, error))
        return {};
    return {{"bridge", false}};
}

QJsonObject HelperServer::opSetupBridge(const QJsonObject &params, QString *error)
{
    const QString ground = params.value("groundIface").toString();
    const QString air = params.value("airIface").toString();
    LinkConfig probe;
    probe.mode = Mode::Bridge;
    probe.groundIface = ground;
    probe.airIface = air;
    const QStringList errs = probe.validate(linkNames());
    if (!errs.isEmpty()) {
        *error = errs.join("; ");
        return {};
    }
    for (const auto &v : links()) {
        const QJsonObject l = v.toObject();
        const QString name = l.value("ifname").toString();
        const QString master = l.value("master").toString();
        if ((name == ground || name == air) && !master.isEmpty() && master != kBridgeName) {
            *error = QStringLiteral("interface %1 is already enslaved to %2").arg(name, master);
            return {};
        }
    }

    const QStringList ports = bridgePorts();
    if (linkNames().contains(kBridgeName) && ports.size() == 2 && ports.contains(ground) && ports.contains(air)) {
        m_bridgeGround = ground;
        m_bridgeAir = air;
        return {{"bridge", true}, {"unchanged", true}};
    }

    CommandList cmds;
    if (m_applied) {
        cmds << TcCommandBuilder::teardown(*m_applied);
        m_applied.reset();
        m_config.reset();
    }
    if (linkNames().contains(kBridgeName))
        cmds << TcCommandBuilder::teardownBridge(ports.value(0), ports.value(1));
    cmds << TcCommandBuilder::setupBridge(ground, air);
    if (!execute(cmds, error)) {
        QString ignored;
        execute(TcCommandBuilder::teardownBridge(ground, air), &ignored);
        return {};
    }
    m_bridgeGround = ground;
    m_bridgeAir = air;
    return {{"bridge", true}};
}

QJsonObject HelperServer::opReadStats(QString *error)
{
    QJsonObject result{{"active", bool(m_applied)},
                       {"cut", m_config ? m_config->cut : false},
                       {"timestampMs", double(QDateTime::currentMSecsSinceEpoch())}};
    if (!m_applied)
        return result;
    for (const DeviceTree &t : m_applied->trees) {
        const RunResult r = run("tc", {"-s", "-j", "qdisc", "show", "dev", t.dev});
        QList<QdiscStats> stats;
        QString perr;
        if (!r.ok || !parseQdiscStats(r.out, &stats, &perr)) {
            *error = QStringLiteral("tc stats on %1 failed: %2").arg(t.dev, r.ok ? perr : QString::fromUtf8(r.err).trimmed());
            return {};
        }
        QJsonObject dir{{"dev", t.dev}};
        QJsonArray leaves;
        for (const auto &s : stats) {
            if (s.root && s.handle == kRootHandle)
                dir.insert("total", s.toJson());
            else if (s.parent.startsWith(kRootHandle))
                leaves.append(s.toJson());
        }
        dir.insert("leaves", leaves);
        result.insert(toString(t.dir), dir);
    }
    return result;
}

QJsonObject HelperServer::opReadSockets(const QJsonObject &params, QString *error)
{
    const QString ip = params.value("ip").toString();
    const int port = params.value("port").toInt(-1);
    if (!isValidIp(ip)) {
        *error = QStringLiteral("invalid IPv4 address");
        return {};
    }
    if (port < 1 || port > 65535) {
        *error = QStringLiteral("port must be 1..65535");
        return {};
    }
    const QString p = ':' + QString::number(port);
    const QStringList base{"-t", "-n", "-o", "-i", "-H", "state", "all"};
    QList<TcpSocketInfo> all;
    for (const QStringList &filter : {QStringList{"dst", ip, "dport", "=", p}, QStringList{"src", ip, "sport", "=", p}}) {
        const RunResult r = run("ss", base + filter);
        if (!r.ok) {
            *error = QStringLiteral("ss failed: %1").arg(QString::fromUtf8(r.err).trimmed());
            return {};
        }
        all << parseSs(QString::fromUtf8(r.out));
    }
    QJsonArray out;
    QStringList seen;
    for (const auto &s : all) {
        if (seen.contains(s.key()))
            continue;
        seen << s.key();
        out.append(s.toJson());
    }
    return {{"sockets", out}, {"timestampMs", double(QDateTime::currentMSecsSinceEpoch())}};
}

// ---------------------------------------------------------------- cleanup

CommandList HelperServer::cleanupCommands()
{
    CommandList cmds;
    if (m_applied)
        cmds << TcCommandBuilder::teardown(*m_applied);
    if (!m_bridgeGround.isEmpty() || !m_bridgeAir.isEmpty())
        cmds << TcCommandBuilder::teardownBridge(m_bridgeGround, m_bridgeAir);
    m_applied.reset();
    m_config.reset();
    m_bridgeGround.clear();
    m_bridgeAir.clear();
    QString ignored;
    execute(cmds, &ignored);
    // Anything still left (e.g. from a previous crash).
    return scanLeftovers();
}

CommandList HelperServer::scanLeftovers()
{
    const QJsonArray l = links();
    const QJsonArray q = runJson("tc", {"-j", "qdisc", "show"});
    QHash<QString, QJsonArray> ingress;
    for (const QString &dev : TcCommandBuilder::devicesWithIngress(q))
        ingress.insert(dev, runJson("tc", {"-j", "filter", "show", "dev", dev, "parent", "ffff:"}));
    return TcCommandBuilder::leftoverCleanup(l, q, ingress);
}

// ---------------------------------------------------------------- process execution

QString HelperServer::resolve(const QString &program) const
{
    // Fixed directories only: PATH is not trusted when running as root.
    for (const char *dir : {"/usr/sbin", "/sbin", "/usr/bin", "/bin"}) {
        const QString path = QString::fromLatin1(dir) + '/' + program;
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return {};
}

HelperServer::RunResult HelperServer::run(const QString &program, const QStringList &args)
{
    RunResult r;
    if (program != "tc" && program != "ip" && program != "ss") {
        r.err = "program not allowed";
        return r;
    }
    const QString exe = resolve(program);
    if (exe.isEmpty()) {
        r.err = QStringLiteral("%1 not found").arg(program).toUtf8();
        return r;
    }
    QProcess p;
    QProcessEnvironment env;
    env.insert("LC_ALL", "C");
    env.insert("PATH", "/usr/sbin:/usr/bin:/sbin:/bin");
    p.setProcessEnvironment(env);
    p.start(exe, args);
    if (!p.waitForFinished(kCommandTimeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        r.err = "timed out";
        return r;
    }
    r.exitCode = p.exitCode();
    r.ok = p.exitStatus() == QProcess::NormalExit && r.exitCode == 0;
    r.out = p.readAllStandardOutput();
    r.err = p.readAllStandardError();
    return r;
}

QJsonArray HelperServer::runJson(const QString &program, const QStringList &args, bool *ok)
{
    const RunResult r = run(program, args);
    const QJsonDocument doc = QJsonDocument::fromJson(r.out);
    if (ok)
        *ok = r.ok && doc.isArray();
    return doc.array();
}

bool HelperServer::execute(const CommandList &cmds, QString *firstError)
{
    bool allOk = true;
    for (const Command &c : cmds) {
        const RunResult r = run(c.program, c.args);
        const QString stderrText = QString::fromUtf8(r.err).trimmed();
        m_executed.append(QJsonObject{
            {"cmd", c.toString()},
            {"ok", r.ok},
            {"exitCode", r.exitCode},
            {"stderr", stderrText},
            {"ignoreError", c.ignoreError},
        });
        if (!r.ok && !c.ignoreError) {
            if (firstError)
                *firstError = QStringLiteral("`%1` failed: %2").arg(c.toString(), stderrText);
            allOk = false;
            break;
        }
    }
    return allOk;
}

QJsonArray HelperServer::links()
{
    return runJson("ip", {"-j", "link", "show"});
}

QStringList HelperServer::linkNames()
{
    QStringList names;
    for (const auto &v : links())
        names << v.toObject().value("ifname").toString();
    return names;
}

QStringList HelperServer::bridgePorts()
{
    QStringList ports;
    for (const auto &v : links()) {
        const QJsonObject l = v.toObject();
        if (l.value("master").toString() == kBridgeName)
            ports << l.value("ifname").toString();
    }
    return ports;
}
