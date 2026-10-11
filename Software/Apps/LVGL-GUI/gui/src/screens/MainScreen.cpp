/**
 ******************************************************************************
 * @file    MainScreen.cpp
 * @brief   Pre-activity menu (see MainScreen.hpp).
 ******************************************************************************
 */

#include "gui/screens/MainScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/theme/Theme.hpp"
#include "gui/Assets.hpp"
#include "gui/Strings.hpp"

#include <algorithm>

#define LOG_MODULE_PRX      "MainScreen"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

using namespace SDK::GUI;

namespace
{
// Same items and geometry as MainView::setupItems() in the TouchGFX app.
using Style = WheelMenu::Item::Style;
const WheelMenu::Item kItems[App::MenuNav::Root::ID_COUNT] = {
    // ID_START
    { Style::Simple, "Start", nullptr, &poppins_semibold_35 },
    // ID_HISTORY
    { Style::Simple, "VO2 history" },
    // ID_GUIDE
    { Style::Simple, "How it works", nullptr, &poppins_semibold_25 },
    // ID_SETTINGS
    { Style::Simple, "Settings" },
};
} // namespace

MainScreen::MainScreen(Model& model)
    : Screen(model)
{
}

void MainScreen::build()
{
    std::copy(std::begin(kItems), std::end(kItems), mItems);
    mMenu      = std::make_unique<WheelMenu>(mRoot, mItems, Menu::ID_COUNT);
    // As in MainView::onAnimationMiddle: the lens and R1 hint change half way
    // through the slide, when the incoming item is about to take the centre.
    mMenu->setSlideMidCallback(
        [](void* ctx, uint16_t) { static_cast<MainScreen*>(ctx)->updateBackground(); }, this);
    mButtons   = std::make_unique<Widgets::Buttons>(mRoot);
    mTitle     = std::make_unique<Widgets::Title>(mRoot, Strings::kAppNameUc);
    mSensorRow = std::make_unique<Widgets::SensorStatusRow>(mRoot, 0, 52, 240, 24);

    mButtons->set(Widgets::Buttons::NONE, Widgets::Buttons::NONE,
                  Widgets::Buttons::AMBER, Widgets::Buttons::WHITE);
}

void MainScreen::onShow()
{
    mMenu->select(mModel.menu().get());
    mModel.menu().resetChildren();
    mModel.resetIdleTimer();
    onGpsFix(mModel.hasGpsFix());
    onAccessoryStatus(mModel.getAccessoryState(), "");
    onSettings(mModel.getSettings());
}

void MainScreen::onHide()
{
    mModel.menu().set(mMenu->selected());
}

void MainScreen::onKey(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L1: mMenu->prev(); break;   // lens follows at the slide midpoint
        case Btn::L2: mMenu->next(); break;
        case Btn::R1: confirm(); break;
        case Btn::R2: mModel.exitApp(); break;
        default: break;
    }
}

void MainScreen::confirm()
{
    switch (mMenu->selected()) {
        case Menu::ID_START: {
            const Settings& s = mModel.getSettings();
            if (s.showGuide || !s.guideSeen) {
                ScreenManager::instance().goTo(ScreenId::GuideBeforeRun);
            } else {
                ScreenManager::instance().startRun();
            }
        } break;
        case Menu::ID_HISTORY:
            if (hasHistory()) {
                ScreenManager::instance().goTo(ScreenId::Vo2History);
            }
            break;
        case Menu::ID_GUIDE:
            ScreenManager::instance().goTo(ScreenId::Guide);
            break;
        case Menu::ID_SETTINGS:
            ScreenManager::instance().goTo(ScreenId::MenuSettings);
            break;
        default:
            break;
    }
}

void MainScreen::updateBackground()
{
    // Start without a fix, or an empty history, is greyed out with R1 hidden.
    const uint16_t sel = mMenu->selected();
    const bool blocked = (sel == Menu::ID_START && !mGpsFix)
                      || (sel == Menu::ID_HISTORY && !hasHistory());
    mMenu->setBackground(blocked ? Color::GRAY_DARK : Color::TEAL_DARK);
    mButtons->setR1(blocked ? Widgets::Buttons::NONE : Widgets::Buttons::AMBER);
}

void MainScreen::onIdleTimeout()
{
    if (mMenu->selected() != Menu::ID_START) {
        mModel.exitApp();
    }
}

void MainScreen::onGpsFix(bool acquired)
{
    mGpsFix = acquired;
    mSensorRow->setGps(Widgets::SensorStatusRow::gpsState(acquired));
    updateBackground();
}

void MainScreen::onAccessoryStatus(uint8_t state, const char* /*name*/)
{
    mSensorRow->setHr(Widgets::SensorStatusRow::hrState(state));
}

void MainScreen::onSettings(const Settings& /*settings*/)
{
    const bool noMaxHr = mModel.getMaxHrBpm() == 0;
    WheelMenu::Item& start = mItems[Menu::ID_START];
    start.style      = noMaxHr ? Style::Tip : Style::Simple;
    start.centerFont = noMaxHr ? nullptr : kItems[Menu::ID_START].centerFont;
    start.tip        = noMaxHr ? Strings::kNoMaxHr : nullptr;
    start.tipColor   = Color::YELLOW;
    mMenu->refresh();
}

void MainScreen::onVo2History()
{
    updateBackground();
}

bool MainScreen::hasHistory() const
{
    const Vo2History* h = mModel.getVo2History();
    return h && !h->empty();
}
