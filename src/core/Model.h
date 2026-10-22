// Data model shared by the GUI, the privileged helper and the tests.
// Everything here is plain data plus JSON (de)serialisation and validation.
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace dlsim {

// Fixed names and handles. Every kernel object DLSim creates carries one of
// these, so leftovers from a crash can be found and removed on next start.
inline constexpr const char *kBridgeName = "br-dlsim";
inline constexpr const char *kIfbName = "ifb-dlsim";
inline constexpr const char *kRootHandle = "1d:";       // prio root on every impaired device
inline constexpr int kIngressFilterPref = 4919;          // pref of the ingress->IFB redirect filter
inline constexpr int kMaxCustomClasses = 14;             // prio supports 16 bands, 2 are reserved
inline constexpr int kMaxClasses = 32;

enum class Direction { Uplink, Downlink }; // uplink = ground->air, downlink = air->ground
enum class Mode { Bridge, Host };
enum class Distribution { Uniform, Normal, Pareto, ParetoNormal };
enum class LossModel { Random, GilbertElliott };
enum class Proto { Any, Tcp, Udp, Icmp };
enum class ClassMode { Link, Custom, Excluded };

QString toString(Direction);
QString toString(Mode);
QString toString(Distribution);
QString toString(LossModel);
QString toString(Proto);
QString toString(ClassMode);
bool parse(const QString &, Mode *);
bool parse(const QString &, Distribution *);
bool parse(const QString &, LossModel *);
bool parse(const QString &, Proto *);
bool parse(const QString &, ClassMode *);

// One netem configuration. Percentages are 0..100.
struct Profile {
    double delayMs = 0;
    double jitterMs = 0;
    double delayCorrPct = 0;
    Distribution distribution = Distribution::Normal; // used only when jitter > 0

    LossModel lossModel = LossModel::Random;
    double lossPct = 0;       // random model
    double lossCorrPct = 0;   // random model
    // Gilbert-Elliott, as in `loss gemodel p r 1-h 1-k`
    double geP = 0;            // good -> bad transition probability
    double geR = 100;          // bad -> good transition probability
    double geBadLossPct = 100; // 1-h: loss probability in the bad state
    double geGoodLossPct = 0;  // 1-k: loss probability in the good state

    quint64 rateKbit = 0;  // 0 = unlimited
    quint32 limitPkts = 0; // 0 = auto (see TcCommandBuilder::autoLimit)

    double duplicatePct = 0;
    double reorderPct = 0;
    double reorderCorrPct = 0;
    quint32 reorderGap = 0;
    double corruptPct = 0;

    bool operator==(const Profile &) const = default;

    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject &o);
    QStringList validate(const QString &prefix = {}) const;
    bool hasLoss() const;
};

// Interpolation between two profiles, t in [0,1]: linear, except rate, which
// is interpolated on a log scale (an unlimited rate counts as 1 Gbit/s).
// Fields that cannot be blended (distribution, ...) switch to `b` at t >= 0.5.
Profile interpolate(const Profile &a, const Profile &b, double t);

// The same profile with 100% random loss: used for CUT LINK.
Profile cutProfile(const Profile &p);

struct TrafficClass {
    QString name;
    bool enabled = true;
    Proto proto = Proto::Any;
    QString ip;        // empty = any. Matches either source or destination (endpoint semantics)
    quint16 port = 0;  // 0 = any. Requires tcp or udp
    ClassMode mode = ClassMode::Link;
    Profile uplink;    // used when mode == Custom
    Profile downlink;

    bool operator==(const TrafficClass &) const = default;
    QJsonObject toJson() const;
    static TrafficClass fromJson(const QJsonObject &o);
    QStringList validate() const;
};

struct LinkConfig {
    Mode mode = Mode::Bridge;
    QString groundIface; // bridge: NIC facing the ground station
    QString airIface;    // bridge: NIC facing the vehicle
    QString hostIface;   // host: the single NIC
    Profile uplink;
    Profile downlink;
    QList<TrafficClass> classes;
    ClassMode otherMode = ClassMode::Link; // Link or Excluded
    bool cut = false;

    bool operator==(const LinkConfig &) const = default;
    QJsonObject toJson() const;
    static LinkConfig fromJson(const QJsonObject &o);
    // knownIfaces: names from `ip -j link`. Empty list skips the existence check.
    QStringList validate(const QStringList &knownIfaces = {}) const;
};

bool isValidIfaceName(const QString &name);
bool isValidIp(const QString &ip);

} // namespace dlsim
