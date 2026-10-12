/**
 ******************************************************************************
 * @file    Vo2Trend.hpp
 * @brief   The VO2 history points shown for a trend view.
 *
 * RUNS: the last kMaxRuns results, evenly spaced. MONTHS_6 / YEAR: the
 * current calendar month and the 5 / 11 before it, split into thirds (days
 * 1-10, 11-20, 21-end) / halves (1-15, 16-end); each part keeps its
 * highest-confidence result (on a tie, the most recent) and is placed by
 * date. Entries are expected oldest first. All dates are local time.
 ******************************************************************************
 */

#ifndef VO2_TREND_HPP
#define VO2_TREND_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <optional>

#include "Vo2History.hpp"

namespace Vo2Trend
{
enum class View : uint8_t { RUNS, MONTHS_6, YEAR, COUNT };

inline constexpr std::size_t kMaxRuns   = 20;
inline constexpr std::size_t kMaxPoints = 24;

struct Point {
    const Vo2History::Entry* entry = nullptr;
    float                    x     = 0.0f;   ///< 0 (left) .. 1 (right)
};

struct Selection {
    std::array<Point, kMaxPoints> points{};
    std::size_t count      = 0;
    View        view       = View::RUNS;
    int         firstMonth = 0;   ///< 0..11; time views only
    int         lastMonth  = 0;
    int         windowStart = 0;  ///< Month index (tm_year * 12 + tm_mon) of the first month.
    int         months      = 0;
};

namespace Detail
{
struct ViewDef {
    int months;
    int partsPerMonth;
};

inline constexpr ViewDef kDefs[] = {{0, 0}, {6, 3}, {12, 2}};
static_assert(6 * 3 <= kMaxPoints && 12 * 2 <= kMaxPoints);

inline std::tm local(std::time_t utc)
{
    std::tm t{};
    localtime_r(&utc, &t);
    return t;
}

inline int monthIndex(const std::tm& t)
{
    return t.tm_year * 12 + t.tm_mon;
}

inline int daysInMonth(const std::tm& t)
{
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int y    = t.tm_year + 1900;
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return kDays[t.tm_mon] + (t.tm_mon == 1 && leap);
}

/// Position of local date @p t in a window of @p months starting at month index @p start.
inline std::optional<float> position(const std::tm& t, int start, int months)
{
    const int month = monthIndex(t) - start;
    if (month < 0 || month >= months) {
        return std::nullopt;
    }
    return (month + (t.tm_mday - 0.5f) / daysInMonth(t)) / months;
}
} // namespace Detail

/// @p today: the current local date; tm_year 0 (not yet known) uses the latest entry's.
inline Selection select(const Vo2History& h, View view, const std::tm& today)
{
    Selection s;
    s.view = view;
    if (h.empty()) {
        return s;
    }

    if (view == View::RUNS) {
        const std::size_t n     = h.count < kMaxRuns ? h.count : kMaxRuns;
        const std::size_t first = h.count - n;
        for (std::size_t i = 0; i < n; ++i) {
            s.points[i] = {&h.entries[first + i], n > 1 ? static_cast<float>(i) / (n - 1) : 0.5f};
        }
        s.count = n;
        return s;
    }

    const Detail::ViewDef def = Detail::kDefs[static_cast<int>(view)];
    const std::tm anchor      = today.tm_year ? today : Detail::local(h.latest().utc);
    const int     last        = Detail::monthIndex(anchor);
    const int     first       = last - def.months + 1;
    s.firstMonth  = ((first % 12) + 12) % 12;
    s.lastMonth   = anchor.tm_mon;
    s.windowStart = first;
    s.months      = def.months;

    int lastPart = -1;
    for (std::size_t i = 0; i < h.count; ++i) {
        const Vo2History::Entry& e = h.entries[i];
        const std::tm t = Detail::local(e.utc);
        const auto    x = Detail::position(t, first, def.months);
        if (!x) {
            continue;
        }
        const int sub  = std::min((t.tm_mday - 1) / (30 / def.partsPerMonth), def.partsPerMonth - 1);
        const int part = (Detail::monthIndex(t) - first) * def.partsPerMonth + sub;
        const Point p{&e, *x};

        if (part != lastPart) {
            s.points[s.count++] = p;
            lastPart = part;
        } else if (e.result.confidence >= s.points[s.count - 1].entry->result.confidence) {
            s.points[s.count - 1] = p;
        }
    }
    return s;
}

inline constexpr std::time_t kRunsMarginSec = 14 * 86400;

/// Where a dated value outside the history (e.g. a lab test) belongs on @p s:
/// by date in the time views; at the run closest in time in RUNS, if within
/// kRunsMarginSec of the runs shown. Empty if it is outside the view.
inline std::optional<float> xOf(const Selection& s, std::time_t utc)
{
    if (s.view != View::RUNS) {
        return Detail::position(Detail::local(utc), s.windowStart, s.months);
    }
    if (s.count == 0 || utc < s.points[0].entry->utc - kRunsMarginSec
            || utc > s.points[s.count - 1].entry->utc + kRunsMarginSec) {
        return std::nullopt;
    }
    const auto distance = [utc](const Point& p) { return std::llabs(static_cast<long long>(p.entry->utc - utc)); };
    return std::min_element(s.points.begin(), s.points.begin() + s.count,
                            [&](const Point& a, const Point& b) { return distance(a) < distance(b); })->x;
}
} // namespace Vo2Trend

#endif // VO2_TREND_HPP
