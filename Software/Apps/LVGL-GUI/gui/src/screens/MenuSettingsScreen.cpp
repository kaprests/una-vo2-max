/**
 ******************************************************************************
 * @file    MenuSettingsScreen.cpp
 * @brief   Settings menu (see MenuSettingsScreen.hpp).
 ******************************************************************************
 */

#include "gui/screens/MenuSettingsScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/Assets.hpp"

using Style = WheelMenu::Item::Style;

MenuSettingsScreen::MenuSettingsScreen(Model& model)
    : Screen(model)
{
}

void MenuSettingsScreen::build()
{
    mItems[Menu::ID_ALERTS]     = { Style::Simple, "Lap Alerts" };
    mItems[Menu::ID_SHOW_GUIDE] = { Style::Toggle, "Show\nguide", "Show guide", &poppins_semibold_25 };
    mItems[Menu::ID_RESET_CALIBRATION] = { Style::Simple, "Reset\ncalibration", "Reset calibration", &poppins_semibold_25 };

    mMenu      = std::make_unique<WheelMenu>(mRoot, mItems, Menu::ID_COUNT);
    mButtons   = std::make_unique<Widgets::Buttons>(mRoot);
    mButtons->set(Widgets::Buttons::NONE, Widgets::Buttons::NONE,
                  Widgets::Buttons::AMBER, Widgets::Buttons::WHITE);
    mTitle     = std::make_unique<Widgets::Title>(mRoot, "SETTINGS");
    mSensorRow = std::make_unique<Widgets::SensorStatusRow>(mRoot, 0, 52, 240, 24);
}

void MenuSettingsScreen::onShow()
{
    mMenu->select(mModel.menu().settings.get());
    mModel.menu().settings.resetChildren();
    mModel.resetIdleTimer();
    onGpsFix(mModel.hasGpsFix());
    onAccessoryStatus(mModel.getAccessoryState(), "");
    onSettings(mModel.getSettings());
}

void MenuSettingsScreen::onHide()
{
    mModel.menu().settings.set(mMenu->selected());
}

void MenuSettingsScreen::onKey(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L1: mMenu->prev(); break;
        case Btn::L2: mMenu->next(); break;
        case Btn::R1:
            if (mMenu->selected() == Menu::ID_ALERTS) {
                ScreenManager::instance().goTo(ScreenId::MenuAlerts);
            } else if (mMenu->selected() == Menu::ID_RESET_CALIBRATION) {
                const LabTests* labs = mModel.getLabTests();
                if (labs && labs->activeCount() > 0) {
                    // Hold-to-confirm starts on the press, as on the action menu.
                    mModel.setHoldConfirmMode(Model::HoldConfirmMode::ResetCalibration);
                    ScreenManager::instance().goTo(ScreenId::TrackHoldConfirm);
                } else {
                    ScreenManager::instance().goTo(ScreenId::CalibrationResetNone);
                }
            } else {
                Settings s = mModel.getSettings();
                s.showGuide = !s.showGuide;
                mModel.saveSettings(s);
                onSettings(s);
            }
            break;
        case Btn::R2:
            ScreenManager::instance().goTo(ScreenId::Main);
            break;
        default:
            break;
    }
}

void MenuSettingsScreen::onGpsFix(bool acquired)
{
    mSensorRow->setGps(Widgets::SensorStatusRow::gpsState(acquired));
}

void MenuSettingsScreen::onAccessoryStatus(uint8_t state, const char* /*name*/)
{
    mSensorRow->setHr(Widgets::SensorStatusRow::hrState(state));
}

void MenuSettingsScreen::onSettings(const Settings& settings)
{
    mItems[Menu::ID_SHOW_GUIDE].toggleState = settings.showGuide;
    mMenu->refresh();
}
