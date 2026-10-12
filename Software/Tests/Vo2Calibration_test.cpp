#include "Vo2Calibration.hpp"

#include <gtest/gtest.h>

namespace
{
using namespace Vo2Calibration;
using Confidence = Vo2Result::Confidence;

constexpr std::time_t kDay = 86400;

Vo2History::Entry run(std::time_t day, float vo2, Confidence c = Confidence::HIGH)
{
    Vo2History::Entry e;
    e.utc               = day * kDay;
    e.result.confidence = c;
    e.result.vo2max     = vo2;
    return e;
}

class Vo2CalibrationTest : public ::testing::TestWithParam<Method>
{
protected:
    Model m = model(GetParam());
};
} // namespace

TEST_P(Vo2CalibrationTest, FitInvertsApply)
{
    EXPECT_NEAR(m.apply(50.0f, m.fit(50.0f, 54.0f)), 54.0f, 1e-4f);
    EXPECT_FLOAT_EQ(m.apply(50.0f, m.neutral), 50.0f);
}

TEST_P(Vo2CalibrationTest, InterpolatesBetweenLabTests)
{
    Vo2History h;
    h.add(run(0, 50.0f));
    h.add(run(90, 50.0f));
    h.add(run(180, 50.0f));
    LabTests labs;
    labs.add({0, 52.0f});
    labs.add({180 * kDay, 55.0f});

    const Anchors a = anchors(h, labs, m);
    ASSERT_EQ(a.count, 2u);
    EXPECT_NEAR(calibrate(h.entries[0].result, 0, a, m).vo2max, 52.0f, 1e-4f);
    EXPECT_NEAR(calibrate(h.entries[2].result, 180 * kDay, a, m).vo2max, 55.0f, 1e-4f);
    const float mid = paramAt(a, 90 * kDay, m);
    EXPECT_FLOAT_EQ(mid, (a.items[0].param + a.items[1].param) / 2.0f);
    EXPECT_FLOAT_EQ(paramAt(a, -10 * kDay, m), a.items[0].param);
    EXPECT_FLOAT_EQ(paramAt(a, 400 * kDay, m), a.items[1].param);
}

TEST_P(Vo2CalibrationTest, AnchorIsTheMedianOfNearbyRuns)
{
    Vo2History h;
    h.add(run(-30, 48.0f));
    h.add(run(-10, 50.0f));
    h.add(run(5, 52.0f));
    h.add(run(60, 40.0f));                       // outside the window
    h.add(run(1, 30.0f, Confidence::LOW));       // too uncertain
    LabTests labs;
    labs.add({0, 51.0f});

    const Anchors a = anchors(h, labs, m);
    ASSERT_EQ(a.count, 1u);
    EXPECT_FLOAT_EQ(a.items[0].param, m.fit(50.0f, 51.0f));
}

TEST_P(Vo2CalibrationTest, SkipsLabTestsWithoutRunsAndRejectsOutliers)
{
    Vo2History h;
    h.add(run(0, 40.0f));
    LabTests labs;
    labs.add({0, 60.0f});              // implies far more than any plausible correction
    labs.add({300 * kDay, 50.0f});     // no runs nearby

    const Anchors a = anchors(h, labs, m);
    EXPECT_EQ(a.count, 0u);
    EXPECT_EQ(a.rejected, 1u);
    EXPECT_FLOAT_EQ(calibrate(h.entries[0].result, 0, a, m).vo2max, 40.0f);
}

TEST_P(Vo2CalibrationTest, LeavesResultsWithoutEstimateUnchanged)
{
    Anchors a;
    a.items[0] = {0, m.max};
    a.count    = 1;
    const Vo2Result none;
    EXPECT_EQ(calibrate(none, 0, a, m).vo2max, 0.0f);
}

INSTANTIATE_TEST_SUITE_P(Methods, Vo2CalibrationTest,
                         ::testing::Values(Method::FACTOR, Method::RUNNING_COST));
