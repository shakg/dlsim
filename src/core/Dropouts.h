// Random dropouts as a schedule: exponential gaps and lengths (a Poisson
// process), generated up front from a seed so a run can be replayed.
#pragma once

#include <QList>
#include <QtGlobal>

namespace dlsim {

struct DropoutWindow {
    double startS;
    double lengthS;
    double endS() const { return startS + lengthS; }
};

// Windows within [0, totalS), in order, not overlapping. Means are clamped to
// at least 0.1 s; the last window is cut at totalS.
QList<DropoutWindow> dropoutSchedule(quint32 seed, double meanIntervalS, double meanDurationS, double totalS);

} // namespace dlsim
