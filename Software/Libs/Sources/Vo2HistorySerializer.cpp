/**
 ******************************************************************************
 * @file    Vo2HistorySerializer.cpp
 * @brief   Saves and loads Vo2History as JSON (see Vo2HistorySerializer.hpp).
 *
 * {"entries":[{"utc":..., "ver":..., <Vo2Json fields>}, ...]}, oldest first;
 * "ver" is absent in files from before 0.1.2.
 ******************************************************************************
 */

#include "Vo2HistorySerializer.hpp"

#include "JsonFile.hpp"
#include "Vo2Json.hpp"

Vo2HistorySerializer::Vo2HistorySerializer(const SDK::Kernel& kernel, const char* path)
    : mKernel(kernel)
    , mPath(path)
{
}

bool Vo2HistorySerializer::save(const Vo2History& history)
{
    return JsonFile::saveEntries(mKernel, mPath, history.count, [&](SDK::JsonStreamWriter& w, std::size_t i) {
        const Vo2History::Entry& e = history.entries[i];
        w.add("utc", static_cast<uint32_t>(e.utc));
        w.add("ver", e.version);
        Vo2Json::write(w, e.result);
    });
}

bool Vo2HistorySerializer::load(Vo2History& history)
{
    history = {};
    return JsonFile::loadEntries(mKernel, mPath, Vo2History::kCapacity, [&](const SDK::JsonStreamReader& r) {
        uint32_t          utc = 0;
        Vo2History::Entry e;
        if (r.get("utc", utc) && Vo2Json::read(r, e.result) && e.result.valid()) {
            e.utc = static_cast<std::time_t>(utc);
            r.get("ver", e.version);   // optional
            history.add(e);
        }
    });
}
