/**
 ******************************************************************************
 * @file    GuideScreen.hpp
 * @brief   How to run for a VO2 max estimate. Before a run R1 starts it and
 *          R2 goes back; from the menu only R2, back.
 ******************************************************************************
 */

#ifndef GUIDE_SCREEN_HPP
#define GUIDE_SCREEN_HPP

#include <memory>

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Widgets.hpp"

class GuideScreen : public Screen
{
public:
    GuideScreen(Model& model, bool beforeRun);

    void onShow() override;
    void onKey(uint8_t code) override;
    void onIdleTimeout() override;

protected:
    void build() override;

private:
    const bool mBeforeRun;

    std::unique_ptr<Widgets::Title>   mTitle;
    std::unique_ptr<Widgets::Buttons> mButtons;
};

#endif // GUIDE_SCREEN_HPP
