/**
 ******************************************************************************
 * @file    TrackScreen.cpp
 * @brief   The live activity screen (see TrackScreen.hpp).
 ******************************************************************************
 */

#include "gui/screens/TrackScreen.hpp"
#include "gui/screens/ScreenManager.hpp"
#include "gui/theme/Theme.hpp"
#include "gui/Assets.hpp"
#include "gui/Format.hpp"
#include "gui/Strings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Vo2Guidance.hpp"

#include "SDK/Utils/ClockTime.hpp"

using namespace SDK::GUI;
using F = Theme::Font;

namespace
{
constexpr uint16_t kFaceCount = App::MenuNav::TrackView::ID_COUNT;

// VO2 data face: the bar of usable running time spans the High threshold.
constexpr int32_t kDataX = 50;
constexpr int32_t kDataY = 142;
constexpr int32_t kDataW = 140;
constexpr int32_t kDataH = 6;

// Status face clock geometry (TrackFaceStatus.hpp).
constexpr int32_t kTimeY       = 63;
constexpr int32_t kMeridiemY   = 105;
constexpr int32_t kMeridiemGap = 5;

/// SDK HeartRateEx::Source: 0 none, 1 optical, 2 external.
const char* hrSourceText(uint8_t source)
{
    switch (source) {
        case 1:  return "OPT";
        case 2:  return "EXT";
        default: return "";
    }
}

void tipText(char* buf, std::size_t size, const Vo2Guidance& g, const Vo2Progress& p)
{
    using Tip = Vo2Guidance::Tip;
    switch (g.tip) {
        case Tip::SET_MAX_HR:  std::snprintf(buf, size, "%s", Strings::kNoMaxHr); break;
        case Tip::NO_HR:       std::snprintf(buf, size, "Weak HR signal"); break;
        case Tip::FLAT_GROUND: std::snprintf(buf, size, "Too slow or steep\nto count"); break;
        case Tip::GO_FASTER:   std::snprintf(buf, size, "Faster: HR %u\nfor a few min", g.targetHr); break;
        case Tip::GO_EASIER:   std::snprintf(buf, size, "Easier: HR %u\nfor a few min", g.targetHr); break;
        case Tip::HR_OFF_PACE: std::snprintf(buf, size, "Spread your\nHR range"); break;
        case Tip::KEEP_GOING: {
            const unsigned left = p.validSec < kVo2Medium.validSec ? (kVo2Medium.validSec - p.validSec + 59) / 60 : 0;
            std::snprintf(buf, size, left ? "Keep going\n%u min more" : "Keep going", left);
            break;
        }
        case Tip::DONE:        std::snprintf(buf, size, "Good data"); break;
    }
}

void setHidden(lv_obj_t* obj, bool hidden)
{
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}
} // namespace

TrackScreen::TrackScreen(Model& model)
    : Screen(model)
{
}

void TrackScreen::build()
{
    mIndicator = std::make_unique<Widgets::ScrollIndicator>(mRoot, Widgets::ScrollIndicator::kSmall);
    mButtons = std::make_unique<Widgets::Buttons>(mRoot);
    mButtons->set(Widgets::Buttons::NONE, Widgets::Buttons::NONE,
                  Widgets::Buttons::NONE, Widgets::Buttons::AMBER);

    buildFaceStatus();
    buildFaceVo2();
    buildFaceLap();
    buildFaceTotal();
}

void TrackScreen::buildFaceTotal()
{
    lv_obj_t* f = mFaceTotal = Theme::container(mRoot, 0, 0, 240, 240);
    Theme::label(f, F::Italic18, "Pace", 0, 10, 240);
    // Value boxes are wider than the design's so a slow pace ("22:00") is not clipped.
    mPaceValue = Theme::label(f, F::SemiBold40, Strings::kNoValue, 0, 30, 240);
    Theme::hline(f, 35, 79, 170);
    Theme::label(f, F::Italic18, "Distance", 0, 88, 240);
    mDistanceValue = Theme::label(f, F::SemiBold35, Strings::kNoValue, 53, 111, 134);
    mDistanceUnits = Theme::label(f, F::Regular18, Strings::kKm, 178, 129, 45, LV_TEXT_ALIGN_LEFT);
    Theme::hline(f, 35, 159, 170);
    mTimerValue = Theme::label(f, F::SemiBold35, "0:00:00", 35, 162, 170);
    Theme::label(f, F::Italic18, "Timer", 0, 204, 240);
}

void TrackScreen::buildFaceLap()
{
    lv_obj_t* f = mFaceLap = Theme::container(mRoot, 0, 0, 240, 240);
    mHrZone  = std::make_unique<Widgets::HeartRateZone>(f, 15, 4);
    mHrValue = Theme::label(f, F::SemiBold40, Strings::kNoValue, 75, 29, 90);
    mHrSourceLabel = Theme::label(f, F::Regular14, "", 160, 32, 45, LV_TEXT_ALIGN_LEFT);
    Theme::label(f, F::Regular18, "HR", 160, 50, 45, LV_TEXT_ALIGN_LEFT);
    Theme::hline(f, 35, 79, 170);
    Theme::label(f, F::Italic18, "Lap Pace", 20, 88, 88);
    // Same centres as the design (68 and 173) but wide enough for "22:00".
    mLapPaceValue = Theme::label(f, F::SemiBold35, Strings::kNoValue, 9, 111, 118);
    Theme::vline(f, 119, 79, 80);
    Theme::label(f, F::Italic18, "Lap Dist.", 132, 88, 88);
    mLapDistValue = Theme::label(f, F::SemiBold35, Strings::kNoValue, 114, 111, 118);
    Theme::hline(f, 35, 159, 170);
    mLapTimerValue = Theme::label(f, F::SemiBold35, "0:00:00", 35, 162, 170);
    Theme::label(f, F::Italic18, "Lap Time", 75, 204, 91);
}

void TrackScreen::buildFaceVo2()
{
    lv_obj_t* f = mFaceVo2 = Theme::container(mRoot, 0, 0, 240, 240);
    Theme::label(f, F::Italic18, "GUIDE", 60, 11, 120);
    mVo2Hr      = Theme::label(f, F::SemiBold40, Strings::kNoValue, 75, 29, 90);
    mVo2Source  = Theme::label(f, F::Regular14, "", 160, 50, 45, LV_TEXT_ALIGN_LEFT);
    mHrStrip    = std::make_unique<Widgets::HrStrip>(f, 30, 88, 180);
    mDataText   = Theme::label(f, F::Regular16, "", 10, 118, 220);
    Theme::box(f, kDataX, kDataY, kDataW, kDataH, Color::GRAY_DARK, LV_RADIUS_CIRCLE);
    mDataFill = Theme::box(f, kDataX, kDataY, 0, kDataH, Color::TEAL, LV_RADIUS_CIRCLE);
    for (const Vo2Level& level : {kVo2Low, kVo2Medium}) {
        Theme::box(f, kDataX + kDataW * level.validSec / kVo2High.validSec, kDataY - 3, 2, kDataH + 6,
                   Color::WHITE);
    }
    mVo2Tip = Theme::label(f, F::Medium18, "", 20, 156, 200);
}

void TrackScreen::buildFaceStatus()
{
    lv_obj_t* f = mFaceStatus = Theme::container(mRoot, 0, 0, 240, 240);
    mSensorRow = std::make_unique<Widgets::SensorStatusRow>(f, 0, 20, 240, 24);
    Theme::hline(f, 35, 62, 170);
    // Digits and AM/PM are sized to their text and centred as a group in setTime().
    mDayTime = Theme::label(f, F::SemiBold60, "--:--", 0, kTimeY, LV_SIZE_CONTENT, LV_TEXT_ALIGN_LEFT);
    mMeridiem = Theme::label(f, F::Medium18, "AM", 0, kMeridiemY, LV_SIZE_CONTENT, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(mMeridiem, LV_OBJ_FLAG_HIDDEN);
    Theme::hline(f, 35, 136, 170);
    mBattery = std::make_unique<Widgets::Battery>(f, 76, 153);
    mPercent = Theme::label(f, F::Medium25, "0%", 42, 187, 157);
}

void TrackScreen::onShow()
{
    mModel.menu().track.action.reset();

    const Track::Data& data = mModel.getTrackData();
    mIndicator->setCount(kFaceCount);
    const uint16_t face = mModel.menu().track.get();
    showFace(face < kFaceCount ? face : static_cast<uint16_t>(FaceId::ID_DEFAULT));

    mIsImperial       = mModel.isUnitsImperial();
    mIs12Hour         = mModel.is12HourFormat();
    mHrThresholdCount = mModel.getHrThresholdsCount() < App::Config::kHrThresholdsCount
                            ? mModel.getHrThresholdsCount()
                            : static_cast<uint8_t>(App::Config::kHrThresholdsCount);
    memcpy(mHrThresholds, mModel.getHrThresholds(), mHrThresholdCount);

    onTrackData(data);
    uint8_t h = 0, m = 0, s = 0;
    mModel.getTime(h, m, s);
    setTime(h, m);
    onBatteryLevel(mModel.getBatteryLevel());
    onGpsFix(mModel.hasGpsFix());
    onAccessoryStatus(mModel.getAccessoryState(), "");
}

void TrackScreen::onHide()
{
    mModel.menu().track.set(mFaceId);
}

void TrackScreen::showFace(uint16_t id)
{
    mFaceId = id;
    setHidden(mFaceTotal,     id != FaceId::ID_TRACK1);
    setHidden(mFaceLap,       id != FaceId::ID_TRACK2);
    setHidden(mFaceVo2,       id != FaceId::ID_TRACK3);
    setHidden(mFaceStatus,    id != FaceId::ID_TRACK4);
    mIndicator->setActive(id);
}

void TrackScreen::onKey(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L1:
            showFace(mFaceId == 0 ? static_cast<uint16_t>(kFaceCount - 1) : static_cast<uint16_t>(mFaceId - 1));
            break;
        case Btn::L2:
            showFace(mFaceId + 1 >= kFaceCount ? uint16_t{0} : static_cast<uint16_t>(mFaceId + 1));
            break;
        case Btn::R1:
            ScreenManager::instance().goTo(ScreenId::TrackAction);
            break;
        case Btn::R2:
            mModel.saveLap();
            ScreenManager::instance().goTo(ScreenId::TrackLap);
            break;
        default:
            break;
    }
}

void TrackScreen::onTrackData(const Track::Data& data)
{
    char buf[32];

    // Totals face
    Fmt::pace(buf, sizeof(buf), Fmt::paceUnits(data.pace, mIsImperial));
    lv_label_set_text(mPaceValue, buf);
    Fmt::distanceTotal(buf, sizeof(buf), Fmt::distUnits(data.distance, mIsImperial));
    lv_label_set_text(mDistanceValue, buf);
    lv_label_set_text(mDistanceUnits, Fmt::units(mIsImperial));
    Fmt::hms(buf, sizeof(buf), data.totalTime);
    lv_label_set_text(mTimerValue, buf);

    // Lap face
    Fmt::pace(buf, sizeof(buf), Fmt::paceUnits(data.lapPace, mIsImperial));
    lv_label_set_text(mLapPaceValue, buf);
    Fmt::distanceLap(buf, sizeof(buf), Fmt::distUnits(data.lapDistance, mIsImperial));
    lv_label_set_text(mLapDistValue, buf);
    Fmt::hms(buf, sizeof(buf), data.lapTime);
    lv_label_set_text(mLapTimerValue, buf);
    Fmt::heartRate(buf, sizeof(buf), data.hr);
    lv_label_set_text(mHrValue, buf);
    mHrZone->setHR(data.hr < App::Display::kMinHR ? 0.0f : data.hr, mHrThresholds, mHrThresholdCount);

    mHrSource = data.hrSource;
    const char* source = data.hr < App::Display::kMinHR ? "" : hrSourceText(mHrSource);
    lv_label_set_text(mHrSourceLabel, source);
    updateHrIcon();

    // VO2 data face
    const Vo2Progress& p         = data.vo2;
    const float        maxHr     = mModel.getMaxHrBpm();
    const Vo2Guidance  g         = vo2Guidance(p, maxHr);
    const bool         onTarget  = g.targetHr && std::fabs(data.hr - g.targetHr) <= Vo2Guidance::kOnTarget;
    Fmt::heartRate(buf, sizeof(buf), data.hr);
    lv_label_set_text(mVo2Hr, buf);
    lv_obj_set_style_text_color(mVo2Hr, Theme::rgb(onTarget ? Color::TEAL : Color::WHITE), LV_PART_MAIN);
    lv_label_set_text(mVo2Source, source);
    mHrStrip->set(maxHr, p.hrLow, p.hrHigh, g.targetHr, data.hr);
    static const char* const kLevel[] = {"No est.", "Low", "Medium", "High"};
    lv_label_set_text_fmt(mDataText, "%s, %u min counted", kLevel[static_cast<int>(p.confidence)], p.validSec / 60u);
    lv_obj_set_style_text_color(mDataText, Theme::rgb(Widgets::vo2Color(p.confidence)), LV_PART_MAIN);
    lv_obj_set_width(mDataFill, kDataW * std::min<int32_t>(p.validSec, kVo2High.validSec) / kVo2High.validSec);
    tipText(buf, sizeof(buf), g, p);
    lv_label_set_text(mVo2Tip, buf);
}

void TrackScreen::onBatteryLevel(uint8_t level)
{
    mBattery->setLevel(level);
    lv_label_set_text_fmt(mPercent, "%u%%", level);
}

void TrackScreen::onTime(uint8_t hour, uint8_t minute, uint8_t /*sec*/)
{
    setTime(hour, minute);
}

void TrackScreen::setTime(uint8_t h, uint8_t m)
{
    const SDK::Clock::Hour12 civil = SDK::Clock::to12Hour(h);
    lv_label_set_text_fmt(mDayTime, "%u:%02u", mIs12Hour ? civil.hour : h, m);

    // Centre digits (+ suffix) as one group on the 240 px face, as the Run app does.
    lv_obj_update_layout(mDayTime);
    const int32_t timeW = lv_obj_get_width(mDayTime);
    int32_t groupW = timeW;
    int32_t merW   = 0;
    if (mIs12Hour) {
        lv_label_set_text(mMeridiem, civil.pm ? "PM" : "AM");
        lv_obj_remove_flag(mMeridiem, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(mMeridiem);
        merW   = lv_obj_get_width(mMeridiem);
        groupW = timeW + kMeridiemGap + merW;
    } else {
        lv_obj_add_flag(mMeridiem, LV_OBJ_FLAG_HIDDEN);
    }
    const int32_t left = (240 - groupW) / 2;
    lv_obj_set_pos(mDayTime, left, kTimeY);
    if (mIs12Hour) {
        lv_obj_set_pos(mMeridiem, left + timeW + kMeridiemGap, kMeridiemY);
    }
}

void TrackScreen::onLapChanged(uint8_t /*lapEnd*/)
{
    ScreenManager::instance().goTo(ScreenId::TrackLap);
}

void TrackScreen::onGpsFix(bool acquired)
{
    mSensorRow->setGps(Widgets::SensorStatusRow::gpsState(acquired));
}

void TrackScreen::onAccessoryStatus(uint8_t state, const char* /*name*/)
{
    mAccessoryState = state;
    updateHrIcon();
}

void TrackScreen::updateHrIcon()
{
    mSensorRow->setHr(Widgets::SensorStatusRow::hrStateFromSource(mAccessoryState, mHrSource));
}
