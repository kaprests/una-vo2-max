/**
 ******************************************************************************
 * @file    MainScreen.hpp
 * @brief   Pre-activity menu: Start / VO2 history / How it works / Settings, with
 *          the sensor-status row and the app title.
 *
 * Port of the Run app's MainView + MainPresenter. Start needs a GPS fix; without
 * one it asks for confirmation first.
 ******************************************************************************
 */

#ifndef MAIN_SCREEN_HPP
#define MAIN_SCREEN_HPP

#include <memory>

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Widgets.hpp"
#include "gui/widgets/WheelMenu.hpp"

class MainScreen : public Screen
{
public:
    explicit MainScreen(Model& model);

    // Screen
    void onShow() override;
    void onHide() override;
    void onKey(uint8_t code) override;

    // ModelListener
    void onIdleTimeout() override;
    void onSettings(const Settings& settings) override;
    void onVo2History() override;
    void onGpsFix(bool acquired) override;
    void onAccessoryStatus(uint8_t state, const char* name) override;

protected:
    void build() override;

private:
    using Menu = App::MenuNav::Root;

    void confirm();
    void updateBackground();
    bool hasHistory() const;

    bool mGpsFix = false;

    std::unique_ptr<Widgets::Title>           mTitle;
    std::unique_ptr<Widgets::SensorStatusRow> mSensorRow;
    std::unique_ptr<Widgets::Buttons>         mButtons;
    std::unique_ptr<WheelMenu>                mMenu;
    WheelMenu::Item                           mItems[Menu::ID_COUNT] {};
};

#endif // MAIN_SCREEN_HPP
