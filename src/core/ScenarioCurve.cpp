#include "ScenarioCurve.h"

#include "SimpleMode.h"

namespace dlsim {

QList<CurvePoint> scenarioCurve(const QJsonArray &steps,
                                const std::function<bool(const QString &, Profile *, Profile *)> &presetLookup,
                                const Profile &startUp, const Profile &startDown, int samplesPerFade)
{
    QList<CurvePoint> out;
    Profile up = startUp, down = startDown;
    double t = 0;
    auto point = [&](double at, const Profile &u, const Profile &d, double extraLoss = -1) {
        CurvePoint p{at, u.delayMs, d.delayMs, averageLossPct(u), averageLossPct(d)};
        if (extraLoss >= 0) {
            // Outage / dropout share on top of the profile's own loss.
            p.lossUpPct = 100 - (100 - p.lossUpPct) * (1 - extraLoss / 100);
            p.lossDownPct = 100 - (100 - p.lossDownPct) * (1 - extraLoss / 100);
        }
        out << p;
    };
    for (const auto &v : steps) {
        const QJsonObject s = v.toObject();
        const QString type = s.value("type").toString();
        const double dur = s.value("durationS").toDouble();
        if (type == "outage") {
            point(t, up, down, 100);
            point(t + dur, up, down, 100);
        } else if (type == "dropouts") {
            const double gap = std::max(0.1, s.value("meanIntervalS").toDouble(30));
            const double len = std::max(0.1, s.value("meanDurationS").toDouble(3));
            const double share = 100 * len / (gap + len);
            point(t, up, down, share);
            point(t + dur, up, down, share);
        } else {
            Profile toUp, toDown;
            const bool known = presetLookup(s.value("preset").toString(), &toUp, &toDown);
            if (!known) {
                toUp = up;
                toDown = down;
            }
            if (type == "fade") {
                const int n = std::max(2, samplesPerFade);
                for (int i = 0; i <= n; ++i) {
                    const double k = double(i) / n;
                    point(t + k * dur, interpolate(up, toUp, k), interpolate(down, toDown, k));
                }
            } else {
                point(t, toUp, toDown);
                point(t + dur, toUp, toDown);
            }
            up = toUp;
            down = toDown;
        }
        t += dur;
    }
    return out;
}

} // namespace dlsim
