/**
 ******************************************************************************
 * @file    ScreenshotTour.cpp
 * @brief   Simulator-only screenshot tour (see ScreenshotTour.hpp).
 *
 * Each step presses a key, waits, then saves the active screen as raw RGB565
 * (<name>.rgb565, 240 x 240); tools/screenshots.py turns them into PNGs.
 ******************************************************************************
 */

#include "ScreenshotTour.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "lvgl.h"
#include "SDK/GUI/Button.hpp"

namespace
{
namespace Btn = SDK::GUI::Button;

struct Step {
    uint32_t    key;    ///< 0 = no key
    uint32_t    waitMs;
    const char* shot;   ///< nullptr = no screenshot
};

constexpr Step kFull[] = {
    {0,       6000, "01-main"},             // after the GPS fix
    {Btn::L2, 1000, "02-main-history"},
    {Btn::R1, 1000, "03-history-latest"},
    {Btn::L2, 1000, "10-history-trend"},
    {Btn::R1, 1000, "12-history-6m"},
    {Btn::R1, 1000, "13-history-1y"},
    {Btn::R1, 1000, nullptr},               // back to the last 20 runs
    {Btn::L2, 1000, "11-history-list"},
    {Btn::R2, 1000, nullptr},
    {Btn::L2, 1000, nullptr},
    {Btn::L2, 1000, nullptr},
    {Btn::R1, 1000, "08-settings"},
    {Btn::R2, 1000, nullptr},
    {Btn::L1, 1000, nullptr},
    {Btn::L1, 1000, nullptr},
    {Btn::L1, 1000, nullptr},
    {Btn::R1, 1000, "04-guide"},            // first Start shows the guide
    {Btn::R1, 8000, "05-run"},              // wait for pace and HR
    {Btn::L2, 1000, "06-run-hr"},
    {Btn::R1, 1000, "07-pause"},
};

constexpr Step kNoMaxHr[] = {
    {0, 6000, "09-main-no-maxhr"},
};

constexpr int32_t kSize = 240;

LV_DRAW_BUF_DEFINE_STATIC(sBuf, kSize, kSize, LV_COLOR_FORMAT_RGB565);

std::string  sDir;
const Step*  sNext = nullptr;
const Step*  sEnd  = nullptr;
const char*  sShot = nullptr;

void save(const char* name)
{
    if (lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &sBuf) != LV_RESULT_OK) {
        std::fprintf(stderr, "Screenshot %s failed\n", name);
        return;
    }
    const std::string path = sDir + "/" + name + ".rgb565";
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        for (int32_t y = 0; y < kSize; ++y) {
            std::fwrite(sBuf.data + y * sBuf.header.stride, 2, kSize, f);
        }
        std::fclose(f);
    }
}

void tick(lv_timer_t* timer)
{
    if (sShot) {
        save(sShot);
    }
    if (sNext == sEnd) {
        // Skip the simulator's shutdown: it is slow and not needed here.
        std::fflush(nullptr);
        std::_Exit(0);
    }
    const Step& step = *sNext++;
    if (step.key) {
        uint32_t key = step.key;
        lv_obj_send_event(lv_screen_active(), LV_EVENT_KEY, &key);
    }
    sShot = step.shot;
    lv_timer_set_period(timer, step.waitMs);
}

template <std::size_t N>
void run(const Step (&steps)[N])
{
    sNext = steps;
    sEnd  = steps + N;
    LV_DRAW_BUF_INIT_STATIC(sBuf);
    lv_timer_create(&tick, 1, nullptr);
}
} // namespace

void ScreenshotTour::startIfRequested()
{
    const char* dir = std::getenv("VO2_SCREENSHOT_DIR");
    if (!dir) {
        return;
    }
    sDir = dir;
    const char* tour = std::getenv("VO2_SCREENSHOT_TOUR");
    if (tour && std::strcmp(tour, "no-max-hr") == 0) {
        run(kNoMaxHr);
    } else {
        run(kFull);
    }
}
