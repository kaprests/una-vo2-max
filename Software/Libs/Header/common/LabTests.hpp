/**
 ******************************************************************************
 * @file    LabTests.hpp
 * @brief   VO2 max results from lab tests, oldest first.
 ******************************************************************************
 */

#ifndef LAB_TESTS_HPP
#define LAB_TESTS_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <optional>

struct LabTests {
    static constexpr std::size_t kCapacity = 16;
    static constexpr float       kMinVo2   = 20.0f;   ///< Lower values count as not set.

    struct Entry {
        std::time_t utc    = 0;   ///< Noon UTC on the test date.
        float       vo2max = 0.0f;
    };

    std::array<Entry, kCapacity> entries{};
    std::size_t                  count = 0;

    bool empty() const { return count == 0; }

    /// Insert by date, replacing an entry with the same date and dropping the
    /// oldest when full. False if nothing changed.
    bool add(const Entry& e)
    {
        Entry* const end = entries.begin() + count;
        Entry* const pos = std::lower_bound(entries.begin(), end, e.utc,
                                            [](const Entry& a, std::time_t t) { return a.utc < t; });
        if (pos != end && pos->utc == e.utc) {
            if (pos->vo2max == e.vo2max) {
                return false;
            }
            pos->vo2max = e.vo2max;
            return true;
        }
        if (count == kCapacity) {
            if (pos == entries.begin()) {
                return false;   // older than everything kept
            }
            std::move(entries.begin() + 1, pos, entries.begin());
            *(pos - 1) = e;
            return true;
        }
        std::move_backward(pos, end, end + 1);
        *pos = e;
        ++count;
        return true;
    }

    /// "YYYY-MM-DD" as noon UTC, so the local date is the same in every time zone
    /// within +-12 h. Empty for a malformed or impossible date.
    static std::optional<std::time_t> parseDate(const char* s)
    {
        int y = 0, m = 0, d = 0;
        char tail = 0;
        if (std::sscanf(s, "%4d-%2d-%2d%c", &y, &m, &d, &tail) != 3
                || y < 1970 || m < 1 || m > 12 || d < 1 || d > daysInMonth(y, m)) {
            return std::nullopt;
        }
        return static_cast<std::time_t>(daysFromCivil(y, m, d)) * 86400 + 12 * 3600;
    }

private:
    static constexpr bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

    static constexpr int daysInMonth(int y, int m)
    {
        constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return kDays[m - 1] + (m == 2 && leap(y));
    }

    /// Days since 1970-01-01 (H. Hinnant's days_from_civil); timegm is not portable.
    static constexpr long daysFromCivil(int y, int m, int d)
    {
        y -= m <= 2;
        const int era = y / 400;
        const int yoe = y - era * 400;
        const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097L + doe - 719468L;
    }
};

#endif // LAB_TESTS_HPP
