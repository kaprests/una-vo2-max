/**
 ******************************************************************************
 * @file    CalibrationResetScreen.cpp
 * @brief   Message after Reset calibration (see the header).
 ******************************************************************************
 */

#include "gui/screens/CalibrationResetScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/theme/Theme.hpp"
#include "gui/Assets.hpp"

namespace
{
constexpr uint32_t kDismissMs = 2000;
} // namespace

CalibrationResetScreen::CalibrationResetScreen(Model& model, bool done)
    : Screen(model)
    , mDone(done)
{
}

CalibrationResetScreen::~CalibrationResetScreen()
{
    if (mDismiss) {
        lv_timer_delete(mDismiss);
    }
}

void CalibrationResetScreen::build()
{
    mTitle = std::make_unique<Widgets::Title>(mRoot, "CALIBRATION");
    if (mDone) {
        Theme::imageTinted(mRoot, &img_circletick_50x50, 95, 75, SDK::GUI::Color::YELLOW_DARK);
        Theme::label(mRoot, Theme::Font::SemiBold30, "Reset", 0, 130, 240);
        Theme::label(mRoot, Theme::Font::Medium18, "Add a lab result\nto recalibrate", 0, 170, 240);
    } else {
        Theme::label(mRoot, Theme::Font::Medium25, "No lab results\nto reset", 0, 95, 240);
    }
}

void CalibrationResetScreen::onShow()
{
    mModel.resetIdleTimer();
    mDismiss = lv_timer_create(&CalibrationResetScreen::dismissCb, kDismissMs, this);
    lv_timer_set_repeat_count(mDismiss, 1);
}

void CalibrationResetScreen::dismissCb(lv_timer_t* t)
{
    auto* self = static_cast<CalibrationResetScreen*>(lv_timer_get_user_data(t));
    self->mDismiss = nullptr;   // one-shot: LVGL deletes the timer after this call
    ScreenManager::instance().goTo(ScreenId::MenuSettings);
}
