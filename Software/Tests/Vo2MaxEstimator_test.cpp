#include "Vo2MaxEstimator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

namespace
{
using Est        = Vo2MaxEstimator;
using Confidence = Vo2Result::Confidence;
using Reason     = Vo2Result::Reason;

constexpr float kMaxHr = 190.0f;

// HR = 70 + 25 * lagged speed  ->  speed at max HR = 4.8 m/s  ->  VO2 = 3.5 + 12 * 4.8 = 61.1
constexpr float kA = 70.0f;
constexpr float kB = 25.0f;
constexpr float kExpectedVo2 = 61.1f;

/// A runner whose HR follows speed through the same lag as the estimator.
struct Runner {
    float lagged = 0.0f;
    float noise  = 0.0f; ///< HR noise SD, bpm
    std::mt19937 rng{7};

    void run(Est& est, float speed, int seconds, float grade = 0.0f)
    {
        const float a = 1.0f - std::exp(-1.0f / Est::Config{}.speedLagSec);
        std::normal_distribution<float> hrNoise;
        for (int i = 0; i < seconds; ++i) {
            lagged += a * (speed - lagged);
            const float hr = kA + kB * lagged + noise * hrNoise(rng);
            est.addSample({hr, Est::kMaxHrTrust, speed, grade, true});
        }
    }

    /// Speed rising evenly from @p from to @p to over @p seconds.
    void ramp(Est& est, float from, float to, int seconds)
    {
        for (int i = 0; i < seconds; ++i) {
            run(est, from + (to - from) * i / seconds, 1);
        }
    }
};
} // namespace

TEST(Vo2MaxEstimator, CleanRampGivesExpectedVo2WithHighConfidence)
{
    Est est;
    Runner().ramp(est, 2.0f, 4.4f, 2400);
    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Reason::OK);
    EXPECT_EQ(r.confidence, Confidence::HIGH);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 0.3f);
    EXPECT_GT(r.r2, 0.99f);
}

TEST(Vo2MaxEstimator, FasterAtTheSameHrScoresProportionallyHigher)
{
    Est base, fast;
    Runner a, b;
    a.ramp(base, 2.0f, 4.4f, 2400);
    for (int i = 0; i < 2400; ++i) {
        const float v = 2.0f + 2.4f * i / 2400;
        b.lagged += (1.0f - std::exp(-1.0f / Est::Config{}.speedLagSec)) * (v - b.lagged);
        fast.addSample({kA + kB * b.lagged, Est::kMaxHrTrust, 1.02f * v, 0.0f, false});
    }
    const float v0 = base.result(kMaxHr).vo2max, v1 = fast.result(kMaxHr).vo2max;
    EXPECT_NEAR(v1 - 3.5f, 1.02f * (v0 - 3.5f), 0.1f);
}

TEST(Vo2MaxEstimator, ConfidenceGrowsWithRunningTime)
{
    const auto confidenceAfter = [](int seconds) {
        Est est;
        Runner().ramp(est, 2.0f, 4.4f, seconds);
        return est.result(kMaxHr).confidence;
    };
    EXPECT_EQ(confidenceAfter(12 * 60), Confidence::LOW);
    EXPECT_EQ(confidenceAfter(23 * 60), Confidence::MEDIUM);
    EXPECT_EQ(confidenceAfter(28 * 60), Confidence::HIGH);
}

TEST(Vo2MaxEstimator, LongEasyRunningDoesNotShiftTheEstimate)
{
    const auto withEasy = [](int easySeconds) {
        Est est;
        Runner r;
        r.noise = 5.0f;
        r.ramp(est, 2.0f, 4.4f, 1800);
        r.run(est, 2.6f, easySeconds);
        return est.result(kMaxHr).vo2max;
    };
    EXPECT_NEAR(withEasy(5 * 60), withEasy(40 * 60), 0.5f);
}

TEST(Vo2MaxEstimator, SinglePaceGivesNoEstimate)
{
    Est est;
    Runner r;
    r.noise = 2.5f;
    r.run(est, 3.0f, 2400);
    const auto res = est.result(kMaxHr);
    EXPECT_EQ(res.reason, Reason::LOW_HR_RANGE);
    EXPECT_EQ(res.confidence, Confidence::NONE);
}

TEST(Vo2MaxEstimator, MaxHrNotSet)
{
    Est est;
    Runner().ramp(est, 2.0f, 4.4f, 2400);
    const auto r = est.result(0.0f);
    EXPECT_EQ(r.reason, Reason::NO_MAX_HR);
    EXPECT_EQ(r.confidence, Confidence::NONE);
}

TEST(Vo2MaxEstimator, TooShort)
{
    Est est;
    Runner().ramp(est, 2.0f, 4.4f, 5 * 60);
    EXPECT_EQ(est.result(kMaxHr).reason, Reason::TOO_SHORT);
}

TEST(Vo2MaxEstimator, HrFallingWithSpeedIsRejected)
{
    Est est;
    float lagged = 0.0f;
    for (int i = 0; i < 2400; ++i) {
        const float v = 2.0f + 2.4f * i / 2400;
        lagged += (1.0f - std::exp(-1.0f / Est::Config{}.speedLagSec)) * (v - lagged);
        est.addSample({240.0f - kB * lagged, Est::kMaxHrTrust, v, 0.0f, false});
    }
    EXPECT_EQ(est.result(kMaxHr).reason, Reason::BAD_SLOPE);
}

TEST(Vo2MaxEstimator, SteepHillsAreIgnoredOnceTheSmoothedGradeIsSteep)
{
    Est est;
    Runner r;
    r.ramp(est, 2.0f, 4.4f, 2400);
    const uint16_t before = est.result(kMaxHr).validSec;
    r.run(est, 2.5f, 600, 12.0f);
    EXPECT_LT(est.result(kMaxHr).validSec - before, 2 * Est::Config{}.gradeLagSec);
}

TEST(Vo2MaxEstimator, UphillCountsAsFasterRunning)
{
    // Same HR response to effort, half the run on a 2 % slope: the grade term
    // turns it back into the effort of the flat run.
    Est flat, hill;
    Runner a, b;
    a.ramp(flat, 2.0f, 4.4f, 2400);
    const float lagStep = 1.0f - std::exp(-1.0f / Est::Config{}.speedLagSec);
    float effort = 0.0f, grade = 0.0f;
    for (int i = 0; i < 2400; ++i) {
        const float g = (i / 300) % 2 ? 2.0f : 0.0f;
        grade += (1.0f - std::exp(-1.0f / Est::Config{}.gradeLagSec)) * (g - grade);
        const float v = 2.0f + 2.4f * i / 2400;
        effort += lagStep * (v * (1.0f + 0.045f * grade) - effort);
        hill.addSample({kA + kB * effort, Est::kMaxHrTrust, v, g, true});
    }
    EXPECT_NEAR(hill.result(kMaxHr).vo2max, flat.result(kMaxHr).vo2max, 0.3f);
}

TEST(Vo2MaxEstimator, UntrustedHrIsIgnored)
{
    Est est;
    for (int i = 0; i < 2400; ++i) {
        est.addSample({150.0f, 0, 3.0f, 0.0f, false});
    }
    EXPECT_EQ(est.result(kMaxHr).validSec, 0);
}

TEST(Vo2MaxEstimator, BriefHrSpikeDoesNotWidenRange)
{
    Est est;
    Runner r;
    r.ramp(est, 2.0f, 4.4f, 2400);
    const float range = est.result(kMaxHr).hrRangeFrac;
    for (int i = 0; i < 10; ++i) {
        est.addSample({185.0f, Est::kMaxHrTrust, 3.0f, 0.0f, false});
    }
    EXPECT_FLOAT_EQ(est.result(kMaxHr).hrRangeFrac, range);
}

TEST(Vo2MaxEstimator, ProgressTracksCollection)
{
    using State = Vo2Progress::State;
    Est est;
    EXPECT_EQ(est.progress(0.0f).state, State::NO_MAX_HR);

    Runner r;
    r.run(est, 3.0f, 10);
    EXPECT_EQ(est.progress(kMaxHr).state, State::WARMING_UP);

    r.run(est, 3.0f, 400);
    const auto p = est.progress(kMaxHr);
    EXPECT_EQ(p.state, State::COUNTING);
    EXPECT_EQ(p.validSec, 410 - Est::Config{}.warmupSec);
    EXPECT_EQ(p.bands, 1);

    est.addSample({0.0f, 0, 3.0f, 0.0f, false});
    EXPECT_EQ(est.progress(kMaxHr).state, State::NO_HR);
    est.skip(600);
    est.addSample({150.0f, Est::kMaxHrTrust, 1.0f, 0.0f, false});
    EXPECT_EQ(est.progress(kMaxHr).state, State::UNUSABLE);
}

TEST(Vo2MaxEstimator, ResetClearsEverything)
{
    Est est;
    Runner().ramp(est, 2.0f, 4.4f, 2400);
    est.reset();
    EXPECT_EQ(est.result(kMaxHr).validSec, 0);
}
