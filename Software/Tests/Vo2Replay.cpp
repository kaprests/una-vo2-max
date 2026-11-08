// Replays a recorded run through the estimator and guidance, printing what the
// run's data face would have shown. Input on stdin: seconds,speed_mps,hr_bpm,grade_pct
// (tools/fit_csv.py). Usage: vo2-replay <max HR> [print interval s]

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <numeric>

#include "Vo2Guidance.hpp"
#include "Vo2MaxEstimator.hpp"

namespace
{
constexpr std::size_t kSmoothSec = 10; // Service::skPaceSmoothingTicks

const char* stateName(Vo2Progress::State s)
{
    using S = Vo2Progress::State;
    switch (s) {
        case S::NO_MAX_HR: return "no max HR";
        case S::NO_HR:     return "no HR";
        case S::UNUSABLE:  return "unusable";
        case S::UNSTEADY:  return "unsteady";
        case S::SETTLING:  return "settling";
        case S::COUNTING:  return "counting";
    }
    return "";
}

const char* tipName(Vo2Guidance::Tip t)
{
    using T = Vo2Guidance::Tip;
    switch (t) {
        case T::SET_MAX_HR:  return "set max HR";
        case T::NO_HR:       return "weak HR";
        case T::FLAT_GROUND: return "flat ground";
        case T::HOLD_STEADY: return "hold steady";
        case T::SETTLING:    return "settling";
        case T::GO_FASTER:   return "faster";
        case T::GO_EASIER:   return "easier";
        case T::HR_OFF_PACE: return "HR off pace";
        case T::KEEP_GOING:  return "keep steady";
        case T::DONE:        return "good data";
    }
    return "";
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: vo2-replay <max HR> [interval s] < run.csv\n");
        return 1;
    }
    const float maxHr    = std::strtof(argv[1], nullptr);
    const long  interval = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 30;

    Vo2MaxEstimator   est;
    std::deque<float> speeds;
    long              t     = 0;
    float             speed = 0.0f, hr = 0.0f;
    char              grade[16] = {};

    std::printf("  time    HR  state       sec  band     target  tip\n");
    while (std::scanf("%ld,%f,%f,%15s", &t, &speed, &hr, grade) == 4) {
        const float gradePct   = std::strtof(grade, nullptr);
        const bool  gradeValid = !std::isnan(gradePct);
        speeds.push_back(speed);
        if (speeds.size() > kSmoothSec) {
            speeds.pop_front();
        }
        const float smooth = std::accumulate(speeds.begin(), speeds.end(), 0.0f) / speeds.size();
        est.addSample({hr, static_cast<uint8_t>(hr > 0 ? Vo2MaxEstimator::kMaxHrTrust : 0), smooth,
                       gradeValid ? gradePct : 0.0f, gradeValid});

        if (t % interval == 0) {
            const Vo2Progress p = est.progress(maxHr);
            const Vo2Guidance g = vo2Guidance(p, maxHr);
            std::printf("%3ld:%02ld  %4.0f  %-10s %4u  %3u-%-3u  %6u  %s\n", t / 60, t % 60, hr,
                        stateName(p.state), p.steadySec, p.hrLow, p.hrHigh, g.targetHr, tipName(g.tip));
        }
    }

    static const char* const kLevel[] = {"none", "Low", "Medium", "High"};
    const Vo2Result r = est.result(maxHr);
    std::printf("\nVO2 max %.1f  confidence %s  steady %u min  HR range %.0f%%  fit %.0f%%\n", r.vo2max,
                kLevel[static_cast<int>(r.confidence)], r.steadySec / 60u, r.hrRangeFrac * 100, r.r2 * 100);
    return 0;
}
