/**
 ******************************************************************************
 * @file    CalibrationResetScreen.hpp
 * @brief   Short message after Reset calibration: done, or nothing to reset.
 *          Returns to Settings by itself.
 ******************************************************************************
 */

#ifndef CALIBRATION_RESET_SCREEN_HPP
#define CALIBRATION_RESET_SCREEN_HPP

#include <memory>

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Widgets.hpp"

class CalibrationResetScreen : public Screen
{
public:
    CalibrationResetScreen(Model& model, bool done);
    ~CalibrationResetScreen() override;

    void onShow() override;

protected:
    void build() override;

private:
    static void dismissCb(lv_timer_t* t);

    bool        mDone;
    lv_timer_t* mDismiss = nullptr;
    std::unique_ptr<Widgets::Title> mTitle;
};

#endif // CALIBRATION_RESET_SCREEN_HPP
