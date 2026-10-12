#include "LabTestsSerializer.hpp"
#include "KernelTestDoubles.hpp"

#include <gtest/gtest.h>

namespace
{
using SDK::TestSupport::KernelFixture;

constexpr std::time_t kDay = 86400;

LabTests::Entry entry(std::time_t day, float vo2max)
{
    return {day * kDay, vo2max};
}
} // namespace

TEST(LabTests, ParseDate)
{
    EXPECT_EQ(LabTests::parseDate("1970-01-01"), 12 * 3600);
    EXPECT_EQ(LabTests::parseDate("2026-10-08"), 20734 * kDay + 12 * 3600);
    EXPECT_EQ(LabTests::parseDate("2024-02-29"), 19782 * kDay + 12 * 3600);
    EXPECT_FALSE(LabTests::parseDate(""));
    EXPECT_FALSE(LabTests::parseDate("2026-13-01"));
    EXPECT_FALSE(LabTests::parseDate("2025-02-29"));
    EXPECT_FALSE(LabTests::parseDate("2026-10-08x"));
    EXPECT_FALSE(LabTests::parseDate("1969-12-31"));
}

TEST(LabTests, AddKeepsDateOrderAndReplacesSameDate)
{
    LabTests t;
    EXPECT_TRUE(t.add(entry(20, 52.0f)));
    EXPECT_TRUE(t.add(entry(10, 50.0f)));
    EXPECT_FALSE(t.add(entry(20, 52.0f)));
    EXPECT_TRUE(t.add(entry(20, 53.0f)));
    ASSERT_EQ(t.count, 2u);
    EXPECT_EQ(t.entries[0].utc, 10 * kDay);
    EXPECT_FLOAT_EQ(t.entries[1].vo2max, 53.0f);
}

TEST(LabTests, FullDropsTheOldest)
{
    LabTests t;
    for (std::size_t i = 0; i < LabTests::kCapacity; ++i) {
        t.add(entry(static_cast<std::time_t>(10 + i), 50.0f));
    }
    EXPECT_FALSE(t.add(entry(5, 49.0f)));                 // older than everything kept
    EXPECT_TRUE(t.add({15 * kDay + kDay / 2, 51.0f}));    // between days 15 and 16
    ASSERT_EQ(t.count, LabTests::kCapacity);
    EXPECT_EQ(t.entries[0].utc, 11 * kDay);
    EXPECT_FLOAT_EQ(t.entries[5].vo2max, 51.0f);
    EXPECT_TRUE(std::is_sorted(t.entries.begin(), t.entries.end(),
                               [](const auto& a, const auto& b) { return a.utc < b.utc; }));
}

TEST(LabTests, ResetIgnoresStoredTestsOnly)
{
    LabTests t;
    EXPECT_FALSE(t.resetCalibration());
    t.add(entry(100, 50.0f));
    t.add(entry(200, 52.0f));
    EXPECT_TRUE(t.resetCalibration());
    EXPECT_EQ(t.activeCount(), 0u);
    EXPECT_FALSE(t.resetCalibration());

    t.add(entry(50, 48.0f));                     // added later, dated earlier
    EXPECT_EQ(t.activeCount(), 1u);
    EXPECT_FALSE(t.entries[0].ignored);
}

TEST(LabTests, ResetSurvivesSameValueButNotANewOne)
{
    LabTests t;
    t.add(entry(100, 50.0f));
    t.resetCalibration();
    EXPECT_FALSE(t.add(entry(100, 50.0f)));      // config re-read on start
    EXPECT_TRUE(t.entries[0].ignored);
    EXPECT_TRUE(t.add(entry(100, 51.0f)));
    EXPECT_FALSE(t.entries[0].ignored);
}

TEST(LabTestsSerializer, RoundTripsAndSkipsInvalid)
{
    KernelFixture fixture;
    constexpr const char* kFile = "Activity/lab_tests.json";
    LabTests saved;
    saved.add(entry(100, 48.5f));
    saved.add(entry(200, 55.0f));
    ASSERT_TRUE(LabTestsSerializer(fixture.kernel, kFile).save(saved));

    LabTests loaded;
    ASSERT_TRUE(LabTestsSerializer(fixture.kernel, kFile).load(loaded));
    ASSERT_EQ(loaded.count, 2u);
    EXPECT_EQ(loaded.entries[1].utc, 200 * kDay);
    EXPECT_FLOAT_EQ(loaded.entries[1].vo2max, 55.0f);

    fixture.fileSystem.seedFile(kFile, R"({"entries":[{"utc":1,"vo2max":10},{"utc":2},{"utc":3,"vo2max":50}]})");
    ASSERT_TRUE(LabTestsSerializer(fixture.kernel, kFile).load(loaded));
    ASSERT_EQ(loaded.count, 1u);
    EXPECT_EQ(loaded.entries[0].utc, 3);
    EXPECT_FALSE(loaded.entries[0].ignored);   // files from before the flag
}

TEST(LabTestsSerializer, KeepsIgnoredFlag)
{
    KernelFixture fixture;
    constexpr const char* kFile = "Activity/lab_tests.json";
    LabTests saved;
    saved.add(entry(100, 48.5f));
    saved.resetCalibration();
    saved.add(entry(200, 55.0f));
    ASSERT_TRUE(LabTestsSerializer(fixture.kernel, kFile).save(saved));

    LabTests loaded;
    ASSERT_TRUE(LabTestsSerializer(fixture.kernel, kFile).load(loaded));
    ASSERT_EQ(loaded.count, 2u);
    EXPECT_TRUE(loaded.entries[0].ignored);
    EXPECT_FALSE(loaded.entries[1].ignored);
}
