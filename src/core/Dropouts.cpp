#include "Dropouts.h"

#include <algorithm>
#include <random>

namespace dlsim {

QList<DropoutWindow> dropoutSchedule(quint32 seed, double meanIntervalS, double meanDurationS, double totalS)
{
    std::mt19937 rng(seed);
    std::exponential_distribution<double> gap(1.0 / std::max(0.1, meanIntervalS));
    std::exponential_distribution<double> len(1.0 / std::max(0.1, meanDurationS));
    QList<DropoutWindow> out;
    double t = 0;
    for (;;) {
        t += gap(rng);
        if (t >= totalS)
            break;
        const double l = std::min(len(rng), totalS - t);
        out << DropoutWindow{t, l};
        t += l;
    }
    return out;
}

} // namespace dlsim
