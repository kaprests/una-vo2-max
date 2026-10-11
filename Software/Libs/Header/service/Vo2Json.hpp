/**
 ******************************************************************************
 * @file    Vo2Json.hpp
 * @brief   Vo2Result as JSON fields, shared by the summary and history files.
 ******************************************************************************
 */

#ifndef VO2_JSON_HPP
#define VO2_JSON_HPP

#include "SDK/JSON/JsonStreamReader.hpp"
#include "SDK/JSON/JsonStreamWriter.hpp"

#include "Vo2Result.hpp"

namespace Vo2Json
{
/// Add the fields to the map the writer is in.
void write(SDK::JsonStreamWriter& writer, const Vo2Result& v);

/// Read the fields of the map at @p path (e.g. "vo2"), or of the top-level
/// map when @p path is null. False, leaving @p v unchanged, if they are
/// missing or out of range.
bool read(const SDK::JsonStreamReader& reader, const char* path, Vo2Result& v);
inline bool read(const SDK::JsonStreamReader& reader, Vo2Result& v) { return read(reader, nullptr, v); }
} // namespace Vo2Json

#endif // VO2_JSON_HPP
