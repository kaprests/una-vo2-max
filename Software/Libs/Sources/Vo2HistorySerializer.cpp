/**
 ******************************************************************************
 * @file    Vo2HistorySerializer.cpp
 * @brief   Saves and loads Vo2History as JSON (see Vo2HistorySerializer.hpp).
 *
 * {"entries":[{"utc":..., <Vo2Json fields>}, ...]}, oldest first.
 ******************************************************************************
 */

#include "Vo2HistorySerializer.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "SDK/JSON/JsonStreamReader.hpp"
#include "SDK/JSON/JsonStreamWriter.hpp"

#include "Vo2Json.hpp"

Vo2HistorySerializer::Vo2HistorySerializer(const SDK::Kernel& kernel, const char* path)
    : mKernel(kernel)
    , mPath(path)
{
}

bool Vo2HistorySerializer::save(const Vo2History& history)
{
    if (const char* slash = std::strrchr(mPath, '/')) {
        if (!mKernel.fs.mkdir(std::string(mPath, slash).c_str())) {
            return false;
        }
    }
    auto file = mKernel.fs.file(mPath);
    if (!file || !file->open(true, true)) {
        return false;
    }

    SDK::JsonStreamWriter writer(file.get());
    writer.startMap();
    writer.startArray("entries");
    for (std::size_t i = 0; i < history.count; ++i) {
        const Vo2History::Entry& e = history.entries[i];
        writer.startMap();
        writer.add("utc", static_cast<uint32_t>(e.utc));
        Vo2Json::write(writer, e.result);
        writer.endMap();
    }
    writer.endArray();
    writer.endMap();

    file->flush();
    file->close();
    return true;
}

bool Vo2HistorySerializer::load(Vo2History& history)
{
    auto file = mKernel.fs.file(mPath);
    if (!file || !file->exist() || !file->open(false, false)) {
        return false;
    }
    const std::size_t size = file->size();
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[size]);
    std::size_t read = 0;
    const bool ok = buffer && size && file->read(buffer.get(), size, read) && read == size;
    file->close();
    if (!ok) {
        return false;
    }

    SDK::JsonStreamReader reader(buffer.get(), size);
    if (!reader.validate()) {
        return false;
    }

    // Each entry is read through a reader over its own object: a query on the
    // whole file searches from the start, which is quadratic over 160 entries.
    history = {};
    for (unsigned i = 0; i < Vo2History::kCapacity; ++i) {
        char path[16];
        std::snprintf(path, sizeof(path), "entries[%u]", i);
        const char* object = nullptr;
        std::size_t len    = 0;
        if (!reader.get(path, object, len)) {
            break;
        }
        const SDK::JsonStreamReader entry(object, len);
        uint32_t          utc = 0;
        Vo2History::Entry e;
        if (entry.get("utc", utc) && Vo2Json::read(entry, e.result) && e.result.valid()) {
            e.utc = static_cast<std::time_t>(utc);
            history.add(e);
        }
    }
    return true;
}
