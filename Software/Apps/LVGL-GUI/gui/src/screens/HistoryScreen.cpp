/**
 ******************************************************************************
 * @file    HistoryScreen.cpp
 * @brief   VO2 max history (see HistoryScreen.hpp).
 ******************************************************************************
 */

#include "gui/screens/HistoryScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/theme/Theme.hpp"
#include "gui/Format.hpp"

#include <algorithm>
#include <cmath>

using namespace SDK::GUI;
using F = Theme::Font;
using Vo2Trend::View;

namespace
{
// Plot area inside the chart container.
constexpr int32_t kPlotX  = 44;
constexpr int32_t kPlotY  = 8;
constexpr int32_t kPlotW  = 152;
constexpr int32_t kPlotH  = 90;
constexpr int32_t kDotR   = 3;
constexpr int32_t kChartY = 60;
constexpr int32_t kChartH = kPlotY + kPlotH + 24;   // room for the month labels
constexpr int32_t kNoteY  = kChartY + kChartH + 4;

const char* viewName(View v)
{
    switch (v) {
        case View::MONTHS_6: return "6 months";
        case View::YEAR:     return "12 months";
        default:             return "Last 20 runs";
    }
}
} // namespace

HistoryScreen::HistoryScreen(Model& model)
    : Screen(model)
{
}

void HistoryScreen::build()
{
    mButtons   = std::make_unique<Widgets::Buttons>(mRoot);
    mIndicator = std::make_unique<Widgets::ScrollIndicator>(mRoot, Widgets::ScrollIndicator::kSmall);

    for (uint8_t p = 0; p < PAGE_COUNT; ++p) {
        mPages[p] = Theme::container(mRoot, 0, 0, 240, 240);
    }
    mLatest = std::make_unique<Widgets::Vo2Card>(mPages[PAGE_LATEST]);
    buildTrend(mPages[PAGE_TREND]);
    buildList(mPages[PAGE_LIST]);

    mTitles[PAGE_LATEST] = std::make_unique<Widgets::Title>(mPages[PAGE_LATEST], "LATEST");
    mTitles[PAGE_TREND]  = std::make_unique<Widgets::Title>(mPages[PAGE_TREND], "TREND");
    mTitles[PAGE_LIST]   = std::make_unique<Widgets::Title>(mPages[PAGE_LIST], "HISTORY");
}

void HistoryScreen::buildTrend(lv_obj_t* f)
{
    mChart = Theme::container(f, 0, kChartY, 240, kChartH);
    mMaxLabel   = Theme::label(mChart, F::Regular14, "", 4, kPlotY - 9, 36, LV_TEXT_ALIGN_RIGHT);
    mMinLabel   = Theme::label(mChart, F::Regular14, "", 4, kPlotY + kPlotH - 9, 36, LV_TEXT_ALIGN_RIGHT);
    mFirstMonth = Theme::label(mChart, F::Regular14, "", kPlotX - 20, kPlotY + kPlotH + 6, 40);
    mLastMonth  = Theme::label(mChart, F::Regular14, "", kPlotX + kPlotW - 20, kPlotY + kPlotH + 6, 40);

    mLine = lv_line_create(mChart);
    lv_obj_set_size(mLine, 240, kChartH);
    lv_obj_set_style_line_width(mLine, 2, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(mLine, true, LV_PART_MAIN);
    lv_obj_set_style_line_color(mLine, Theme::rgb(Color::TEAL), LV_PART_MAIN);
    for (auto& dot : mDots) {
        dot = Theme::dot(mChart, 0, 0, kDotR, Color::GRAY);
    }

    mTrendNote = Theme::label(f, F::Regular16, "", 30, kNoteY, 180);
}

void HistoryScreen::buildList(lv_obj_t* f)
{
    for (uint8_t r = 0; r < kRows; ++r) {
        const int32_t y = 60 + r * 26;
        mRows[r][0] = Theme::label(f, F::Regular16, "", 40, y + 2, 110, LV_TEXT_ALIGN_LEFT);
        mRows[r][1] = Theme::label(f, F::Medium18, "", 150, y, 50, LV_TEXT_ALIGN_RIGHT);
    }
}

void HistoryScreen::onShow()
{
    mModel.resetIdleTimer();
    mHistory = mModel.getVo2History();
    if (!mHistory || mHistory->empty()) {
        ScreenManager::instance().goTo(ScreenId::Main);
        return;
    }
    const Vo2History::Entry& latest = mHistory->latest();
    mLatest->set(latest.result, latest.utc);
    setView(View::RUNS);
    showPage(PAGE_LATEST);
}

void HistoryScreen::setView(View view)
{
    mView      = view;
    mSelection = Vo2Trend::select(*mHistory, view, mModel.getLocalTime());
    mListPage  = 0;
    mListPages = static_cast<uint8_t>(std::max<std::size_t>((mSelection.count + kRows - 1) / kRows, 1));
    mIndicator->setCount(static_cast<uint16_t>(PAGE_LIST + mListPages));
    drawTrend();
    fillList();
}

void HistoryScreen::drawTrend()
{
    const Vo2Trend::Selection& s = mSelection;
    const bool timeView = mView != View::RUNS;
    Theme::setHidden(mChart, s.count < 2);
    if (s.count < 2) {
        lv_label_set_text_fmt(mTrendNote, "%s:\nmore runs needed", viewName(mView));
        lv_obj_set_y(mTrendNote, 100);
        return;
    }
    lv_obj_set_y(mTrendNote, kNoteY);

    const auto [lo, hi] = std::minmax_element(s.points.begin(), s.points.begin() + s.count,
        [](const auto& a, const auto& b) { return a.entry->result.vo2max < b.entry->result.vo2max; });
    const float yMin = std::floor(lo->entry->result.vo2max) - 2.0f;
    const float yMax = std::ceil(hi->entry->result.vo2max) + 2.0f;
    lv_label_set_text_fmt(mMaxLabel, "%d", static_cast<int>(yMax));
    lv_label_set_text_fmt(mMinLabel, "%d", static_cast<int>(yMin));

    Theme::setHidden(mFirstMonth, !timeView);
    Theme::setHidden(mLastMonth, !timeView);
    lv_label_set_text(mFirstMonth, Fmt::monthName(s.firstMonth));
    lv_label_set_text(mLastMonth, Fmt::monthName(s.lastMonth));

    for (std::size_t i = 0; i < kMaxPoints; ++i) {
        Theme::setHidden(mDots[i], i >= s.count);
        if (i >= s.count) {
            continue;
        }
        const Vo2Result& r = s.points[i].entry->result;
        const int32_t x = kPlotX + static_cast<int32_t>(std::lround(s.points[i].x * kPlotW));
        const int32_t y = kPlotY + static_cast<int32_t>(std::lround((yMax - r.vo2max) / (yMax - yMin) * kPlotH));
        mPoints[i] = {static_cast<lv_value_precise_t>(x), static_cast<lv_value_precise_t>(y)};
        lv_obj_set_pos(mDots[i], x - kDotR, y - kDotR);
        lv_obj_set_style_bg_color(mDots[i], Theme::rgb(Widgets::vo2Color(r.confidence)), LV_PART_MAIN);
    }
    lv_line_set_points(mLine, mPoints.data(), static_cast<uint32_t>(s.count));

    const float delta = s.points[s.count - 1].entry->result.vo2max - s.points[0].entry->result.vo2max;
    char buf[12];
    Fmt::fixed(buf, sizeof(buf), std::fabs(delta), 1);
    if (timeView) {
        lv_label_set_text_fmt(mTrendNote, "%s  %c%s", viewName(mView), delta < 0 ? '-' : '+', buf);
    } else {
        lv_label_set_text_fmt(mTrendNote, "%u runs  %c%s", static_cast<unsigned>(s.count),
                              delta < 0 ? '-' : '+', buf);
    }
}

void HistoryScreen::fillList()
{
    for (uint8_t r = 0; r < kRows; ++r) {
        const std::size_t idx  = static_cast<std::size_t>(mListPage) * kRows + r;   // newest first
        const bool        used = idx < mSelection.count;
        Theme::setHidden(mRows[r][0], !used);
        Theme::setHidden(mRows[r][1], !used);
        if (!used) {
            continue;
        }
        const Vo2History::Entry& e = *mSelection.points[mSelection.count - 1 - idx].entry;
        char buf[16];
        Fmt::date(buf, sizeof(buf), e.utc);
        lv_label_set_text(mRows[r][0], buf);
        Fmt::fixed(buf, sizeof(buf), e.result.vo2max, 1);
        lv_label_set_text(mRows[r][1], buf);
        lv_obj_set_style_text_color(mRows[r][1], Theme::rgb(Widgets::vo2Color(e.result.confidence)), LV_PART_MAIN);
    }
}

void HistoryScreen::showPage(uint8_t page)
{
    mPage = page;
    for (uint8_t p = 0; p < PAGE_COUNT; ++p) {
        Theme::setHidden(mPages[p], p != page);
    }
    using B = Widgets::Buttons;
    mButtons->set(B::NONE, B::NONE, page == PAGE_TREND ? B::AMBER : B::NONE, B::WHITE);
    mIndicator->setActive(page < PAGE_LIST ? page : static_cast<uint16_t>(PAGE_LIST + mListPage));
}

void HistoryScreen::onKey(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L2:
            if (mPage == PAGE_LIST && mListPage + 1 < mListPages) {
                ++mListPage;
                fillList();
                showPage(PAGE_LIST);
            } else if (mPage < PAGE_LIST) {
                showPage(mPage + 1);
            }
            break;

        case Btn::L1:
            if (mPage == PAGE_LIST && mListPage > 0) {
                --mListPage;
                fillList();
                showPage(PAGE_LIST);
            } else if (mPage > PAGE_LATEST) {
                showPage(mPage - 1);
            }
            break;

        case Btn::R1:
            if (mPage == PAGE_TREND) {
                setView(static_cast<View>((static_cast<int>(mView) + 1) % static_cast<int>(View::COUNT)));
                showPage(PAGE_TREND);
            }
            break;

        case Btn::R2:
            ScreenManager::instance().goTo(ScreenId::Main);
            break;

        default:
            break;
    }
}

void HistoryScreen::onIdleTimeout()
{
    ScreenManager::instance().goTo(ScreenId::Main);
}
