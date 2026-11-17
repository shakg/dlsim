#include "SimpleMode.h"

#include <cmath>

namespace dlsim {

namespace {

QString loss(SimpleSettings::Loss l)
{
    switch (l) {
    case SimpleSettings::Loss::None: return QStringLiteral("none");
    case SimpleSettings::Loss::Occasional: return QStringLiteral("occasional");
    case SimpleSettings::Loss::Bursty: return QStringLiteral("bursty");
    }
    return {};
}

QString num(double v) { return QString::number(v, 'g', 3); }

QString rateText(quint64 kbit)
{
    if (kbit == 0)
        return QStringLiteral("unlimited");
    return kbit >= 1000 ? num(kbit / 1000.0) + " Mbit/s" : QString::number(kbit) + " kbit/s";
}

QString lossPhrase(double pct)
{
    if (pct <= 0)
        return QStringLiteral("no loss");
    if (pct >= 50)
        return QStringLiteral("%1% of packets lost").arg(num(pct));
    return QStringLiteral("about 1 in %1 packets lost").arg(qRound(100.0 / pct));
}

} // namespace

QVariantMap SimpleSettings::toMap() const
{
    return {{"latencyMs", latencyMs}, {"variationMs", variationMs}, {"loss", dlsim::loss(loss)},
            {"lossPct", lossPct},     {"burstPackets", burstPackets}, {"rateKbit", double(rateKbit)}};
}

SimpleSettings SimpleSettings::fromMap(const QVariantMap &m)
{
    SimpleSettings s;
    s.latencyMs = std::clamp(m.value("latencyMs").toDouble(), 0.0, 60000.0);
    s.variationMs = std::clamp(m.value("variationMs").toDouble(), 0.0, 60000.0);
    const QString l = m.value("loss").toString();
    s.loss = l == "bursty" ? Loss::Bursty : l == "occasional" ? Loss::Occasional : Loss::None;
    s.lossPct = std::clamp(m.value("lossPct").toDouble(), 0.0, 100.0);
    s.burstPackets = std::clamp(m.value("burstPackets", 10).toDouble(), 1.0, 10000.0);
    s.rateKbit = quint64(std::clamp(m.value("rateKbit").toDouble(), 0.0, 10'000'000.0));
    return s;
}

double averageLossPct(const Profile &p)
{
    if (p.lossModel == LossModel::Random)
        return p.lossPct;
    if (p.geP <= 0)
        return p.geGoodLossPct;
    const double bad = p.geR > 0 ? p.geP / (p.geP + p.geR) : 1.0; // share of time in the bad state
    return bad * p.geBadLossPct + (1 - bad) * p.geGoodLossPct;
}

SimpleSettings toSimple(const Profile &p)
{
    SimpleSettings s;
    s.latencyMs = p.delayMs;
    s.variationMs = p.jitterMs;
    s.rateKbit = p.rateKbit;
    s.lossPct = averageLossPct(p);
    if (p.lossModel == LossModel::GilbertElliott && p.geP > 0) {
        s.loss = SimpleSettings::Loss::Bursty;
        s.burstPackets = p.geR > 0 ? 100.0 / p.geR : 10000.0;
    } else {
        s.loss = s.lossPct > 0 ? SimpleSettings::Loss::Occasional : SimpleSettings::Loss::None;
    }
    return s;
}

Profile applySimple(Profile p, const SimpleSettings &s)
{
    p.delayMs = s.latencyMs;
    p.jitterMs = s.variationMs;
    p.rateKbit = s.rateKbit > 0 ? std::max<quint64>(s.rateKbit, 8) : 0;
    switch (s.loss) {
    case SimpleSettings::Loss::None:
        p.lossModel = LossModel::Random;
        p.lossPct = 0;
        p.geP = 0;
        break;
    case SimpleSettings::Loss::Occasional:
        p.lossModel = LossModel::Random;
        p.lossPct = s.lossPct;
        break;
    case SimpleSettings::Loss::Bursty: {
        p.lossModel = LossModel::GilbertElliott;
        const double r = std::clamp(100.0 / s.burstPackets, 0.01, 100.0);
        const double l = std::clamp(s.lossPct, 0.0, 99.0);
        p.geR = r;
        p.geP = std::min(100.0, r * l / (100.0 - l)); // bad-state share p/(p+r) = l/100
        p.geBadLossPct = 100;
        p.geGoodLossPct = 0;
        break;
    }
    }
    return p;
}

QStringList advancedOnlyFields(const Profile &p)
{
    QStringList f;
    if (p.jitterMs > 0 && p.delayCorrPct != 0)
        f << QStringLiteral("delay correlation");
    if (p.jitterMs > 0 && p.distribution != Distribution::Normal)
        f << QStringLiteral("%1 delay distribution").arg(toString(p.distribution));
    if (p.lossModel == LossModel::Random && p.lossPct > 0 && p.lossCorrPct != 0)
        f << QStringLiteral("loss correlation");
    if (p.lossModel == LossModel::GilbertElliott && p.geP > 0 && (p.geBadLossPct != 100 || p.geGoodLossPct != 0))
        f << QStringLiteral("Gilbert-Elliott state losses");
    if (p.limitPkts != 0)
        f << QStringLiteral("queue limit");
    if (p.duplicatePct > 0)
        f << QStringLiteral("duplication");
    if (p.reorderPct > 0)
        f << QStringLiteral("reordering");
    if (p.corruptPct > 0)
        f << QStringLiteral("corruption");
    return f;
}

QString summarize(const Profile &up, const Profile &down)
{
    QStringList parts;
    parts << QStringLiteral("≈ %1 ms round trip").arg(num(up.delayMs + down.delayMs));
    const double lu = averageLossPct(up), ld = averageLossPct(down);
    if (std::abs(lu - ld) < 1e-9)
        parts << lossPhrase(lu);
    else
        parts << QStringLiteral("uplink %1, downlink %2").arg(lossPhrase(lu), lossPhrase(ld));
    if (up.rateKbit == down.rateKbit)
        parts << (up.rateKbit ? QStringLiteral("max %1").arg(rateText(up.rateKbit)) : QStringLiteral("no rate limit"));
    else
        parts << QStringLiteral("max %1 up, %2 down").arg(rateText(up.rateKbit), rateText(down.rateKbit));
    return parts.join(QStringLiteral(" · "));
}

} // namespace dlsim
