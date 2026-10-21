#include "Model.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>

namespace dlsim {

namespace {

template <typename E, size_t N>
QString enumName(E v, const std::array<std::pair<E, const char *>, N> &table)
{
    for (const auto &[e, s] : table)
        if (e == v)
            return QString::fromLatin1(s);
    return {};
}

template <typename E, size_t N>
bool enumParse(const QString &s, E *out, const std::array<std::pair<E, const char *>, N> &table)
{
    for (const auto &[e, n] : table) {
        if (s == QLatin1String(n)) {
            *out = e;
            return true;
        }
    }
    return false;
}

constexpr std::array<std::pair<Direction, const char *>, 2> kDirections{{
    {Direction::Uplink, "uplink"}, {Direction::Downlink, "downlink"}}};
constexpr std::array<std::pair<Mode, const char *>, 2> kModes{{
    {Mode::Bridge, "bridge"}, {Mode::Host, "host"}}};
constexpr std::array<std::pair<Distribution, const char *>, 4> kDistributions{{
    {Distribution::Uniform, "uniform"}, {Distribution::Normal, "normal"},
    {Distribution::Pareto, "pareto"}, {Distribution::ParetoNormal, "paretonormal"}}};
constexpr std::array<std::pair<LossModel, const char *>, 2> kLossModels{{
    {LossModel::Random, "random"}, {LossModel::GilbertElliott, "gemodel"}}};
constexpr std::array<std::pair<Proto, const char *>, 4> kProtos{{
    {Proto::Any, "any"}, {Proto::Tcp, "tcp"}, {Proto::Udp, "udp"}, {Proto::Icmp, "icmp"}}};
constexpr std::array<std::pair<ClassMode, const char *>, 3> kClassModes{{
    {ClassMode::Link, "link"}, {ClassMode::Custom, "custom"}, {ClassMode::Excluded, "excluded"}}};

double num(const QJsonObject &o, const char *key, double def)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble())
        return def;
    const double d = v.toDouble();
    return std::isfinite(d) ? d : def;
}

void checkRange(QStringList &errs, const QString &prefix, const char *name, double v, double lo, double hi)
{
    if (!(v >= lo && v <= hi))
        errs << QStringLiteral("%1%2 must be within %3..%4 (got %5)")
                    .arg(prefix, QLatin1String(name)).arg(lo).arg(hi).arg(v);
}

double lerp(double a, double b, double t) { return (1 - t) * a + t * b; } // exact at both ends

constexpr double kUnlimitedKbit = 1'000'000; // 1 Gbit/s stands in for "unlimited" while blending

} // namespace

QString toString(Direction v) { return enumName(v, kDirections); }
QString toString(Mode v) { return enumName(v, kModes); }
QString toString(Distribution v) { return enumName(v, kDistributions); }
QString toString(LossModel v) { return enumName(v, kLossModels); }
QString toString(Proto v) { return enumName(v, kProtos); }
QString toString(ClassMode v) { return enumName(v, kClassModes); }
bool parse(const QString &s, Mode *v) { return enumParse(s, v, kModes); }
bool parse(const QString &s, Distribution *v) { return enumParse(s, v, kDistributions); }
bool parse(const QString &s, LossModel *v) { return enumParse(s, v, kLossModels); }
bool parse(const QString &s, Proto *v) { return enumParse(s, v, kProtos); }
bool parse(const QString &s, ClassMode *v) { return enumParse(s, v, kClassModes); }

// ---------------------------------------------------------------- Profile

QJsonObject Profile::toJson() const
{
    return QJsonObject{
        {"delayMs", delayMs},
        {"jitterMs", jitterMs},
        {"delayCorrPct", delayCorrPct},
        {"distribution", toString(distribution)},
        {"lossModel", toString(lossModel)},
        {"lossPct", lossPct},
        {"lossCorrPct", lossCorrPct},
        {"geP", geP},
        {"geR", geR},
        {"geBadLossPct", geBadLossPct},
        {"geGoodLossPct", geGoodLossPct},
        {"rateKbit", double(rateKbit)},
        {"limitPkts", double(limitPkts)},
        {"duplicatePct", duplicatePct},
        {"reorderPct", reorderPct},
        {"reorderCorrPct", reorderCorrPct},
        {"reorderGap", double(reorderGap)},
        {"corruptPct", corruptPct},
    };
}

Profile Profile::fromJson(const QJsonObject &o)
{
    Profile p;
    p.delayMs = num(o, "delayMs", p.delayMs);
    p.jitterMs = num(o, "jitterMs", p.jitterMs);
    p.delayCorrPct = num(o, "delayCorrPct", p.delayCorrPct);
    parse(o.value("distribution").toString(), &p.distribution);
    parse(o.value("lossModel").toString(), &p.lossModel);
    p.lossPct = num(o, "lossPct", p.lossPct);
    p.lossCorrPct = num(o, "lossCorrPct", p.lossCorrPct);
    p.geP = num(o, "geP", p.geP);
    p.geR = num(o, "geR", p.geR);
    p.geBadLossPct = num(o, "geBadLossPct", p.geBadLossPct);
    p.geGoodLossPct = num(o, "geGoodLossPct", p.geGoodLossPct);
    p.rateKbit = quint64(std::clamp(num(o, "rateKbit", 0), 0.0, 1e12));
    p.limitPkts = quint32(std::clamp(num(o, "limitPkts", 0), 0.0, 4e9));
    p.duplicatePct = num(o, "duplicatePct", p.duplicatePct);
    p.reorderPct = num(o, "reorderPct", p.reorderPct);
    p.reorderCorrPct = num(o, "reorderCorrPct", p.reorderCorrPct);
    p.reorderGap = quint32(std::clamp(num(o, "reorderGap", 0), 0.0, 4e9));
    p.corruptPct = num(o, "corruptPct", p.corruptPct);
    return p;
}

QStringList Profile::validate(const QString &prefix) const
{
    QStringList e;
    checkRange(e, prefix, "delayMs", delayMs, 0, 60000);
    checkRange(e, prefix, "jitterMs", jitterMs, 0, 60000);
    checkRange(e, prefix, "delayCorrPct", delayCorrPct, 0, 100);
    checkRange(e, prefix, "lossPct", lossPct, 0, 100);
    checkRange(e, prefix, "lossCorrPct", lossCorrPct, 0, 100);
    checkRange(e, prefix, "geP", geP, 0, 100);
    checkRange(e, prefix, "geR", geR, 0, 100);
    checkRange(e, prefix, "geBadLossPct", geBadLossPct, 0, 100);
    checkRange(e, prefix, "geGoodLossPct", geGoodLossPct, 0, 100);
    checkRange(e, prefix, "rateKbit", double(rateKbit), 0, 10'000'000);
    checkRange(e, prefix, "limitPkts", double(limitPkts), 0, 1'000'000);
    checkRange(e, prefix, "duplicatePct", duplicatePct, 0, 100);
    checkRange(e, prefix, "reorderPct", reorderPct, 0, 100);
    checkRange(e, prefix, "reorderCorrPct", reorderCorrPct, 0, 100);
    checkRange(e, prefix, "reorderGap", double(reorderGap), 0, 1'000'000);
    checkRange(e, prefix, "corruptPct", corruptPct, 0, 100);
    if (rateKbit > 0 && rateKbit < 8)
        e << prefix + QStringLiteral("rateKbit must be 0 (unlimited) or at least 8");
    return e;
}

bool Profile::hasLoss() const
{
    return lossModel == LossModel::Random ? lossPct > 0 : geP > 0;
}

Profile interpolate(const Profile &a, const Profile &b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    Profile r = t < 0.5 ? a : b;
    r.delayMs = lerp(a.delayMs, b.delayMs, t);
    r.jitterMs = lerp(a.jitterMs, b.jitterMs, t);
    r.delayCorrPct = lerp(a.delayCorrPct, b.delayCorrPct, t);
    r.duplicatePct = lerp(a.duplicatePct, b.duplicatePct, t);
    r.reorderPct = lerp(a.reorderPct, b.reorderPct, t);
    r.reorderCorrPct = lerp(a.reorderCorrPct, b.reorderCorrPct, t);
    r.corruptPct = lerp(a.corruptPct, b.corruptPct, t);

    if (a.lossModel == b.lossModel) {
        r.lossPct = lerp(a.lossPct, b.lossPct, t);
        r.lossCorrPct = lerp(a.lossCorrPct, b.lossCorrPct, t);
        r.geP = lerp(a.geP, b.geP, t);
        r.geR = lerp(a.geR, b.geR, t);
        r.geBadLossPct = lerp(a.geBadLossPct, b.geBadLossPct, t);
        r.geGoodLossPct = lerp(a.geGoodLossPct, b.geGoodLossPct, t);
    } else {
        // Models differ: blend through "no loss" so the switch is not a jump.
        const Profile &src = t < 0.5 ? a : b;
        const double k = t < 0.5 ? 1.0 - 2 * t : 2 * t - 1.0;
        r.lossPct = src.lossPct * k;
        r.geP = src.geP * k;
    }

    const double ra = a.rateKbit ? double(a.rateKbit) : kUnlimitedKbit;
    const double rb = b.rateKbit ? double(b.rateKbit) : kUnlimitedKbit;
    // Geometric: a fade from 1 Gbit/s to 200 kbit/s should not stay above 100 Mbit/s for 90% of the time.
    const quint64 rr = quint64(std::llround(std::exp(lerp(std::log(ra), std::log(rb), t))));
    r.rateKbit = rr >= quint64(kUnlimitedKbit) ? 0 : rr;

    // Keep an explicit limit only if both ends set one.
    r.limitPkts = (a.limitPkts && b.limitPkts) ? quint32(std::lround(lerp(a.limitPkts, b.limitPkts, t))) : 0;
    return r;
}

Profile cutProfile(const Profile &p)
{
    Profile c = p;
    c.lossModel = LossModel::Random;
    c.lossPct = 100;
    c.lossCorrPct = 0;
    return c;
}

// ---------------------------------------------------------------- TrafficClass

QJsonObject TrafficClass::toJson() const
{
    return QJsonObject{
        {"name", name},
        {"enabled", enabled},
        {"proto", toString(proto)},
        {"ip", ip},
        {"port", int(port)},
        {"mode", toString(mode)},
        {"uplink", uplink.toJson()},
        {"downlink", downlink.toJson()},
    };
}

TrafficClass TrafficClass::fromJson(const QJsonObject &o)
{
    TrafficClass c;
    c.name = o.value("name").toString();
    c.enabled = o.value("enabled").toBool(true);
    parse(o.value("proto").toString(), &c.proto);
    c.ip = o.value("ip").toString().trimmed();
    c.port = quint16(std::clamp(o.value("port").toInt(0), 0, 65535));
    parse(o.value("mode").toString(), &c.mode);
    c.uplink = Profile::fromJson(o.value("uplink").toObject());
    c.downlink = Profile::fromJson(o.value("downlink").toObject());
    return c;
}

QStringList TrafficClass::validate() const
{
    QStringList e;
    const QString p = QStringLiteral("class '%1': ").arg(name);
    if (name.size() > 64)
        e << p + QStringLiteral("name too long");
    if (!ip.isEmpty() && !isValidIp(ip))
        e << p + QStringLiteral("invalid IPv4 address '%1'").arg(ip);
    if (port != 0 && proto != Proto::Tcp && proto != Proto::Udp)
        e << p + QStringLiteral("a port needs protocol tcp or udp");
    if (mode == ClassMode::Custom) {
        e << uplink.validate(p + "uplink.");
        e << downlink.validate(p + "downlink.");
    }
    return e;
}

// ---------------------------------------------------------------- LinkConfig

QJsonObject LinkConfig::toJson() const
{
    QJsonArray cls;
    for (const auto &c : classes)
        cls.append(c.toJson());
    return QJsonObject{
        {"mode", toString(mode)},
        {"groundIface", groundIface},
        {"airIface", airIface},
        {"hostIface", hostIface},
        {"uplink", uplink.toJson()},
        {"downlink", downlink.toJson()},
        {"classes", cls},
        {"otherMode", toString(otherMode)},
        {"cut", cut},
    };
}

LinkConfig LinkConfig::fromJson(const QJsonObject &o)
{
    LinkConfig c;
    parse(o.value("mode").toString(), &c.mode);
    c.groundIface = o.value("groundIface").toString();
    c.airIface = o.value("airIface").toString();
    c.hostIface = o.value("hostIface").toString();
    c.uplink = Profile::fromJson(o.value("uplink").toObject());
    c.downlink = Profile::fromJson(o.value("downlink").toObject());
    for (const auto &v : o.value("classes").toArray())
        c.classes.append(TrafficClass::fromJson(v.toObject()));
    parse(o.value("otherMode").toString(), &c.otherMode);
    c.cut = o.value("cut").toBool(false);
    return c;
}

QStringList LinkConfig::validate(const QStringList &knownIfaces) const
{
    QStringList e;
    auto checkIface = [&](const QString &role, const QString &name) {
        if (!isValidIfaceName(name)) {
            e << QStringLiteral("%1 interface name '%2' is invalid").arg(role, name);
            return;
        }
        if (name == "lo" || name == kBridgeName || name == kIfbName)
            e << QStringLiteral("%1 interface '%2' cannot be used").arg(role, name);
        else if (!knownIfaces.isEmpty() && !knownIfaces.contains(name))
            e << QStringLiteral("%1 interface '%2' does not exist").arg(role, name);
    };
    if (mode == Mode::Bridge) {
        checkIface("ground", groundIface);
        checkIface("air", airIface);
        if (groundIface == airIface)
            e << QStringLiteral("ground and air interfaces must differ");
    } else {
        checkIface("host", hostIface);
    }
    e << uplink.validate("uplink.");
    e << downlink.validate("downlink.");
    if (classes.size() > kMaxClasses)
        e << QStringLiteral("at most %1 traffic classes").arg(kMaxClasses);
    int custom = 0;
    for (const auto &c : classes) {
        e << c.validate();
        if (c.enabled && c.mode == ClassMode::Custom)
            ++custom;
    }
    if (custom > kMaxCustomClasses)
        e << QStringLiteral("at most %1 classes with a custom profile").arg(kMaxCustomClasses);
    if (otherMode == ClassMode::Custom)
        e << QStringLiteral("'Other' traffic can only use the link profile or be excluded");
    return e;
}

bool isValidIfaceName(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.-]{1,15}$"));
    return re.match(name).hasMatch() && name != "." && name != "..";
}

bool isValidIp(const QString &ip)
{
    // Dotted quad only, no leading zeros (they would be read as octal by some tools).
    static const QRegularExpression re(QStringLiteral(
        "^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$"));
    return re.match(ip).hasMatch();
}

} // namespace dlsim
