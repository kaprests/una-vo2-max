/**
 ******************************************************************************
 * @file    Vo2Result.hpp
 * @brief   VO2 max estimate, shared by the service and the GUI.
 ******************************************************************************
 */

#ifndef VO2_RESULT_HPP
#define VO2_RESULT_HPP

#include <cstdint>

struct Vo2Result {
    enum class Confidence : uint8_t { NONE, LOW, MEDIUM, HIGH };

    enum class Reason : uint8_t {
        OK,
        NO_MAX_HR,
        TOO_SHORT,     ///< Not enough usable running.
        LOW_HR_RANGE,  ///< Paces too similar.
        POOR_FIT,      ///< Line too uncertain.
        BAD_SLOPE,     ///< HR does not rise with speed.
        OUT_OF_RANGE,  ///< Result not physiological.
    };

    Confidence confidence  = Confidence::NONE;
    Reason     reason      = Reason::TOO_SHORT;
    float      vo2max      = 0.0f; ///< ml/kg/min
    uint16_t   validSec    = 0;    ///< Seconds that went into the fit.
    float      hrRangeFrac = 0.0f; ///< (highest - lowest band HR) / max HR
    float      r2          = 0.0f; ///< Of the line through the band means.

    bool valid() const { return confidence != Confidence::NONE; }
};

#endif // VO2_RESULT_HPP
