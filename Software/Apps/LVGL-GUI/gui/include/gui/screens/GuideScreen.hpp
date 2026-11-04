/**
 ******************************************************************************
 * @file    GuideScreen.hpp
 * @brief   How to run for a VO2 max estimate, in pages turned with L1/L2.
 *          Before a run R1 starts it; R2 goes back.
 ******************************************************************************
 */

#ifndef GUIDE_SCREEN_HPP
#define GUIDE_SCREEN_HPP

#include <array>
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
    static constexpr uint8_t kPages = 6;

    void showPage(uint8_t page);

    const bool mBeforeRun;
    uint8_t    mPage = 0;

    std::array<lv_obj_t*, kPages>             mPages{};
    std::unique_ptr<Widgets::Title>           mTitle;
    std::unique_ptr<Widgets::Buttons>         mButtons;
    std::unique_ptr<Widgets::ScrollIndicator> mIndicator;
};

#endif // GUIDE_SCREEN_HPP
