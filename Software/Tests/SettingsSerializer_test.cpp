#include "SettingsSerializer.hpp"
#include "KernelTestDoubles.hpp"

#include <gtest/gtest.h>

namespace
{
using SDK::TestSupport::KernelFixture;

constexpr const char* kFile = "settings.json";
} // namespace

TEST(SettingsSerializer, GuideFlagsRoundTrip)
{
    KernelFixture fixture;
    SettingsSerializer serializer(fixture.kernel, kFile);

    Settings saved{};
    saved.showGuide = true;
    saved.guideSeen = true;
    ASSERT_TRUE(serializer.save(saved));

    Settings loaded{};
    ASSERT_TRUE(serializer.load(loaded));
    EXPECT_TRUE(loaded.showGuide);
    EXPECT_TRUE(loaded.guideSeen);
}

TEST(SettingsSerializer, MissingGuideFlagsDefaultOff)
{
    KernelFixture fixture;
    fixture.fileSystem.seedFile(kFile, R"({"version":1})");

    Settings loaded{};
    ASSERT_TRUE(SettingsSerializer(fixture.kernel, kFile).load(loaded));
    EXPECT_FALSE(loaded.showGuide);
    EXPECT_FALSE(loaded.guideSeen);
}
