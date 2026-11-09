/**
 ******************************************************************************
 * @file    GuideScreen.cpp
 * @brief   Run guidance (see GuideScreen.hpp).
 ******************************************************************************
 */

#include "gui/screens/GuideScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/theme/Theme.hpp"

#include <iterator>

using B = Widgets::Buttons;

namespace
{
constexpr const char* kText[] = {
    "Set max HR in\nthe UNA app.\nRun on flat ground.\nWarm up 10 min easy.",
    "Run a few min\neach at easy,\nmoderate and hard.\nA wide HR range\nmatters most.",
    "While running,\nnavigate to the\nGUIDE screen for\nlive guidance.",
    "On GUIDE:\nnumber, tick = live HR\nteal = range so far\nbox = HR to aim for",
    "Teal and box\nappear after about\n2 min of running.\nThe number turns\nteal on target.",
    "Medium needs 20+\nmin counted and a\n20% HR range.\nHigh: 25+ min\nand 25%.",
};
} // namespace

GuideScreen::GuideScreen(Model& model, bool beforeRun)
    : Screen(model)
    , mBeforeRun(beforeRun)
{
}

void GuideScreen::build()
{
    static_assert(std::size(kText) == kPages);
    mTitle     = std::make_unique<Widgets::Title>(mRoot, "ABOUT");
    mButtons   = std::make_unique<Widgets::Buttons>(mRoot);
    mButtons->set(B::NONE, B::NONE, mBeforeRun ? B::AMBER : B::NONE, B::WHITE);
    mIndicator = std::make_unique<Widgets::ScrollIndicator>(mRoot, Widgets::ScrollIndicator::kSmall);
    mIndicator->setCount(kPages);
    for (uint8_t p = 0; p < kPages; ++p) {
        mPages[p] = Theme::container(mRoot, 0, 0, 240, 240);
        Theme::label(mPages[p], Theme::Font::Regular18, kText[p], 20, 56, 200);
    }
    showPage(0);
}

void GuideScreen::showPage(uint8_t page)
{
    mPage = page;
    for (uint8_t p = 0; p < kPages; ++p) {
        Theme::setHidden(mPages[p], p != page);
    }
    mIndicator->setActive(page);
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
    if (code == Btn::L1) {
        showPage((mPage + kPages - 1) % kPages);
    } else if (code == Btn::L2) {
        showPage((mPage + 1) % kPages);
    } else if (code == Btn::R1 && mBeforeRun) {
        ScreenManager::instance().startRun();
    } else if (code == Btn::R2) {
        ScreenManager::instance().goTo(ScreenId::Main);
    }
}

void GuideScreen::onIdleTimeout()
{
    ScreenManager::instance().goTo(ScreenId::Main);
}
