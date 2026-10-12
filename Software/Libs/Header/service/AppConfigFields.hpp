/**
 ******************************************************************************
 * @file    AppConfigFields.hpp
 * @brief   Config fields set in the companion app. Must match the
 *          "configFields" in Output/app-manifest.json.
 ******************************************************************************
 */

#ifndef APP_CONFIG_FIELDS_HPP
#define APP_CONFIG_FIELDS_HPP

#include "SDK/AppConfig/AppConfig.hpp"

namespace AppConfigFields
{
inline constexpr const char* kFileName  = "app_config.json";
inline constexpr const char* kMaxHr     = "maxHr";
inline constexpr const char* kLabVo2max = "labVo2max";
inline constexpr const char* kLabDate   = "labDate";

inline constexpr SDK::AppConfig::Field kFields[] = {
    SDK::AppConfig::intField("maxHr", 0, 0, 230),
    SDK::AppConfig::floatField("labVo2max", 0.0f, 0.0f, 90.0f),
    SDK::AppConfig::stringField("labDate", "", 0, 10),
};
} // namespace AppConfigFields

#endif // APP_CONFIG_FIELDS_HPP
