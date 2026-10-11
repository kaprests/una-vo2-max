/**
 ******************************************************************************
 * @file    ScreenshotTour.hpp
 * @brief   Simulator-only: press through the app and save screenshots.
 *
 * Runs only when VO2_SCREENSHOT_DIR is set (see `mise run screenshots`).
 * VO2_SCREENSHOT_TOUR selects the script: "full" (default) or "no-max-hr".
 ******************************************************************************
 */

#ifndef SCREENSHOT_TOUR_HPP
#define SCREENSHOT_TOUR_HPP

namespace ScreenshotTour
{
/// Start the tour on the LVGL thread if VO2_SCREENSHOT_DIR is set.
void startIfRequested();
} // namespace ScreenshotTour

#endif // SCREENSHOT_TOUR_HPP
