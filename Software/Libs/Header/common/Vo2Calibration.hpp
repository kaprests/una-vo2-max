/**
 ******************************************************************************
 * @file    Vo2Calibration.hpp
 * @brief   Per-user calibration of VO2 max estimates against lab results.
 *
 * Each lab test with nearby runs becomes an anchor: the median, over those
 * runs, of the parameter that maps the run's estimate to the lab value. The
 * parameter is interpolated linearly between anchors and held at the nearest
 * one outside them, then applied to each estimate. The stored history stays
 * raw, so new lab tests recalibrate past estimates.
 ******************************************************************************
 */

#ifndef VO2_CALIBRATION_HPP
#define VO2_CALIBRATION_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>

#include "LabTests.hpp"
#include "Vo2History.hpp"

namespace Vo2Calibration
{
enum class Method : uint8_t { FACTOR, RUNNING_COST };

inline constexpr Method kMethod = Method::FACTOR;

/// How a parameter p maps a raw estimate to a calibrated one.
struct Model {
    float neutral;   ///< p that leaves estimates unchanged
    float min;       ///< Plausible range of p; anchors outside it are rejected.
    float max;
    float (*fit)(float raw, float lab);   ///< p with apply(raw, p) == lab
    float (*apply)(float raw, float p);
};

namespace Detail
{
inline constexpr float kRest = 3.5f;   ///< ACSM resting VO2, ml/kg/min
inline constexpr float kCost = 0.2f;   ///< ACSM running cost, ml/kg/m
} // namespace Detail

/// FACTOR scales the estimate. RUNNING_COST replaces the ACSM running cost and
/// keeps the resting term.
inline constexpr Model model(Method m)
{
    using namespace Detail;
    if (m == Method::RUNNING_COST) {
        return {kCost, 0.8f * kCost, 1.25f * kCost,
                [](float raw, float lab) { return (lab - kRest) / (raw - kRest) * kCost; },
                [](float raw, float p) { return kRest + (raw - kRest) * p / kCost; }};
    }
    return {1.0f, 0.8f, 1.25f,
            [](float raw, float lab) { return lab / raw; },
            [](float raw, float p) { return raw * p; }};
}

inline constexpr std::time_t kWindowSec = 42 * 86400;   ///< Runs paired with a lab test: +-6 weeks.

struct Anchor {
    std::time_t utc   = 0;
    float       param = 0.0f;
};

struct Anchors {
    std::array<Anchor, LabTests::kCapacity> items{};
    std::size_t count    = 0;
    std::size_t rejected = 0;
};

/// One anchor per lab test that has runs of Medium confidence or better
/// within kWindowSec. Oldest first.
inline Anchors anchors(const Vo2History& h, const LabTests& labs, const Model& m)
{
    Anchors a;
    std::array<float, Vo2History::kCapacity> fits{};
    for (std::size_t l = 0; l < labs.count; ++l) {
        const LabTests::Entry& lab = labs.entries[l];
        std::size_t n = 0;
        for (std::size_t i = 0; i < h.count; ++i) {
            const Vo2History::Entry& e = h.entries[i];
            if (e.result.confidence >= Vo2Result::Confidence::MEDIUM
                    && e.utc >= lab.utc - kWindowSec && e.utc <= lab.utc + kWindowSec) {
                fits[n++] = m.fit(e.result.vo2max, lab.vo2max);
            }
        }
        if (n == 0) {
            continue;
        }
        // Lower median for an even count: an actual run's value, not a blend.
        std::nth_element(fits.begin(), fits.begin() + (n - 1) / 2, fits.begin() + n);
        const float p = fits[(n - 1) / 2];
        if (p < m.min || p > m.max) {
            ++a.rejected;
            continue;
        }
        a.items[a.count++] = {lab.utc, p};
    }
    return a;
}

/// Linear between anchors, the nearest anchor's value outside them, neutral without any.
inline float paramAt(const Anchors& a, std::time_t utc, const Model& m)
{
    if (a.count == 0) {
        return m.neutral;
    }
    const Anchor* const begin = a.items.data();
    const Anchor* const end   = begin + a.count;
    const Anchor* const next  = std::lower_bound(begin, end, utc,
                                                 [](const Anchor& x, std::time_t t) { return x.utc < t; });
    if (next == begin) {
        return begin->param;
    }
    if (next == end) {
        return (end - 1)->param;
    }
    const Anchor& prev = *(next - 1);
    const float   f    = static_cast<float>(utc - prev.utc) / static_cast<float>(next->utc - prev.utc);
    return prev.param + f * (next->param - prev.param);
}

/// @p r with its VO2 max calibrated; unchanged if it has no estimate.
inline Vo2Result calibrate(Vo2Result r, std::time_t utc, const Anchors& a, const Model& m)
{
    if (r.valid()) {
        r.vo2max = m.apply(r.vo2max, paramAt(a, utc, m));
    }
    return r;
}
} // namespace Vo2Calibration

#endif // VO2_CALIBRATION_HPP
