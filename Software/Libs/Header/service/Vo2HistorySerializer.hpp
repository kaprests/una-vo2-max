/**
 ******************************************************************************
 * @file    Vo2HistorySerializer.hpp
 * @brief   Saves and loads Vo2History as JSON.
 ******************************************************************************
 */

#ifndef VO2_HISTORY_SERIALIZER_HPP
#define VO2_HISTORY_SERIALIZER_HPP

#include "SDK/Kernel/Kernel.hpp"

#include "Vo2History.hpp"

class Vo2HistorySerializer
{
public:
    Vo2HistorySerializer(const SDK::Kernel& kernel, const char* path);

    bool save(const Vo2History& history);
    /// Entries that are missing or invalid are skipped.
    bool load(Vo2History& history);

private:
    const SDK::Kernel& mKernel;
    const char*        mPath;
};

#endif // VO2_HISTORY_SERIALIZER_HPP
