/**
 ******************************************************************************
 * @file    Vo2Guidance.hpp
 * @brief   What the runner should do next to improve the estimate.
 *
 * The HR range of the run matters most. The runner is sent to kMaxFrac first,
 * since data near max HR shortens the extrapolation, then easier.
 ******************************************************************************
 */

#ifndef VO2_GUIDANCE_HPP
#define VO2_GUIDANCE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Vo2Progress.hpp"

struct Vo2Guidance {
    enum class Tip : uint8_t {
        SET_MAX_HR,
        NO_HR,
        FLAT_GROUND, ///< Too slow, or on a steep hill.
        GO_FASTER,
        GO_EASIER,
        HR_OFF_PACE, ///< Range is wide enough but the line is too uncertain.
        KEEP_GOING,  ///< Warming up, or more running needed.
        DONE,
    };

    static constexpr float kMinFrac = 0.60f; ///< Of max HR: lowest target.
    static constexpr float kMaxFrac = 0.90f; ///< Highest target.
    static constexpr int   kSlack   = 3;     ///< bpm a range end may fall short.
    static constexpr int   kOnTarget = 3;    ///< bpm either side of the target that count as on it.

    Tip     tip      = Tip::KEEP_GOING;
    uint8_t targetHr = 0; ///< HR to hold next, bpm; 0 if none.
};

/// Aims for kVo2Medium's HR range: wide enough for a good line, and what a
/// typical run of easy running plus a harder stretch can reach.
inline Vo2Guidance vo2Guidance(const Vo2Progress& p, float maxHrBpm)
{
    using State = Vo2Progress::State;
    using Tip   = Vo2Guidance::Tip;

    Vo2Guidance g{Tip::KEEP_GOING, 0};
    if (p.hrHigh > 0) {
        const int range  = static_cast<int>(std::lround(kVo2Medium.hrRangeFrac * maxHrBpm));
        const int cap    = static_cast<int>(Vo2Guidance::kMaxFrac * maxHrBpm);
        const int top    = std::min(p.hrLow + range, cap);
        const int bottom = std::max(p.hrHigh - range, static_cast<int>(Vo2Guidance::kMinFrac * maxHrBpm));
        if (p.hrHigh < top - Vo2Guidance::kSlack) {
            g = {Tip::GO_FASTER, static_cast<uint8_t>(cap)};
        } else if (p.hrLow > bottom + Vo2Guidance::kSlack) {
            g = {Tip::GO_EASIER, static_cast<uint8_t>(bottom)};
        } else if (p.reason == Vo2Result::Reason::POOR_FIT) {
            g.tip = Tip::HR_OFF_PACE;
        } else if (p.confidence >= Vo2Result::Confidence::MEDIUM) {
            g.tip = Tip::DONE;
        }
    }

    switch (p.state) {
        case State::NO_MAX_HR:  return {Tip::SET_MAX_HR, 0};
        case State::NO_HR:      g.tip = Tip::NO_HR;       break;
        case State::UNUSABLE:   g.tip = Tip::FLAT_GROUND; break;
        case State::WARMING_UP: g.tip = Tip::KEEP_GOING;  break;
        case State::COUNTING:   break;
    }
    return g;
}

#endif // VO2_GUIDANCE_HPP
