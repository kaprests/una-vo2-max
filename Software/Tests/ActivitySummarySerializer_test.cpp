#include "ActivitySummarySerializer.hpp"
#include "KernelTestDoubles.hpp"

#include <gtest/gtest.h>

namespace
{
using SDK::TestSupport::KernelFixture;
using Vo2 = Vo2Result;

constexpr const char* kFile = "summary.json";
} // namespace

TEST(ActivitySummarySerializer, Vo2RoundTrips)
{
    KernelFixture fixture;
    ActivitySummarySerializer serializer(fixture.kernel, kFile);

    ActivitySummary saved{};
    saved.vo2 = {Vo2::Confidence::MEDIUM, Vo2::Reason::OK, 52.5f, 840, 0.18f, 0.72f};
    ASSERT_TRUE(serializer.save(saved));

    ActivitySummary loaded{};
    ASSERT_TRUE(serializer.load(loaded));
    EXPECT_EQ(loaded.vo2.confidence, Vo2::Confidence::MEDIUM);
    EXPECT_EQ(loaded.vo2.reason, Vo2::Reason::OK);
    EXPECT_FLOAT_EQ(loaded.vo2.vo2max, 52.5f);
    EXPECT_EQ(loaded.vo2.validSec, 840);
    EXPECT_FLOAT_EQ(loaded.vo2.hrRangeFrac, 0.18f);
    EXPECT_FLOAT_EQ(loaded.vo2.r2, 0.72f);
}

TEST(ActivitySummarySerializer, MissingVo2LoadsAsNoEstimate)
{
    KernelFixture fixture;
    fixture.fileSystem.seedFile(kFile, R"({"utc":0,"time":60,"distance":200})");

    ActivitySummary loaded{};
    ASSERT_TRUE(ActivitySummarySerializer(fixture.kernel, kFile).load(loaded));
    EXPECT_EQ(loaded.vo2.confidence, Vo2::Confidence::NONE);
    EXPECT_FLOAT_EQ(loaded.distance, 200.0f);
}

TEST(ActivitySummarySerializer, OutOfRangeVo2LoadsAsNoEstimate)
{
    KernelFixture fixture;
    fixture.fileSystem.seedFile(kFile, R"({"vo2":{"confidence":9,"reason":0,"vo2max":50}})");

    ActivitySummary loaded{};
    ASSERT_TRUE(ActivitySummarySerializer(fixture.kernel, kFile).load(loaded));
    EXPECT_FALSE(loaded.vo2.valid());
}
