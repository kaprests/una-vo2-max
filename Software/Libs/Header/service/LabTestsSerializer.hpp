/**
 ******************************************************************************
 * @file    LabTestsSerializer.hpp
 * @brief   Saves and loads LabTests as JSON.
 ******************************************************************************
 */

#ifndef LAB_TESTS_SERIALIZER_HPP
#define LAB_TESTS_SERIALIZER_HPP

#include "SDK/Kernel/Kernel.hpp"

#include "LabTests.hpp"

class LabTestsSerializer
{
public:
    LabTestsSerializer(const SDK::Kernel& kernel, const char* path);

    bool save(const LabTests& tests);
    /// Entries that are missing fields or below LabTests::kMinVo2 are skipped.
    bool load(LabTests& tests);

private:
    const SDK::Kernel& mKernel;
    const char*        mPath;
};

#endif // LAB_TESTS_SERIALIZER_HPP
