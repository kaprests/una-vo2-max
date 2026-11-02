/**
 ******************************************************************************
 * @file    Vo2Guidance.hpp
 * @brief   What the runner should do next to improve the estimate.
 *
 * The range of counted HR matters most, so the runner is sent faster first
 * (a shorter extrapolation to max HR), within kMaxFrac, then easier.
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
        FLAT_GROUND, ///< Too slow, too fast or on a hill.
        HOLD_STEADY,
        SETTLING,
        GO_FASTER,
        GO_EASIER,
        HR_OFF_PACE, ///< Range is wide enough but HR does not follow speed.
        KEEP_GOING,  ///< Range is wide enough; more steady time needed.
        DONE,
    };

    static constexpr float kMinFrac = 0.60f; ///< Of max HR: lowest target.
    static constexpr float kMaxFrac = 0.90f; ///< Highest target.
    static constexpr int   kSlack   = 3;     ///< bpm a range end may fall short.

    Tip     tip      = Tip::KEEP_GOING;
    uint8_t targetHr = 0; ///< HR to hold next, bpm; 0 if none.
};

inline Vo2Guidance vo2Guidance(const Vo2Progress& p, float maxHrBpm)
{
    using State = Vo2Progress::State;
    using Tip   = Vo2Guidance::Tip;

    Vo2Guidance g{Tip::KEEP_GOING, 0};
    if (p.hrHigh > 0) {
        const int range  = static_cast<int>(std::lround(kVo2High.hrRangeFrac * maxHrBpm));
        const int top    = std::min(p.hrLow + range, static_cast<int>(Vo2Guidance::kMaxFrac * maxHrBpm));
        const int bottom = std::max(p.hrHigh - range, static_cast<int>(Vo2Guidance::kMinFrac * maxHrBpm));
        if (p.hrHigh < top - Vo2Guidance::kSlack) {
            g = {Tip::GO_FASTER, static_cast<uint8_t>(top)};
        } else if (p.hrLow > bottom + Vo2Guidance::kSlack) {
            g = {Tip::GO_EASIER, static_cast<uint8_t>(bottom)};
        } else if (p.r2 < kVo2High.r2 && p.steadySec >= kVo2Low.steadySec) {
            g.tip = Tip::HR_OFF_PACE;
        } else {
            g.tip = p.confidence == Vo2Result::Confidence::HIGH ? Tip::DONE : Tip::KEEP_GOING;
        }
    }

    switch (p.state) {
        case State::NO_MAX_HR: return {Tip::SET_MAX_HR, 0};
        case State::NO_HR:     g.tip = Tip::NO_HR;       break;
        case State::UNUSABLE:  g.tip = Tip::FLAT_GROUND; break;
        case State::UNSTEADY:  g.tip = Tip::HOLD_STEADY; break;
        case State::SETTLING:  g.tip = Tip::SETTLING;    break;
        case State::COUNTING:  break;
    }
    return g;
}

#endif // VO2_GUIDANCE_HPP
