// "Simple mode" for the link editor: a few plain controls mapped onto netem.
// Simple edits touch only their own fields; anything else in the profile is
// kept and reported by advancedOnlyFields(), so nothing is lost silently.
#pragma once

#include "Model.h"

#include <QStringList>
#include <QVariantMap>

namespace dlsim {

struct SimpleSettings {
    enum class Loss { None, Occasional, Bursty };
    double latencyMs = 0;
    double variationMs = 0;
    Loss loss = Loss::None;
    double lossPct = 0;        // average share of packets lost, 0..100
    double burstPackets = 10;  // bursty: mean length of a loss burst
    quint64 rateKbit = 0;      // 0 = unlimited

    QVariantMap toMap() const;
    static SimpleSettings fromMap(const QVariantMap &m);
};

// Average share of packets lost (0..100). Gilbert-Elliott: the steady-state
// mix of good and bad state losses.
double averageLossPct(const Profile &p);

SimpleSettings toSimple(const Profile &p);
// base with the simple fields replaced. Bursty loss uses Gilbert-Elliott with
// r = 100/burst, 1-h = 100%, 1-k = 0 and p chosen for the average loss.
Profile applySimple(Profile base, const SimpleSettings &s);
// Labels of fields set to something simple mode cannot show.
QStringList advancedOnlyFields(const Profile &p);
// "≈ 80 ms round trip · about 1 in 100 packets lost · max 2 Mbit/s up, 8 Mbit/s down"
QString summarize(const Profile &up, const Profile &down);

} // namespace dlsim
