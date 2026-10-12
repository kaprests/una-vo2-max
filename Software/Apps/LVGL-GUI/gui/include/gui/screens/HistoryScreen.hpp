/**
 ******************************************************************************
 * @file    HistoryScreen.hpp
 * @brief   VO2 max history: the latest result, a trend chart and a paged
 *          list, browsed with L1/L2. On the trend page R1 cycles the view
 *          (last 20 runs, 6 months, 1 year); the list follows it. Lab test
 *          results are shown as rings and "Lab" rows. R2 returns to the main menu.
 ******************************************************************************
 */

#ifndef HISTORY_SCREEN_HPP
#define HISTORY_SCREEN_HPP

#include <array>
#include <ctime>
#include <memory>

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Widgets.hpp"
#include "LabTests.hpp"
#include "Vo2Trend.hpp"

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

    static constexpr uint8_t kRows      = 5;
    static constexpr auto    kMaxPoints = Vo2Trend::kMaxPoints;
    static constexpr auto    kMaxLabs   = LabTests::kCapacity;
    static constexpr auto    kMaxRings  = Vo2Trend::kMaxLabMarks;

    struct ListRow {
        std::time_t utc;
        float       vo2max;
        uint32_t    color;
        bool        lab;
    };

    void buildTrend(lv_obj_t* f);
    void buildList(lv_obj_t* f);
    void setLatest();
    void setView(Vo2Trend::View view);
    void drawTrend();
    void fillList();
    void showPage(uint8_t page);

    const Vo2History*   mHistory = nullptr;
    const LabTests*     mLabs    = nullptr;
    Vo2Trend::View      mView    = Vo2Trend::View::RUNS;
    Vo2Trend::Selection mSelection;
    Vo2Trend::LabMarks  mLabMarks;

    lv_obj_t* mPages[PAGE_COUNT] = {};
    std::unique_ptr<Widgets::Title>   mTitles[PAGE_COUNT];
    std::unique_ptr<Widgets::Vo2Card> mLatest;
    lv_obj_t* mLatestLab = nullptr;

    // Trend
    lv_obj_t* mChart      = nullptr;
    lv_obj_t* mLine       = nullptr;
    lv_obj_t* mMaxLabel   = nullptr;
    lv_obj_t* mMinLabel   = nullptr;
    lv_obj_t* mFirstMonth = nullptr;
    lv_obj_t* mLastMonth  = nullptr;
    lv_obj_t* mTrendNote  = nullptr;
    std::array<lv_obj_t*, kMaxPoints>          mDots{};
    std::array<lv_point_precise_t, kMaxPoints> mPoints{};
    std::array<lv_obj_t*, kMaxRings>           mLabRings{};

    // List, newest first
    lv_obj_t* mRows[kRows][2] = {};   ///< date, value
    std::array<ListRow, kMaxPoints + kMaxLabs> mListRows{};
    std::size_t mListCount = 0;

    std::unique_ptr<Widgets::Buttons>         mButtons;
    std::unique_ptr<Widgets::ScrollIndicator> mIndicator;

    uint8_t mPage      = PAGE_LATEST;
    uint8_t mListPage  = 0;
    uint8_t mListPages = 0;
};

#endif // HISTORY_SCREEN_HPP
