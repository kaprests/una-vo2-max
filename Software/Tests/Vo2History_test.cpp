#include "Vo2HistorySerializer.hpp"
#include "KernelTestDoubles.hpp"

#include <gtest/gtest.h>

#include "SDK/Tools/FirmwareVersion.hpp"

namespace
{
using SDK::TestSupport::KernelFixture;
using Confidence = Vo2Result::Confidence;

constexpr const char* kFile = "Activity/vo2_history.json";

Vo2History::Entry entry(std::time_t utc, float vo2max, Confidence c = Confidence::MEDIUM)
{
    Vo2History::Entry e;
    e.utc               = utc;
    e.result.confidence = c;
    e.result.reason     = Vo2Result::Reason::OK;
    e.result.vo2max     = vo2max;
    return e;
}
} // namespace

TEST(Vo2History, AddKeepsOrderAndDropsOldestWhenFull)
{
    Vo2History h;
    for (std::size_t i = 0; i < Vo2History::kCapacity + 3; ++i) {
        h.add(entry(static_cast<std::time_t>(i), 40.0f + i));
    }
    EXPECT_EQ(h.count, Vo2History::kCapacity);
    EXPECT_EQ(h.entries[0].utc, 3);
    EXPECT_EQ(h.latest().utc, static_cast<std::time_t>(Vo2History::kCapacity + 2));
}

TEST(Vo2HistorySerializer, RoundTrips)
{
    KernelFixture fixture;
    Vo2HistorySerializer serializer(fixture.kernel, kFile);

    Vo2History saved;
    saved.add(entry(1000, 48.5f, Confidence::LOW));
    saved.add(entry(2000, 51.2f, Confidence::HIGH));
    saved.entries[1].version = vo2Version(0, 1, 2);
    ASSERT_TRUE(serializer.save(saved));

    Vo2History loaded;
    ASSERT_TRUE(serializer.load(loaded));
    ASSERT_EQ(loaded.count, 2u);
    EXPECT_EQ(loaded.entries[0].utc, 1000);
    EXPECT_FLOAT_EQ(loaded.entries[0].result.vo2max, 48.5f);
    EXPECT_EQ(loaded.entries[0].result.confidence, Confidence::LOW);
    EXPECT_EQ(loaded.latest().utc, 2000);
    EXPECT_EQ(loaded.latest().result.confidence, Confidence::HIGH);
    EXPECT_EQ(loaded.latest().version, vo2Version(0, 1, 2));
}

TEST(Vo2History, UntaggedAndOlderEntriesAreOutdated)
{
    EXPECT_EQ(vo2Version(1, 2, 3), SDK::ParseVersion("1.2.3").u32);
    Vo2History::Entry e = entry(1, 50.0f);
    EXPECT_EQ(e.appVersion(), kVo2UntaggedVersion);
    EXPECT_TRUE(e.outdated());
    e.version = kVo2ComparableSince;
    EXPECT_FALSE(e.outdated());
}

TEST(Vo2HistorySerializer, SkipsInvalidEntries)
{
    KernelFixture fixture;
    fixture.fileSystem.seedFile(kFile, R"({"entries":[
        {"utc":1,"confidence":2,"reason":0,"vo2max":50},
        {"utc":2,"confidence":9,"reason":0,"vo2max":51},
        {"utc":3,"confidence":0,"reason":2,"vo2max":0},
        {"utc":4,"confidence":3,"reason":0,"vo2max":52}]})");

    Vo2History loaded;
    ASSERT_TRUE(Vo2HistorySerializer(fixture.kernel, kFile).load(loaded));
    ASSERT_EQ(loaded.count, 2u);
    EXPECT_EQ(loaded.entries[0].utc, 1);
    EXPECT_EQ(loaded.entries[0].version, 0u);   // files from before the version tag
    EXPECT_EQ(loaded.entries[1].utc, 4);
}

TEST(Vo2HistorySerializer, LoadsAFullHistory)
{
    KernelFixture fixture;
    Vo2HistorySerializer serializer(fixture.kernel, kFile);

    Vo2History saved;
    for (std::size_t i = 0; i < Vo2History::kCapacity; ++i) {
        saved.add(entry(static_cast<std::time_t>(1000 + i), 45.0f + i * 0.05f));
    }
    ASSERT_TRUE(serializer.save(saved));

    Vo2History loaded;
    ASSERT_TRUE(serializer.load(loaded));
    ASSERT_EQ(loaded.count, Vo2History::kCapacity);
    EXPECT_EQ(loaded.entries[0].utc, 1000);
    EXPECT_EQ(loaded.latest().utc, static_cast<std::time_t>(1000 + Vo2History::kCapacity - 1));
}
