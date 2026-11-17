// Delay and loss over a scenario's time, for the timeline graph. Uses the
// same interpolate() as the engine's fades, so the graph matches playback.
#pragma once

#include "Model.h"

#include <QJsonArray>
#include <QList>
#include <functional>

namespace dlsim {

struct CurvePoint {
    double t;            // seconds from the start
    double delayUpMs, delayDownMs;
    double lossUpPct, lossDownPct; // average loss; outages 100, dropouts their mean share
};

// presetLookup(name, &up, &down) -> false if unknown (the step then keeps the
// previous profiles). start: the profiles before the first step.
// samplesPerFade: points inside each fade step (other steps are flat).
QList<CurvePoint> scenarioCurve(const QJsonArray &steps,
                                const std::function<bool(const QString &, Profile *, Profile *)> &presetLookup,
                                const Profile &startUp, const Profile &startDown, int samplesPerFade = 20);

} // namespace dlsim
