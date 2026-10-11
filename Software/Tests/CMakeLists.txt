# Host unit tests for the app's service code (GoogleTest).
# Follows the SDK's harness (Docs/unit-testing.md) and reuses its GoogleTest
# fetch; kept in this repo because una-sdk/ is a pinned, unmodified clone.
cmake_minimum_required(VERSION 3.21)
project(vo2max-host-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

if("$ENV{UNA_SDK}" STREQUAL "")
    message(FATAL_ERROR "UNA_SDK environment variable is not set.")
endif()
include($ENV{UNA_SDK}/Tests/Host/cmake/FetchGoogleTest.cmake)

set(LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../Libs)

add_executable(vo2max-host-tests
    Vo2MaxEstimator_test.cpp
    ${LIBS}/Sources/Vo2MaxEstimator.cpp
)
target_include_directories(vo2max-host-tests PRIVATE ${LIBS}/Header/service)
target_link_libraries(vo2max-host-tests PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(vo2max-host-tests)
