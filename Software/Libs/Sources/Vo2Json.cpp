/**
 ******************************************************************************
 * @file    Vo2Json.cpp
 * @brief   Vo2Result as JSON fields (see Vo2Json.hpp).
 ******************************************************************************
 */

#include "Vo2Json.hpp"

#include <cstdio>

void Vo2Json::write(SDK::JsonStreamWriter& writer, const Vo2Result& v)
{
    writer.add("confidence", static_cast<uint8_t>(v.confidence));
    writer.add("reason",     static_cast<uint8_t>(v.reason));
    writer.add("vo2max",     v.vo2max);
    writer.add("valid_sec",  v.validSec);
    writer.add("hr_range",   v.hrRangeFrac);
    writer.add("r2",         v.r2);
}

bool Vo2Json::read(const SDK::JsonStreamReader& reader, const char* path, Vo2Result& v)
{
    char key[48];
    const auto get = [&](const char* field, auto& out) {
        if (!path) {
            return reader.get(field, out);
        }
        std::snprintf(key, sizeof(key), "%s.%s", path, field);
        return reader.get(key, out);
    };

    uint8_t confidence = 0;
    uint8_t reason     = 0;
    if (!get("confidence", confidence) || !get("reason", reason)
            || confidence > static_cast<uint8_t>(Vo2Result::Confidence::HIGH)
            || reason > static_cast<uint8_t>(Vo2Result::Reason::OUT_OF_RANGE)) {
        return false;
    }

    Vo2Result r;
    r.confidence = static_cast<Vo2Result::Confidence>(confidence);
    r.reason     = static_cast<Vo2Result::Reason>(reason);
    get("vo2max",     r.vo2max);
    get("valid_sec", r.validSec) || get("steady_sec", r.validSec);   // before 0.1.4
    get("hr_range",   r.hrRangeFrac);
    get("r2",         r.r2);
    v = r;
    return true;
}
