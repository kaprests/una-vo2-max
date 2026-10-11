/**
 ******************************************************************************
 * @file    SettingsSerializer.cpp
 * @date    08-04-2025
 * @author  Denys Saienko <denys.saienko@droid-technologies.com>
 * @brief   Serializes/Deserializes settings to a file.
 ******************************************************************************
 *
 ******************************************************************************
 */
#include "SettingsSerializer.hpp"

#include <cassert>

#include "SDK/JSON/JsonStreamWriter.hpp"
#include "SDK/JSON/JsonStreamReader.hpp"

#define LOG_MODULE_PRX      "SettingsSerializer"
#define LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#include "SDK/UnaLogger/Logger.h"


SettingsSerializer::SettingsSerializer(const SDK::Kernel& kernel,
    const char *pathToFile) :
    mKernel(kernel), mPath(pathToFile)
{
    assert(pathToFile != nullptr);
}

bool SettingsSerializer::save(const Settings &settings)
{
    const char *slash = strrchr(mPath, '/');
    if (slash) {
        char buff[SDK::Interface::IFileSystem::skMaxPathLen] { };
        snprintf(buff, sizeof(buff), "%.*s", static_cast<int>(slash - mPath), mPath);
        // Create dir
        if (!mKernel.fs.mkdir(buff)) {
            return false;
        }
    }

    std::unique_ptr<SDK::Interface::IFile> file = mKernel.fs.file(mPath);

    if (!file) {
        return false;
    }

    if (!file->open(true, true)) {
        file.reset();
        return false;
    }

    SDK::JsonStreamWriter writer(file.get());

    writer.startMap();

    writer.add("version",        settings.version);
    writer.add("auto_pause_en",  settings.autoPauseEn);
    writer.add("phone_notif_en", settings.phoneNotifEn);
    writer.add("calib_trace_en", settings.calibTraceEn);
    writer.add("alert_distance_id", static_cast<uint8_t>(settings.alertDistanceId));
    writer.add("alert_time_id",     static_cast<uint8_t>(settings.alertTimeId));
    writer.add("show_guide",     settings.showGuide);
    writer.add("guide_seen",     settings.guideSeen);


    writer.endMap();

    file->flush();
    file->close();

    return true;
}

bool SettingsSerializer::load(Settings &settings)
{
    std::unique_ptr<SDK::Interface::IFile> file = mKernel.fs.file(mPath);

    if (!file) {
        return false;
    }

    if (!file->exist()) {
        file.reset();
        return false;
    }

    if (!file->open(false, false)) {
        file.reset();
        return false;
    }

    size_t fileSize = file->size();
    if (fileSize == 0) { // Check for adequate size
        file->close();
        file.reset();
        return false;
    }

    char* buffer = new (std::nothrow)char[fileSize];
    if (buffer == nullptr) {
        file->close();
        file.reset();
        return false;
    }

    size_t read = 0;
    bool status = file->read(buffer, fileSize, read) && (read == fileSize);

    file->close();
    file.reset();

    if (!status) {
        delete[] buffer;
        return false;
    }

    SDK::JsonStreamReader reader(buffer, fileSize);

    if (!reader.validate()) {
        LOG_ERROR("JSON is invalid\n");
        delete[] buffer;
        return false;
    }

    reader.get("version",        settings.version);
    reader.get("auto_pause_en",  settings.autoPauseEn);
    reader.get("phone_notif_en", settings.phoneNotifEn);
    settings.calibTraceEn = false;  // deterministic default for legacy files
    reader.get("calib_trace_en", settings.calibTraceEn);
    reader.get("show_guide",     settings.showGuide);
    reader.get("guide_seen",     settings.guideSeen);
    uint8_t distId = static_cast<uint8_t>(Settings::Alerts::Distance::ID_DEFAULT);
    uint8_t timeId = static_cast<uint8_t>(Settings::Alerts::Time::ID_OFF);
    reader.get("alert_distance_id", distId);
    reader.get("alert_time_id",     timeId);
    settings.alertDistanceId = distId < Settings::Alerts::Distance::ID_COUNT
        ? static_cast<Settings::Alerts::Distance::Id>(distId)
        : Settings::Alerts::Distance::ID_DEFAULT;
    settings.alertTimeId = timeId < Settings::Alerts::Time::ID_COUNT
        ? static_cast<Settings::Alerts::Time::Id>(timeId)
        : Settings::Alerts::Time::ID_OFF;

    // Distance and time auto-laps are mutually exclusive. Normalize a legacy
    // file that has both enabled by keeping distance (which getLapDivSource()
    // prefers) and clearing time, matching the settings-menu invariant.
    if (settings.alertDistanceId != Settings::Alerts::Distance::ID_OFF) {
        settings.alertTimeId = Settings::Alerts::Time::ID_OFF;
    }

    delete[] buffer;

    return true;
}
