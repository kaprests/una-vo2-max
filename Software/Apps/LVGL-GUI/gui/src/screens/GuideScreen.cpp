/**
 ******************************************************************************
 * @file    GuideScreen.cpp
 * @brief   Run guidance (see GuideScreen.hpp).
 ******************************************************************************
 */

#include "gui/screens/GuideScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/theme/Theme.hpp"

using B = Widgets::Buttons;

GuideScreen::GuideScreen(Model& model, bool beforeRun)
    : Screen(model)
    , mBeforeRun(beforeRun)
{
}

void GuideScreen::build()
{
    mTitle   = std::make_unique<Widgets::Title>(mRoot, "GUIDE");
    mButtons = std::make_unique<Widgets::Buttons>(mRoot);
    mButtons->set(B::NONE, B::NONE, mBeforeRun ? B::AMBER : B::NONE, B::WHITE);
    Theme::label(mRoot, Theme::Font::Regular18,
                 "Run on flat ground.\nWarm up 10 min easy.\nThen 5+ min easy\nand 5+ min at\nmoderate pace.",
                 20, 66, 200);
}

void GuideScreen::onShow()
{
    mModel.resetIdleTimer();
    if (!mModel.getSettings().guideSeen) {
        Settings s = mModel.getSettings();
        s.guideSeen = true;
        mModel.saveSettings(s);
    }
}

void GuideScreen::onKey(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    if (code == Btn::R1 && mBeforeRun) {
        ScreenManager::instance().startRun();
    } else if (code == Btn::R2) {
        ScreenManager::instance().goTo(ScreenId::Main);
    }
}

void GuideScreen::onIdleTimeout()
{
    ScreenManager::instance().goTo(ScreenId::Main);
}
