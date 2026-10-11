/**
 ******************************************************************************
 * @file    ActivitySummary.hpp
 * @date    08-04-2025
 * @author  Denys Saienko <denys.saienko@droid-technologies.com>
 * @brief   Represents a common summary track information.
 ******************************************************************************
 *
 ******************************************************************************
 */

#ifndef ACTIVITY_SUMMARY_HPP
#define ACTIVITY_SUMMARY_HPP

#include <cstdint>
#include <ctime>
#include <vector>

#include "SDK/TrackMap/TrackMapScreen.hpp"

/**
 * @struct LapSummary
 * @brief Summary data for a single lap.
 */
struct LapSummary {
    time_t duration;  ///< Lap duration in seconds
    float  distance;  ///< Lap distance in m
    float  paceAvg;   ///< Average pace in s/m
};

/// VO2 max estimate, mirroring Vo2MaxEstimator's result for the GUI.
struct Vo2Summary {
    enum class Confidence : uint8_t { NONE, LOW, MEDIUM, HIGH };
    enum class Reason : uint8_t { OK, NO_MAX_HR, TOO_SHORT, LOW_HR_RANGE, POOR_FIT, BAD_SLOPE, OUT_OF_RANGE };

    Confidence confidence  = Confidence::NONE;
    Reason     reason      = Reason::TOO_SHORT;
    float      vo2max      = 0.0f; ///< ml/kg/min
    uint16_t   steadySec   = 0;
    float      hrRangeFrac = 0.0f;
    float      r2          = 0.0f;
};

/**
 * @struct ActivitySummary
 * @brief Represents a common summary track information.
 */
struct ActivitySummary {
    time_t utc;         ///< Last activity UTC time
    time_t time;        ///< Total track time in seconds
    float distance;     ///< Total track distance in m
    float speedAvg;     ///< Average speed in m/s
    float elevation;    ///< Accumulated elevation gain (ascent) in m
    float paceAvg;      ///< Average pace in s/m
    float hrMax;        ///< Maximum Heart Rate in bpm
    float hrAvg;        ///< Average Heart Rate in bpm
    SDK::TrackMapScreen map; ///< Track map

    std::vector<LapSummary> laps; ///< Per-lap summary data
    Vo2Summary vo2;
};

#endif // ACTIVITY_SUMMARY_HPP
