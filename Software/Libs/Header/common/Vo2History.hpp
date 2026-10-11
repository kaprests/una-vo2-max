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
#include <ctime>

#include "Vo2Result.hpp"

struct Vo2History {
    static constexpr std::size_t kCapacity = 20;

    struct Entry {
        std::time_t utc = 0;
        Vo2Result   result;
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
