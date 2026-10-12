/**
 ******************************************************************************
 * @file    JsonFile.hpp
 * @brief   Read and write a JSON file holding {"entries": [...]}.
 ******************************************************************************
 */

#ifndef JSON_FILE_HPP
#define JSON_FILE_HPP

#include <cstddef>
#include <functional>

#include "SDK/JSON/JsonStreamReader.hpp"
#include "SDK/JSON/JsonStreamWriter.hpp"
#include "SDK/Kernel/Kernel.hpp"

namespace JsonFile
{
/// Write {"entries": [...]}, calling @p writeEntry inside each of @p count maps.
bool saveEntries(const SDK::Kernel& kernel, const char* path, std::size_t count,
                 const std::function<void(SDK::JsonStreamWriter&, std::size_t)>& writeEntry);

/// Call @p readEntry for each of at most @p maxCount entries, oldest first.
/// False if the file is missing or not valid JSON.
bool loadEntries(const SDK::Kernel& kernel, const char* path, std::size_t maxCount,
                 const std::function<void(const SDK::JsonStreamReader&)>& readEntry);
} // namespace JsonFile

#endif // JSON_FILE_HPP
