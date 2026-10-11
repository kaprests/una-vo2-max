#include "Vo2MaxEstimator.hpp"

#include <gtest/gtest.h>

#include <random>
#include <vector>

namespace
{
using Est = Vo2MaxEstimator;

constexpr float kMaxHr = 190.0f;

// HR = 70 + 25 * v  ->  v at max HR = 4.8 m/s  ->  VO2 = 3.5 + 0.2 * 288 = 61.1
constexpr float kA = 70.0f;
constexpr float kB = 25.0f;
constexpr float kExpectedVo2 = 61.1f;

// Samples consumed before a steady stretch counts: window fill + settle.
constexpr int kLeadIn = static_cast<int>(Est::kWindowSec) - 1 + Est::Config{}.settleSec;

Est::Sample sample(float speed, float hr, float grade = 0.0f)
{
    Est::Sample s;
    s.speedMs    = speed;
    s.hrBpm      = hr;
    s.hrTrust    = 3;
    s.gradePct   = grade;
    s.gradeValid = true;
    return s;
}

// Run each pace for secPerPace seconds with HR exactly on the line.
void runPaces(Est& est, const std::vector<float>& paces, int secPerPace)
{
    for (float v : paces) {
        for (int i = 0; i < secPerPace; ++i) {
            est.addSample(sample(v, kA + kB * v));
        }
    }
}
} // namespace

TEST(Vo2MaxEstimator, CleanMultiPaceRunGivesExpectedVo2WithHighConfidence)
{
    Est est;
    runPaces(est, {2.2f, 2.8f, 3.4f, 4.0f}, 600);

    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::OK);
    EXPECT_EQ(r.confidence, Est::Confidence::HIGH);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 0.1f);
    EXPECT_NEAR(r.vMaxMs, 4.8f, 0.01f);
    EXPECT_NEAR(r.slope, kB, 0.01f);
    EXPECT_NEAR(r.r2, 1.0f, 1e-4f);
    // Each pace change restarts the settle wait, so every pace loses at
    // least the settle time; the first also loses the window fill.
    EXPECT_GE(r.steadySec, 4 * (600 - kLeadIn));
    EXPECT_LE(r.steadySec, 600 - kLeadIn + 3 * (600 - Est::Config{}.settleSec));
}

TEST(Vo2MaxEstimator, PaceChangeIsDetectedOnItsFirstSecond)
{
    Est est;
    runPaces(est, {3.0f}, 400);
    const auto before = est.result(kMaxHr).steadySec;
    // A 0.6 m/s step: the new pace must not count until it has settled.
    runPaces(est, {3.6f}, Est::Config{}.settleSec);
    EXPECT_EQ(est.result(kMaxHr).steadySec, before);
}

TEST(Vo2MaxEstimator, LeadInSamplesAreExcluded)
{
    Est est;
    runPaces(est, {3.0f}, 400);
    EXPECT_EQ(est.result(kMaxHr).steadySec, 400 - kLeadIn);
}

TEST(Vo2MaxEstimator, NoisyHrStillEstimatesWithLowerFit)
{
    Est est;
    std::mt19937 rng(42);
    std::normal_distribution<float> hrNoise(0.0f, 5.0f);
    std::normal_distribution<float> speedNoise(0.0f, 0.08f);
    for (float v : {2.2f, 2.8f, 3.4f, 4.0f}) {
        for (int i = 0; i < 600; ++i) {
            const float speed = v + speedNoise(rng);
            est.addSample(sample(speed, kA + kB * v + hrNoise(rng)));
        }
    }

    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::OK);
    EXPECT_NE(r.confidence, Est::Confidence::NONE);
    EXPECT_LT(r.r2, 0.99f);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 3.0f);
}

TEST(Vo2MaxEstimator, MaxHrNotSet)
{
    Est est;
    runPaces(est, {2.2f, 2.8f, 3.4f, 4.0f}, 600);
    const auto r = est.result(0.0f);
    EXPECT_EQ(r.reason, Est::Reason::NO_MAX_HR);
    EXPECT_EQ(r.confidence, Est::Confidence::NONE);
    EXPECT_EQ(r.vo2max, 0.0f);
}

TEST(Vo2MaxEstimator, TooShort)
{
    Est est;
    runPaces(est, {3.0f}, 200);
    EXPECT_EQ(est.result(kMaxHr).reason, Est::Reason::TOO_SHORT);
}

TEST(Vo2MaxEstimator, SinglePaceHasTooLittleHrRange)
{
    Est est;
    runPaces(est, {3.0f}, 1200);
    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::LOW_HR_RANGE);
    EXPECT_EQ(r.confidence, Est::Confidence::NONE);
}

TEST(Vo2MaxEstimator, HillsAreIgnored)
{
    Est est;
    runPaces(est, {2.2f, 2.8f, 3.4f, 4.0f}, 600);
    // A long hill with HR far off the line must not change the estimate.
    for (int i = 0; i < 600; ++i) {
        est.addSample(sample(2.5f, 185.0f, 8.0f));
    }
    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::OK);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 0.1f);
}

TEST(Vo2MaxEstimator, UnsteadyPaceIsIgnored)
{
    Est est;
    for (int i = 0; i < 2400; ++i) {
        const float v = (i / 10) % 2 ? 2.5f : 3.8f;   // switches every 10 s
        est.addSample(sample(v, kA + kB * v));
    }
    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.steadySec, 0);
    EXPECT_EQ(r.reason, Est::Reason::TOO_SHORT);
}

TEST(Vo2MaxEstimator, UntrustedHrIsIgnored)
{
    Est est;
    for (int i = 0; i < 1200; ++i) {
        auto s = sample(3.0f, kA + kB * 3.0f);
        s.hrTrust = 0;
        est.addSample(s);
    }
    EXPECT_EQ(est.result(kMaxHr).steadySec, 0);
}

TEST(Vo2MaxEstimator, HrFallingWithSpeedIsRejected)
{
    Est est;
    for (float v : {2.2f, 2.8f, 3.4f, 4.0f}) {
        for (int i = 0; i < 600; ++i) {
            est.addSample(sample(v, 220.0f - kB * v));
        }
    }
    EXPECT_EQ(est.result(kMaxHr).reason, Est::Reason::BAD_SLOPE);
}

TEST(Vo2MaxEstimator, CustomConfigIsUsed)
{
    Est::Config cfg;
    cfg.settleSec       = 0;
    cfg.minSteadySec    = 60;
    cfg.minHrRangeFrac  = 0.0f;
    Est est(cfg);
    runPaces(est, {2.8f, 3.4f}, 120);

    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::OK);
    EXPECT_EQ(r.confidence, Est::Confidence::LOW);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 0.1f);
    // First pace loses only the window fill; the change drops the seconds
    // whose window is visibly unsteady, with no settle wait after.
    constexpr int firstPace = 120 - (static_cast<int>(Est::kWindowSec) - 1);
    EXPECT_GT(r.steadySec, firstPace);
    EXPECT_LT(r.steadySec, 2 * 120 - (static_cast<int>(Est::kWindowSec) - 1));
}

TEST(Vo2MaxEstimator, ResetClearsEverything)
{
    Est est;
    runPaces(est, {2.2f, 2.8f, 3.4f, 4.0f}, 600);
    est.reset();
    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.steadySec, 0);
    EXPECT_EQ(r.reason, Est::Reason::TOO_SHORT);
}
