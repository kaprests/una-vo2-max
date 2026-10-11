/**
 ******************************************************************************
 * @file    Format.hpp
 * @brief   Value formatting shared by the Run screens.
 *
 * The TouchGFX app formats through touchgfx::Unicode. Here every decimal is
 * produced with integer arithmetic, which keeps the output independent of
 * the float formatting in the kernel's C library export table.
 * Behaviour matches the Run app face for face: "---" below the display
 * minimums, pace rounded to the nearest second, and the same precision steps.
 ******************************************************************************
 */

#ifndef FORMAT_HPP
#define FORMAT_HPP

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "SDK/Utils/Utils.hpp"
#include "gui/model/Model.hpp"

namespace Fmt
{

/// Write @p value with @p decimals digits after the point, rounded half-up.
inline void fixed(char* buf, size_t n, float value, int decimals)
{
    if (value < 0.0f) {
        value = 0.0f;
    }
    uint32_t scale = 1;
    for (int i = 0; i < decimals; ++i) {
        scale *= 10;
    }
    const uint32_t scaled = static_cast<uint32_t>(value * static_cast<float>(scale) + 0.5f);
    const uint32_t whole  = scaled / scale;
    const uint32_t frac   = scaled % scale;
    if (decimals == 0) {
        snprintf(buf, n, "%lu", static_cast<unsigned long>(whole));
    } else {
        snprintf(buf, n, "%lu.%0*lu", static_cast<unsigned long>(whole), decimals,
                 static_cast<unsigned long>(frac));
    }
}

/// Seconds per metre -> seconds per km or per mile. 0 stays 0.
inline float paceUnits(float secPerM, bool imperial)
{
    if (secPerM < 1e-6f) {
        return 0.0f;
    }
    const float secPerKm = secPerM * 1000.0f;
    return imperial ? secPerKm / SDK::Utils::kmToMiles(1.0f) : secPerKm;
}

/// Metres -> km or miles.
inline float distUnits(float metres, bool imperial)
{
    const float km = metres / 1000.0f;
    return imperial ? SDK::Utils::kmToMiles(km) : km;
}

/// "m:ss" (or "h:mm" from one hour) for a pace already in display units.
inline void pace(char* buf, size_t n, float paceInUnits)
{
    if (paceInUnits < App::Display::kMinPace) {
        snprintf(buf, n, "---");
        return;
    }
    const auto hms = SDK::Utils::toHMS(static_cast<std::time_t>(paceInUnits + 0.5f));
    if (hms.h > 0) {
        snprintf(buf, n, "%u:%02u", hms.h, hms.m);
    } else {
        snprintf(buf, n, "%u:%02u", hms.m, hms.s);
    }
}

/// Session distance: two decimals below 100, one above.
inline void distanceTotal(char* buf, size_t n, float distInUnits)
{
    if (distInUnits < App::Display::kMinDist) {
        snprintf(buf, n, "---");
    } else if (distInUnits < 100.0f) {
        fixed(buf, n, distInUnits, 2);
    } else {
        fixed(buf, n, distInUnits, 1);
    }
}

/// Lap distance: two decimals below 10, one below 100, none above.
inline void distanceLap(char* buf, size_t n, float distInUnits)
{
    if (distInUnits < App::Display::kMinDist) {
        snprintf(buf, n, "---");
    } else if (distInUnits < 10.0f) {
        fixed(buf, n, distInUnits, 2);
    } else if (distInUnits < 100.0f) {
        fixed(buf, n, distInUnits, 1);
    } else {
        fixed(buf, n, distInUnits, 0);
    }
}

/// "h:mm:ss".
inline void hms(char* buf, size_t n, std::time_t sec)
{
    const auto t = SDK::Utils::toHMS(sec);
    snprintf(buf, n, "%u:%02u:%02u", t.h, t.m, t.s);
}

/// "m:ss", or "h:mm" from one hour.
inline void shortTime(char* buf, size_t n, std::time_t sec)
{
    const auto t = SDK::Utils::toHMS(sec);
    if (t.h > 0) {
        snprintf(buf, n, "%u:%02u", t.h, t.m);
    } else {
        snprintf(buf, n, "%u:%02u", t.m, t.s);
    }
}

/// "8 Oct 2026" in local time. Avoids strftime, which pulls in locale code.
inline void date(char* buf, size_t n, std::time_t utc)
{
    static constexpr const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::tm t{};
    localtime_r(&utc, &t);
    snprintf(buf, n, "%d %s %d", t.tm_mday, kMonths[t.tm_mon], 1900 + t.tm_year);
}

/// Whole beats per minute, or "---" below the physiological minimum.
inline void heartRate(char* buf, size_t n, float bpm)
{
    if (bpm < App::Display::kMinHR) {
        snprintf(buf, n, "---");
    } else {
        fixed(buf, n, bpm, 0);
    }
}

inline const char* units(bool imperial)
{
    return imperial ? "mi" : "km";
}

// --- Lap alert texts ---------------------------------------------------------

/// "OFF", "<n> km" or "<n> mile(s)".
inline void alertDistance(char* buf, size_t n, Settings::Alerts::Distance::Id id, bool imperial)
{
    if (id == Settings::Alerts::Distance::ID_OFF) {
        snprintf(buf, n, "OFF");
        return;
    }
    const unsigned v = Settings::Alerts::Distance::kValues[id];
    if (imperial) {
        snprintf(buf, n, "%u %s", v, v > 1 ? "miles" : "mile");
    } else {
        snprintf(buf, n, "%u km", v);
    }
}

/// "OFF", "<n> min", or with @p longForm "<n> minute(s)".
inline void alertTime(char* buf, size_t n, Settings::Alerts::Time::Id id, bool longForm)
{
    if (id == Settings::Alerts::Time::ID_OFF) {
        snprintf(buf, n, "OFF");
        return;
    }
    const unsigned v = Settings::Alerts::Time::kValues[id];
    if (longForm) {
        snprintf(buf, n, "%u %s", v, v > 1 ? "minutes" : "minute");
    } else {
        snprintf(buf, n, "%u min", v);
    }
}

} // namespace Fmt

#endif // FORMAT_HPP
