/**
 ******************************************************************************
 * @file    Vo2MaxEstimator.hpp
 * @brief   VO2 max from a submaximal run (heart rate vs. speed).
 *
 * Fed one sample per second. Keeps steady, flat, trusted samples, fits
 * HR = a + b * speed, extends the line to max HR, and converts that speed to
 * VO2 max with the ACSM running equation.
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

        float    minSpeedMps   = 1.8f;   ///< Slower is walking.
        float    maxSpeedMps   = 6.5f;   ///< Faster is sprinting or GPS error.
        float    maxGradePct   = 3.0f;
        float    minHrBpm      = 60.0f;
        uint8_t  minHrTrust    = 1;
        float    steadyDriftMps= 0.25f;  ///< Max change between window halves.
        float    steadyJumpMps = 0.5f;   ///< Max newest speed off window mean.
        uint16_t settleSec     = 60;     ///< HR lag after a pace change.
        uint8_t  maxGapSec     = 5;      ///< Unusable seconds skipped before a stretch breaks.
        float    minMaxHr      = 120.0f; ///< Lower max HR counts as not set.
        uint16_t minBinSec     = 20;     ///< Counted seconds before an HR bin widens the range.

        Level minimum = kVo2Low;
        Level medium  = kVo2Medium;
        Level high    = kVo2High;
    };

    struct Sample {
        float   hrBpm      = 0.0f;
        uint8_t hrTrust    = 0;
        float   speedMps   = 0.0f;
        float   gradePct   = 0.0f;
        bool    gradeValid = false;
    };

    static constexpr std::size_t kWindowSec  = 30;
    static constexpr uint8_t     kMaxHrTrust = 3; ///< Trust levels 1..3 are valid.

    Vo2MaxEstimator();
    explicit Vo2MaxEstimator(const Config& config);

    void        reset();
    void        addSample(const Sample& s);
    Vo2Result   result(float maxHrBpm) const;
    Vo2Progress progress(float maxHrBpm) const;

private:
    /// Least-squares fit of y on x, accumulated as running sums.
    struct LinearFit {
        uint32_t n = 0;
        double   sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;

        void   add(double x, double y);
        double varX() const  { return sxx - sx * sx / n; }
        double varY() const  { return syy - sy * sy / n; }
        double covXY() const { return sxy - sx * sy / n; }
        double slope() const { return covXY() / varX(); }
        double r2() const    { return covXY() * covXY() / (varX() * varY()); }
        double intercept(double slope) const { return (sy - slope * sx) / n; }
    };

    /// HR range of the counted samples: centres of the outermost bins with
    /// at least minBinSec, so brief spikes do not widen it.
    struct Band {
        float low  = 0.0f;
        float high = 0.0f;
    };

    static constexpr float       kBinMinBpm = 60.0f;
    static constexpr float       kBinBpm    = 5.0f;
    static constexpr std::size_t kBins      = 32; ///< Up to 220 bpm.

    bool hrUsable(const Sample& s) const;
    bool speedUsable(const Sample& s) const;
    bool windowSteady(float newest) const;
    void breakStretch();
    Band hrBand() const;

    Config mConfig;

    std::array<float, kWindowSec> mWindow{}; ///< Speed ring buffer.
    std::size_t mWindowCount = 0;
    std::size_t mWindowHead  = 0;            ///< Oldest entry once full.
    uint16_t    mSteadySec   = 0;
    uint8_t     mGapSec      = 0;

    LinearFit mFit; ///< x = speed, y = HR
    std::array<uint16_t, kBins> mHrBins{}; ///< Counted seconds per HR bin.
    Vo2Progress::State mState = Vo2Progress::State::SETTLING;
};

#endif // VO2MAX_ESTIMATOR_HPP
