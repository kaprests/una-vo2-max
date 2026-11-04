# Host unit tests for the app's service code (GoogleTest).
# Follows the SDK's harness (Docs/unit-testing.md) and reuses its GoogleTest
# fetch and kernel test doubles; kept in this repo because una-sdk/ is a pinned,
# unmodified clone.
cmake_minimum_required(VERSION 3.21)
project(vo2max-host-tests CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("$ENV{UNA_SDK}" STREQUAL "")
    message(FATAL_ERROR "UNA_SDK environment variable is not set.")
endif()
set(SDK  $ENV{UNA_SDK})
set(LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../Libs)

include(${SDK}/Tests/Host/cmake/FetchGoogleTest.cmake)

add_executable(vo2max-host-tests
    Vo2MaxEstimator_test.cpp
    ActivitySummarySerializer_test.cpp
    SettingsSerializer_test.cpp
    Vo2History_test.cpp
    Vo2Trend_test.cpp
    LabTests_test.cpp
    Vo2Calibration_test.cpp
    Vo2Guidance_test.cpp
    ${LIBS}/Sources/Vo2MaxEstimator.cpp
    ${LIBS}/Sources/ActivitySummarySerializer.cpp
    ${LIBS}/Sources/SettingsSerializer.cpp
    ${LIBS}/Sources/Vo2HistorySerializer.cpp
    ${LIBS}/Sources/LabTestsSerializer.cpp
    ${LIBS}/Sources/JsonFile.cpp
    ${LIBS}/Sources/Vo2Json.cpp
    ${SDK}/Tests/Host/support/KernelTestDoubles.cpp
    ${SDK}/Libs/Source/UnaLogger/Logger.cpp
    ${SDK}/Libs/Source/JSON/JsonStreamReader.cpp
    ${SDK}/Libs/Source/JSON/JsonStreamWriter.cpp
    ${SDK}/Libs/Source/Simulator/Kernel/Mock/FileSystem.cpp
    ${SDK}/Libs/Source/Wrappers/StdLibWrappers.c
    ${SDK}/ThirdParty/coreJSON/source/core_json.c
)
target_include_directories(vo2max-host-tests PRIVATE
    ${LIBS}/Header/common
    ${LIBS}/Header/service
    ${SDK}/Libs/Header
    ${SDK}/Tests/Host/support
    ${SDK}/ThirdParty/coreJSON/source/include
)
target_compile_options(vo2max-host-tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>)
target_link_libraries(vo2max-host-tests PRIVATE GTest::gtest_main)

# Replays a recorded run (mise run replay).
add_executable(vo2-replay Vo2Replay.cpp ${LIBS}/Sources/Vo2MaxEstimator.cpp)
target_include_directories(vo2-replay PRIVATE ${LIBS}/Header/common ${LIBS}/Header/service)
target_compile_options(vo2-replay PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
include(GoogleTest)
gtest_discover_tests(vo2max-host-tests)
