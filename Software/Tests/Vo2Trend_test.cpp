#include "Vo2Trend.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

namespace
{
using Vo2Trend::View;
using Confidence = Vo2Result::Confidence;

class Vo2TrendTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        setenv("TZ", "UTC", 1);
        tzset();
    }

    static std::time_t utc(int y, int m, int d)
    {
        std::tm t{};
        t.tm_year = y - 1900;
        t.tm_mon  = m - 1;
        t.tm_mday = d;
        t.tm_hour = 12;
        return timegm(&t);
    }

    static std::tm day(int y, int m, int d)
    {
        const std::time_t t = utc(y, m, d);
        std::tm out{};
        gmtime_r(&t, &out);
        return out;
    }

    void add(int y, int m, int d, float vo2, Confidence c = Confidence::MEDIUM)
    {
        Vo2History::Entry e;
        e.utc               = utc(y, m, d);
        e.result.confidence = c;
        e.result.vo2max     = vo2;
        history.add(e);
    }

    Vo2History history;
};
} // namespace

TEST_F(Vo2TrendTest, RunsTakesTheLast20EvenlySpaced)
{
    for (int i = 0; i < 25; ++i) {
        add(2026, 1 + i / 5, 1 + i % 5, 40.0f + i);
    }
    const auto s = Vo2Trend::select(history, View::RUNS, day(2026, 10, 1));
    ASSERT_EQ(s.count, Vo2Trend::kMaxRuns);
    EXPECT_FLOAT_EQ(s.points[0].entry->result.vo2max, 45.0f);
    EXPECT_FLOAT_EQ(s.points[0].x, 0.0f);
    EXPECT_FLOAT_EQ(s.points[19].x, 1.0f);
}

TEST_F(Vo2TrendTest, SixMonthsKeepsBestConfidencePerThird)
{
    add(2026, 10, 2, 50.0f, Confidence::LOW);
    add(2026, 10, 5, 51.0f, Confidence::HIGH);
    add(2026, 10, 9, 52.0f, Confidence::MEDIUM);   // same third, lower confidence
    add(2026, 10, 15, 53.0f);                      // second third
    const auto s = Vo2Trend::select(history, View::MONTHS_6, day(2026, 10, 20));
    ASSERT_EQ(s.count, 2u);
    EXPECT_FLOAT_EQ(s.points[0].entry->result.vo2max, 51.0f);
    EXPECT_FLOAT_EQ(s.points[1].entry->result.vo2max, 53.0f);
}

TEST_F(Vo2TrendTest, TieGoesToTheMostRecent)
{
    add(2026, 10, 2, 50.0f);
    add(2026, 10, 8, 51.0f);
    const auto s = Vo2Trend::select(history, View::YEAR, day(2026, 10, 20));
    ASSERT_EQ(s.count, 1u);
    EXPECT_FLOAT_EQ(s.points[0].entry->result.vo2max, 51.0f);
}

TEST_F(Vo2TrendTest, WindowExcludesOlderMonthsAndPlacesByDate)
{
    add(2026, 4, 30, 45.0f);   // 7 months back: outside 6 months
    add(2026, 5, 1, 46.0f);    // first day of the window
    add(2026, 10, 31, 50.0f);  // last day of the window
    const auto s = Vo2Trend::select(history, View::MONTHS_6, day(2026, 10, 31));
    ASSERT_EQ(s.count, 2u);
    EXPECT_FLOAT_EQ(s.points[0].entry->result.vo2max, 46.0f);
    EXPECT_NEAR(s.points[0].x, 0.0f, 0.01f);
    EXPECT_NEAR(s.points[1].x, 1.0f, 0.01f);
    EXPECT_EQ(s.firstMonth, 4);   // May
    EXPECT_EQ(s.lastMonth, 9);    // October
}

TEST_F(Vo2TrendTest, YearHasAtMost24Points)
{
    for (int m = 1; m <= 12; ++m) {
        for (int d = 1; d <= 28; d += 3) {
            add(2026, m, d, 40.0f + m);
        }
    }
    const auto s = Vo2Trend::select(history, View::YEAR, day(2026, 12, 31));
    EXPECT_EQ(s.count, 24u);
    for (std::size_t i = 1; i < s.count; ++i) {
        EXPECT_LT(s.points[i - 1].x, s.points[i].x);
    }
}

TEST_F(Vo2TrendTest, UnknownTodayUsesTheLatestEntry)
{
    add(2026, 3, 10, 48.0f);
    const auto s = Vo2Trend::select(history, View::MONTHS_6, std::tm{});
    ASSERT_EQ(s.count, 1u);
    EXPECT_EQ(s.lastMonth, 2);   // March
}

TEST_F(Vo2TrendTest, XOfPlacesByDateInTimeViews)
{
    add(2026, 10, 1, 50.0f);
    const auto s = Vo2Trend::select(history, View::MONTHS_6, day(2026, 10, 31));
    EXPECT_NEAR(*Vo2Trend::xOf(s, utc(2026, 5, 1)), 0.0f, 0.01f);
    EXPECT_NEAR(*Vo2Trend::xOf(s, utc(2026, 10, 31)), 1.0f, 0.01f);
    EXPECT_FALSE(Vo2Trend::xOf(s, utc(2026, 4, 30)));
}

TEST_F(Vo2TrendTest, XOfUsesNearestRunInRunsView)
{
    add(2026, 9, 1, 50.0f);
    add(2026, 9, 11, 51.0f);
    add(2026, 9, 21, 52.0f);
    const auto s = Vo2Trend::select(history, View::RUNS, day(2026, 10, 1));
    EXPECT_FLOAT_EQ(*Vo2Trend::xOf(s, utc(2026, 9, 12)), 0.5f);
    EXPECT_FLOAT_EQ(*Vo2Trend::xOf(s, utc(2026, 10, 4)), 1.0f);    // within the margin
    EXPECT_FALSE(Vo2Trend::xOf(s, utc(2026, 10, 10)));             // beyond it
}

TEST_F(Vo2TrendTest, LabMarksKeepTheNewestWithinTheView)
{
    add(2026, 10, 1, 50.0f);
    LabTests labs;
    labs.add({utc(2025, 1, 15), 45.0f});   // outside the 12-month view
    for (int i = 0; i < 15; ++i) {
        labs.add({utc(2026, 1 + i % 10, 1 + i), 50.0f + i});
    }
    const auto s = Vo2Trend::select(history, View::YEAR, day(2026, 10, 31));
    const auto m = Vo2Trend::labMarks(s, labs);
    ASSERT_EQ(m.count, Vo2Trend::kMaxLabMarks);
    EXPECT_EQ(m.marks[0].entry, &labs.entries[labs.count - 1]);
    for (std::size_t i = 1; i < m.count; ++i) {
        EXPECT_GT(m.marks[i - 1].entry->utc, m.marks[i].entry->utc);
    }
}
