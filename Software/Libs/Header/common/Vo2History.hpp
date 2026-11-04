/**
 ******************************************************************************
 * @file    Vo2History.hpp
 * @brief   The last valid VO2 max results, oldest first.
 ******************************************************************************
 */

#ifndef VO2_HISTORY_HPP
#define VO2_HISTORY_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>

#include "Vo2Result.hpp"

/// App version packed as (major << 16) | (minor << 8) | patch, as SDK::ParseVersion.
constexpr uint32_t vo2Version(uint8_t major, uint8_t minor, uint8_t patch)
{
    return static_cast<uint32_t>(major) << 16 | static_cast<uint32_t>(minor) << 8 | patch;
}

/// Untagged results come from 0.1.1 or earlier.
inline constexpr uint32_t kVo2UntaggedVersion = vo2Version(0, 1, 1);

/// First version whose results are comparable with the current ones. Bump it
/// only in a release that changes how results are computed.
inline constexpr uint32_t kVo2ComparableSince = vo2Version(0, 1, 2);

struct Vo2History {
    static constexpr std::size_t kCapacity = 160;   ///< About a year at 3 runs a week.

    struct Entry {
        std::time_t utc     = 0;
        Vo2Result   result;
        uint32_t    version = 0; ///< App version that recorded it; 0 if untagged.

        uint32_t appVersion() const { return version ? version : kVo2UntaggedVersion; }
        bool     outdated() const   { return appVersion() < kVo2ComparableSince; }
    };

    std::array<Entry, kCapacity> entries{};
    std::size_t                  count = 0;

    /// Append, dropping the oldest entry when full.
    void add(const Entry& e)
    {
        if (count == kCapacity) {
            std::move(entries.begin() + 1, entries.end(), entries.begin());
            --count;
        }
        entries[count++] = e;
    }

    bool         empty() const  { return count == 0; }
    const Entry& latest() const { return entries[count - 1]; }
};

#endif // VO2_HISTORY_HPP
