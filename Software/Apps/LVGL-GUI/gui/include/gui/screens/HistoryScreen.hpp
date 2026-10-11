/**
 ******************************************************************************
 * @file    HistoryScreen.hpp
 * @brief   VO2 max history: the latest result, a trend chart and a paged
 *          list, browsed with L1/L2. R2 returns to the main menu.
 ******************************************************************************
 */

#ifndef HISTORY_SCREEN_HPP
#define HISTORY_SCREEN_HPP

#include <array>
#include <memory>

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Widgets.hpp"
#include "Vo2History.hpp"

class HistoryScreen : public Screen
{
public:
    explicit HistoryScreen(Model& model);

    void onShow() override;
    void onKey(uint8_t code) override;
    void onIdleTimeout() override;

protected:
    void build() override;

private:
    enum Page : uint8_t { PAGE_LATEST, PAGE_TREND, PAGE_LIST, PAGE_COUNT };

    static constexpr uint8_t kRows = 5;
    static constexpr auto    kCapacity = Vo2History::kCapacity;

    void buildTrend(lv_obj_t* f);
    void buildList(lv_obj_t* f);
    void setTrend(const Vo2History& h);
    void fillList();
    void showPage(uint8_t page);

    const Vo2History* mHistory = nullptr;

    lv_obj_t* mPages[PAGE_COUNT] = {};
    std::unique_ptr<Widgets::Title> mTitles[PAGE_COUNT];
    std::unique_ptr<Widgets::Vo2Card> mLatest;

    // Trend
    lv_obj_t* mChart     = nullptr;
    lv_obj_t* mLine      = nullptr;
    lv_obj_t* mMaxLabel  = nullptr;
    lv_obj_t* mMinLabel  = nullptr;
    lv_obj_t* mTrendNote = nullptr;
    std::array<lv_obj_t*, kCapacity>          mDots{};
    std::array<lv_point_precise_t, kCapacity> mPoints{};

    // List
    lv_obj_t* mRows[kRows][2] = {};   ///< date, value

    std::unique_ptr<Widgets::Buttons>         mButtons;
    std::unique_ptr<Widgets::ScrollIndicator> mIndicator;

    uint8_t mPage      = PAGE_LATEST;
    uint8_t mListPage  = 0;
    uint8_t mListPages = 0;
};

#endif // HISTORY_SCREEN_HPP
