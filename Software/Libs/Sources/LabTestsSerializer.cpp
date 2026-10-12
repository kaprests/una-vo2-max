/**
 ******************************************************************************
 * @file    LabTestsSerializer.cpp
 * @brief   Saves and loads LabTests as JSON (see LabTestsSerializer.hpp).
 *
 * {"entries":[{"utc":..., "vo2max":..., "ignored":true}, ...]}, oldest first;
 * "ignored" is written only when set.
 ******************************************************************************
 */

#include "LabTestsSerializer.hpp"

#include "JsonFile.hpp"

LabTestsSerializer::LabTestsSerializer(const SDK::Kernel& kernel, const char* path)
    : mKernel(kernel)
    , mPath(path)
{
}

bool LabTestsSerializer::save(const LabTests& tests)
{
    return JsonFile::saveEntries(mKernel, mPath, tests.count, [&](SDK::JsonStreamWriter& w, std::size_t i) {
        w.add("utc", static_cast<uint32_t>(tests.entries[i].utc));
        w.add("vo2max", tests.entries[i].vo2max);
        if (tests.entries[i].ignored) {
            w.add("ignored", true);
        }
    });
}

bool LabTestsSerializer::load(LabTests& tests)
{
    tests = {};
    return JsonFile::loadEntries(mKernel, mPath, LabTests::kCapacity, [&](const SDK::JsonStreamReader& r) {
        uint32_t        utc = 0;
        LabTests::Entry e;
        if (r.get("utc", utc) && r.get("vo2max", e.vo2max) && e.vo2max >= LabTests::kMinVo2) {
            e.utc = static_cast<std::time_t>(utc);
            bool ignored = false;
            e.ignored = r.get("ignored", ignored) && ignored;   // optional
            tests.add(e);
        }
    });
}
