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

class Vo2MaxEstimator
{
public:
    struct Config {
        /// Thresholds a result must meet to reach a confidence level.
        struct Level {
            uint16_t steadySec;
            float    hrRangeFrac; ///< (max - min HR) / max HR
            float    r2;
        };

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

        Level minimum{ 300, 0.10f, 0.30f};
        Level medium { 600, 0.15f, 0.60f};
        Level high   {1200, 0.20f, 0.80f};
    };

    struct Sample {
        float   hrBpm      = 0.0f;
        uint8_t hrTrust    = 0;
        float   speedMps   = 0.0f;
        float   gradePct   = 0.0f;
        bool    gradeValid = false;
    };

    enum class Confidence : uint8_t { NONE, LOW, MEDIUM, HIGH };

    enum class Reason : uint8_t {
        OK,
        NO_MAX_HR,
        TOO_SHORT,     ///< Not enough steady running.
        LOW_HR_RANGE,  ///< Paces too similar.
        POOR_FIT,      ///< HR does not follow speed.
        BAD_SLOPE,     ///< HR does not rise with speed.
        OUT_OF_RANGE,  ///< Result not physiological.
    };

    struct Result {
        Confidence confidence  = Confidence::NONE;
        Reason     reason      = Reason::TOO_SHORT;
        float      vo2max      = 0.0f; ///< ml/kg/min
        float      vMaxMps     = 0.0f; ///< Speed at max HR.
        uint16_t   steadySec   = 0;
        float      hrMin       = 0.0f;
        float      hrMax       = 0.0f;
        float      hrRangeFrac = 0.0f;
        float      r2          = 0.0f;
        float      slope       = 0.0f; ///< bpm per m/s
    };

    static constexpr std::size_t kWindowSec  = 30;
    static constexpr uint8_t     kMaxHrTrust = 3; ///< Trust levels 1..3 are valid.

    Vo2MaxEstimator();
    explicit Vo2MaxEstimator(const Config& config);

    void   reset();
    void   addSample(const Sample& s);
    Result result(float maxHrBpm) const;

private:
    /// Least-squares fit of y on x, accumulated as running sums.
    struct LinearFit {
        uint32_t n = 0;
        double   sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
        float    yMin = 0, yMax = 0;

        void   add(double x, double y);
        double varX() const  { return sxx - sx * sx / n; }
        double varY() const  { return syy - sy * sy / n; }
        double covXY() const { return sxy - sx * sy / n; }
        double slope() const { return covXY() / varX(); }
        double intercept() const { return (sy - slope() * sx) / n; }
        double r2() const    { return covXY() * covXY() / (varX() * varY()); }
    };

    bool usable(const Sample& s) const;
    bool windowSteady(float newest) const;
    void breakStretch();

    Config mConfig;

    std::array<float, kWindowSec> mWindow{}; ///< Speed ring buffer.
    std::size_t mWindowCount = 0;
    std::size_t mWindowHead  = 0;            ///< Oldest entry once full.
    uint16_t    mSteadySec   = 0;
    uint8_t     mGapSec      = 0;

    LinearFit mFit; ///< x = speed, y = HR
};

#endif // VO2MAX_ESTIMATOR_HPP
