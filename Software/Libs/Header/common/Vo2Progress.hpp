/**
 ******************************************************************************
 * @file    Vo2Progress.hpp
 * @brief   Confidence thresholds and live data-collection state of a run,
 *          shared by the service and the GUI.
 ******************************************************************************
 */

#ifndef VO2_PROGRESS_HPP
#define VO2_PROGRESS_HPP

#include <cstdint>

#include "Vo2Result.hpp"

/// Thresholds a result must meet to reach a confidence level.
struct Vo2Level {
    uint16_t steadySec;
    float    hrRangeFrac; ///< HR range / max HR
    float    r2;
};

inline constexpr Vo2Level kVo2Low   { 300, 0.10f, 0.30f};
inline constexpr Vo2Level kVo2Medium{ 600, 0.15f, 0.60f};
inline constexpr Vo2Level kVo2High  {1200, 0.20f, 0.80f};

struct Vo2Progress {
    enum class State : uint8_t {
        NO_MAX_HR,
        NO_HR,     ///< HR missing or untrusted.
        UNUSABLE,  ///< Too slow, too fast or on a hill.
        UNSTEADY,
        SETTLING,  ///< HR catching up after a pace change.
        COUNTING,
    };

    State                 state      = State::SETTLING;
    Vo2Result::Confidence confidence = Vo2Result::Confidence::NONE;
    uint8_t               settleSec  = 0; ///< Until counting starts.
    uint8_t               hrLow      = 0; ///< Counted HR range, bpm; 0 if none.
    uint8_t               hrHigh     = 0;
    uint16_t              steadySec  = 0; ///< Counted seconds.
    float                 r2         = 0.0f;
};

#endif // VO2_PROGRESS_HPP
