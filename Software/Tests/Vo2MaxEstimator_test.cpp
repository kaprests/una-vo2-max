#include "Vo2MaxEstimator.hpp"

#include <gtest/gtest.h>

#include <deque>
#include <numeric>
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

const std::vector<float> kPaces = {2.2f, 2.8f, 3.4f, 4.0f};

// Seconds before a steady stretch counts: window fill + settle.
constexpr int kLeadIn = static_cast<int>(Est::kWindowSec) - 1 + Est::Config{}.settleSec;

Est::Sample sample(float speed, float hr, float grade = 0.0f)
{
    Est::Sample s;
    s.speedMps   = speed;
    s.hrBpm      = hr;
    s.hrTrust    = Est::kMaxHrTrust;
    s.gradePct   = grade;
    s.gradeValid = true;
    return s;
}

/// Each pace for secPerPace seconds, HR exactly on the line.
void runPaces(Est& est, const std::vector<float>& paces, int secPerPace)
{
    for (float v : paces) {
        for (int i = 0; i < secPerPace; ++i) {
            est.addSample(sample(v, kA + kB * v));
        }
    }
}
} // namespace

TEST(Vo2MaxEstimator, CleanRunGivesExpectedVo2WithHighConfidence)
{
    Est est;
    runPaces(est, kPaces, 600);

    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::OK);
    EXPECT_EQ(r.confidence, Est::Confidence::HIGH);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 0.1f);
    EXPECT_NEAR(r.vMaxMps, 4.8f, 0.01f);
    EXPECT_NEAR(r.slope, kB, 0.01f);
    EXPECT_NEAR(r.r2, 1.0f, 1e-4f);
}

TEST(Vo2MaxEstimator, LeadInIsExcluded)
{
    Est est;
    runPaces(est, {3.0f}, 400);
    EXPECT_EQ(est.result(kMaxHr).steadySec, 400 - kLeadIn);
}

TEST(Vo2MaxEstimator, PaceChangeWaitsForSettle)
{
    Est est;
    runPaces(est, {3.0f}, 400);
    const auto before = est.result(kMaxHr).steadySec;
    runPaces(est, {3.6f}, Est::Config{}.settleSec);
    EXPECT_EQ(est.result(kMaxHr).steadySec, before);
}

TEST(Vo2MaxEstimator, NoisyDataStillEstimates)
{
    // GPS speed noise as the Service delivers it: smoothed over 10 s.
    Est est;
    std::mt19937 rng(42);
    std::normal_distribution<float> hrNoise(0.0f, 5.0f);
    std::normal_distribution<float> gpsNoise(0.0f, 0.3f);
    std::deque<float> window;
    for (float v : kPaces) {
        for (int i = 0; i < 600; ++i) {
            window.push_back(v + gpsNoise(rng));
            if (window.size() > 10) {
                window.pop_front();
            }
            const float smoothed = std::accumulate(window.begin(), window.end(), 0.0f) / window.size();
            est.addSample(sample(smoothed, kA + kB * v + hrNoise(rng)));
        }
    }

    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.reason, Est::Reason::OK);
    EXPECT_LT(r.r2, 0.99f);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 3.0f);
}

TEST(Vo2MaxEstimator, MaxHrNotSet)
{
    Est est;
    runPaces(est, kPaces, 600);
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
    EXPECT_EQ(est.result(kMaxHr).reason, Est::Reason::LOW_HR_RANGE);
}

TEST(Vo2MaxEstimator, HillsAreIgnored)
{
    Est est;
    runPaces(est, kPaces, 600);
    for (int i = 0; i < 600; ++i) {
        est.addSample(sample(2.5f, 185.0f, 8.0f));
    }
    EXPECT_NEAR(est.result(kMaxHr).vo2max, kExpectedVo2, 0.1f);
}

TEST(Vo2MaxEstimator, UnsteadyPaceIsIgnored)
{
    Est est;
    for (int i = 0; i < 2400; ++i) {
        const float v = (i / 10) % 2 ? 2.5f : 3.8f;
        est.addSample(sample(v, kA + kB * v));
    }
    EXPECT_EQ(est.result(kMaxHr).steadySec, 0);
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

TEST(Vo2MaxEstimator, ShortGapKeepsStretch)
{
    Est est;
    runPaces(est, {3.0f}, 400);
    for (int i = 0; i < Est::Config{}.maxGapSec; ++i) {
        est.addSample(sample(0.0f, 0.0f));
    }
    runPaces(est, {3.0f}, 100);
    EXPECT_EQ(est.result(kMaxHr).steadySec, 500 - kLeadIn);
}

TEST(Vo2MaxEstimator, LongGapBreaksStretch)
{
    Est est;
    runPaces(est, {3.0f}, 400);
    for (int i = 0; i <= Est::Config{}.maxGapSec; ++i) {
        est.addSample(sample(0.0f, 0.0f));
    }
    runPaces(est, {3.0f}, 100);
    EXPECT_EQ(est.result(kMaxHr).steadySec, 400 - kLeadIn + 100 - kLeadIn);
}

TEST(Vo2MaxEstimator, HrFallingWithSpeedIsRejected)
{
    Est est;
    for (float v : kPaces) {
        for (int i = 0; i < 600; ++i) {
            est.addSample(sample(v, 220.0f - kB * v));
        }
    }
    EXPECT_EQ(est.result(kMaxHr).reason, Est::Reason::BAD_SLOPE);
}

TEST(Vo2MaxEstimator, CustomConfigAcceptsShortRun)
{
    Est defaults;
    runPaces(defaults, {2.8f, 3.4f}, 150);
    EXPECT_EQ(defaults.result(kMaxHr).confidence, Est::Confidence::NONE);

    Est::Config cfg;
    cfg.settleSec = 0;
    cfg.minimum   = {60, 0.0f, 0.3f};
    Est est(cfg);
    runPaces(est, {2.8f, 3.4f}, 150);

    const auto r = est.result(kMaxHr);
    EXPECT_EQ(r.confidence, Est::Confidence::LOW);
    EXPECT_NEAR(r.vo2max, kExpectedVo2, 0.1f);
}

TEST(Vo2MaxEstimator, ResetClearsEverything)
{
    Est est;
    runPaces(est, kPaces, 600);
    est.reset();
    EXPECT_EQ(est.result(kMaxHr).steadySec, 0);
}
