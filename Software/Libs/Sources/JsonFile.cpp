/**
 ******************************************************************************
 * @file    JsonFile.cpp
 * @brief   Read and write a JSON file holding {"entries": [...]} (see JsonFile.hpp).
 ******************************************************************************
 */

#include "JsonFile.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

bool JsonFile::saveEntries(const SDK::Kernel& kernel, const char* path, std::size_t count,
                           const std::function<void(SDK::JsonStreamWriter&, std::size_t)>& writeEntry)
{
    if (const char* slash = std::strrchr(path, '/')) {
        if (!kernel.fs.mkdir(std::string(path, slash).c_str())) {
            return false;
        }
    }
    auto file = kernel.fs.file(path);
    if (!file || !file->open(true, true)) {
        return false;
    }

    SDK::JsonStreamWriter writer(file.get());
    writer.startMap();
    writer.startArray("entries");
    for (std::size_t i = 0; i < count; ++i) {
        writer.startMap();
        writeEntry(writer, i);
        writer.endMap();
    }
    writer.endArray();
    writer.endMap();

    file->flush();
    file->close();
    return true;
}

bool JsonFile::loadEntries(const SDK::Kernel& kernel, const char* path, std::size_t maxCount,
                           const std::function<void(const SDK::JsonStreamReader&)>& readEntry)
{
    auto file = kernel.fs.file(path);
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
    // whole file searches from the start, which is quadratic in the entry count.
    for (unsigned i = 0; i < maxCount; ++i) {
        char query[24];   // "entries[" + up to 10 digits + "]"
        std::snprintf(query, sizeof(query), "entries[%u]", i);
        const char* object = nullptr;
        std::size_t len    = 0;
        if (!reader.get(query, object, len)) {
            break;
        }
        readEntry(SDK::JsonStreamReader(object, len));
    }
    return true;
}
