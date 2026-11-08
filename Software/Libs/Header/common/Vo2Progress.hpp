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

/// Thresholds a result must meet to reach a confidence level. They measure
/// how well the run determines the line, not how close it is to a lab value.
struct Vo2Level {
    float    hrRangeFrac; ///< Band HR range / max HR
    uint8_t  bands;       ///< 5 bpm HR bands with enough data
    uint16_t validSec;    ///< Seconds that went into the fit
};

inline constexpr Vo2Level kVo2Low   {0.15f, 5,  600};
inline constexpr Vo2Level kVo2Medium{0.20f, 7, 1200};
inline constexpr Vo2Level kVo2High  {0.25f, 9, 1500};

struct Vo2Progress {
    enum class State : uint8_t {
        NO_MAX_HR,
        NO_HR,       ///< HR missing or untrusted.
        UNUSABLE,    ///< Too slow, or on a steep hill.
        WARMING_UP,  ///< First seconds of the run, not used.
        COUNTING,
    };

    State                 state      = State::WARMING_UP;
    Vo2Result::Confidence confidence = Vo2Result::Confidence::NONE;
    Vo2Result::Reason     reason     = Vo2Result::Reason::TOO_SHORT;
    uint8_t               hrLow      = 0; ///< Band HR range, bpm; 0 if none.
    uint8_t               hrHigh     = 0;
    uint8_t               bands      = 0;
    uint16_t              validSec   = 0;
};

#endif // VO2_PROGRESS_HPP
