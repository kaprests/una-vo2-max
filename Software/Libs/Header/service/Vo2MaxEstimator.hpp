/**
 ******************************************************************************
 * @file    Vo2MaxEstimator.hpp
 * @brief   VO2 max from a submaximal run (heart rate vs. speed).
 *
 * Fed one sample per second during a run. Keeps only steady, flat, trusted
 * samples, fits HR = a + b * speed by least squares, extends the line to the
 * user's max HR to get the speed at max effort, and converts that speed to
 * VO2 max with the ACSM running equation (VO2 = 3.5 + 0.2 * speed[m/min]).
 *
 * Plain C++ with no SDK dependencies, so it can be unit tested on the host.
 * Memory is fixed: running sums for the fit plus a small speed window.
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
    /// Tunable thresholds. Defaults are a first guess, to be tuned on real runs.
    struct Config {
        // Sample filter
        float    minSpeedMs       = 1.8f;  ///< Below this is walking, not running.
        float    maxSpeedMs       = 6.5f;  ///< Above this is sprinting or GPS error.
        float    maxGradePct      = 3.0f;  ///< |grade| above this is a hill.
        bool     requireGrade     = false; ///< Drop samples without a valid grade.
        float    minHrBpm         = 60.0f;
        uint8_t  minHrTrust       = 1;     ///< Trusted levels are minHrTrust..3.

        // Steadiness, checked every second over the last kWindowSec speeds:
        // the means of the window's two halves differ by at most
        // steadyDriftMs, and the newest speed is within steadyJumpMs of the
        // window mean (catches a pace change on its first second). Samples
        // count only after settleSec consecutive steady seconds, so HR can
        // catch up with a pace change.
        float    steadyDriftMs    = 0.25f;
        float    steadyJumpMs     = 0.5f;
        uint16_t settleSec        = 60;

        // Result
        uint16_t minSteadySec     = 300;   ///< Less than this: no estimate.
        float    minHrRangeFrac   = 0.10f; ///< HR spread as fraction of max HR; less: no estimate.
        float    minR2            = 0.30f; ///< Fit quality below this: no estimate.
        float    minMaxHr         = 120.0f;///< Max HR below this counts as not set.

        // Confidence levels (all must hold for a level)
        uint16_t mediumSteadySec  = 600;
        float    mediumHrRangeFrac= 0.15f;
        float    mediumR2         = 0.60f;
        uint16_t highSteadySec    = 1200;
        float    highHrRangeFrac  = 0.20f;
        float    highR2           = 0.80f;
    };

    struct Sample {
        float   hrBpm      = 0.0f;
        uint8_t hrTrust    = 0;
        float   speedMs    = 0.0f;
        float   gradePct   = 0.0f;
        bool    gradeValid = false;
    };

    enum class Confidence : uint8_t { NONE = 0, LOW, MEDIUM, HIGH };

    /// Why there is no estimate (Confidence::NONE).
    enum class Reason : uint8_t {
        OK = 0,
        NO_MAX_HR,       ///< Max HR not set or implausible.
        TOO_SHORT,       ///< Not enough steady running.
        LOW_HR_RANGE,    ///< Paces too similar to fit a line.
        POOR_FIT,        ///< HR does not follow speed (noise, drift).
        BAD_SLOPE,       ///< HR does not rise with speed.
        OUT_OF_RANGE,    ///< Result outside physiological limits.
    };

    struct Result {
        Confidence confidence = Confidence::NONE;
        Reason     reason     = Reason::TOO_SHORT;
        float      vo2max     = 0.0f; ///< ml/kg/min; 0 when confidence is NONE.
        float      vMaxMs     = 0.0f; ///< Extrapolated speed at max HR.
        uint16_t   steadySec  = 0;    ///< Samples used in the fit.
        float      hrMin      = 0.0f;
        float      hrMax      = 0.0f;
        float      hrRangeFrac= 0.0f; ///< (hrMax - hrMin) / max HR.
        float      r2         = 0.0f;
        float      slope      = 0.0f; ///< bpm per m/s.
    };

    static constexpr std::size_t kWindowSec = 30;

    Vo2MaxEstimator();
    explicit Vo2MaxEstimator(const Config& config);

    void reset();
    void addSample(const Sample& s);
    Result result(float maxHrBpm) const;

    const Config& config() const { return mConfig; }

private:
    bool isUsable(const Sample& s) const;
    void breakStretch();

    Config mConfig;

    // Speed window for the steadiness check (ring buffer).
    std::array<float, kWindowSec> mWindow{};
    std::size_t mWindowCount = 0;
    std::size_t mWindowHead  = 0;
    uint16_t    mSteadyRun   = 0; ///< Consecutive seconds with a steady window.

    // Least-squares sums over accepted samples (x = speed, y = HR).
    uint32_t mN   = 0;
    double   mSx  = 0, mSy = 0, mSxx = 0, mSxy = 0, mSyy = 0;
    float    mHrMin = 0.0f, mHrMax = 0.0f;
};

#endif // VO2MAX_ESTIMATOR_HPP
