/**
 ******************************************************************************
 * @file    Vo2MaxEstimator.hpp
 * @brief   VO2 max from a submaximal run (heart rate vs. speed).
 *
 * Fed one sample per second. HR follows speed with a delay, so speed is passed
 * through a first-order lag before it is paired with HR. Usable seconds are
 * grouped into 5 bpm HR bands; a weighted line HR = a + b * speed through the
 * band means is extended to max HR, and that speed is converted to VO2 max
 * with the ACSM running equation. Capping each band's weight keeps long easy
 * running from outweighing short hard efforts.
 *
 * Speed is corrected for grade with the ACSM grade term (4.5 % more effort per
 * 1 % uphill). See data_analysis/RESULTS.md for how the method was chosen.
 *
 * No SDK dependencies; fixed memory regardless of run length.
 ******************************************************************************
 */

#ifndef VO2MAX_ESTIMATOR_HPP
#define VO2MAX_ESTIMATOR_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "Vo2Progress.hpp"
#include "Vo2Result.hpp"

class Vo2MaxEstimator
{
public:
    struct Config {
        using Level = Vo2Level;

        float    minSpeedMps    = 1.8f;   ///< Lagged speed; slower is walking.
        float    maxSpeedMps    = 6.5f;   ///< Faster is GPS error; the sample is skipped.
        float    maxGradePct    = 8.0f;   ///< Smoothed; steeper is excluded.
        float    minHrBpm       = 60.0f;
        uint8_t  minHrTrust     = 1;
        float    speedLagSec    = 40.0f;  ///< HR response to a change in speed.
        float    gradeLagSec    = 30.0f;  ///< Smooths barometric grade noise.
        uint16_t warmupSec      = 90;     ///< Start of the run, not used.
        uint16_t minBandSec     = 20;     ///< Shorter bands are ignored.
        uint16_t maxBandWeight  = 60;     ///< Band weight cap, seconds.
        float    maxVo2Se       = 4.0f;   ///< Larger standard error: no estimate.
        float    minMaxHr       = 120.0f; ///< Lower max HR counts as not set.

        Level low    = kVo2Low;
        Level medium = kVo2Medium;
        Level high   = kVo2High;
    };

    struct Sample {
        float   hrBpm      = 0.0f;
        uint8_t hrTrust    = 0;
        float   speedMps   = 0.0f;
        float   gradePct   = 0.0f;
        bool    gradeValid = false;
    };

    static constexpr uint8_t kMaxHrTrust = 3; ///< Trust levels 1..3 are valid.

    Vo2MaxEstimator();
    explicit Vo2MaxEstimator(const Config& config);

    void        reset();
    /// @p seconds without samples (a pause): the lags decay as if standing.
    void        skip(uint32_t seconds);
    void        addSample(const Sample& s);
    Vo2Result   result(float maxHrBpm) const;
    Vo2Progress progress(float maxHrBpm) const;

private:
    static constexpr float       kBandMinBpm = 60.0f;
    static constexpr float       kBandBpm    = 5.0f;
    static constexpr std::size_t kBands      = 32; ///< Up to 220 bpm.

    struct Band {
        uint16_t sec    = 0;
        float    speed  = 0.0f; ///< Sums
        float    hr     = 0.0f;
    };

    /// Bands with enough data, as weight and means.
    struct Point {
        float weight = 0.0f;
        float speed  = 0.0f;
        float hr     = 0.0f;
    };
    struct Points {
        std::array<Point, kBands> items{};
        uint8_t                   count = 0;
        float                     low   = 0.0f; ///< Lowest and highest mean HR
        float                     high  = 0.0f;
    };

    Points points() const;

    Config      mConfig;
    float       mSpeed    = 0.0f; ///< Lagged, grade-corrected
    float       mGrade    = 0.0f; ///< Smoothed
    uint32_t    mElapsed  = 0;
    uint32_t    mValidSec = 0;
    std::array<Band, kBands> mBands{};
    Vo2Progress::State mState = Vo2Progress::State::WARMING_UP;
};

#endif // VO2MAX_ESTIMATOR_HPP
