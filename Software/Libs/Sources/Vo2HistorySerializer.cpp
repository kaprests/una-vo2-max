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

    history = {};
    for (unsigned i = 0; i < Vo2History::kCapacity; ++i) {
        char path[16];
        char key[24];
        std::snprintf(path, sizeof(path), "entries[%u]", i);
        std::snprintf(key, sizeof(key), "%s.utc", path);
        uint32_t utc = 0;
        if (!reader.get(key, utc)) {
            break;
        }
        Vo2History::Entry e{static_cast<std::time_t>(utc), {}};
        if (Vo2Json::read(reader, path, e.result) && e.result.valid()) {
            history.add(e);
        }
    }
    return true;
}
