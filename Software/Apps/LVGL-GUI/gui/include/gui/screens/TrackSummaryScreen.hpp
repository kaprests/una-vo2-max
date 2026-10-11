/**
 ******************************************************************************
 * @file    TrackSummaryScreen.hpp
 * @brief   Activity summary: VO2 max, map, overview, heart rate and a paged lap list,
 *          browsed with L1/L2.
 *
 * Port of the Run app's TrackSummaryView and its SummaryFace* containers.
 * Shows the current track (from the action menu while paused, R2 returns
 * there; or after a save, R1 leaves the app) or the latest VO2 max result
 * (from the main menu, R2 returns there).
 ******************************************************************************
 */

#ifndef TRACK_SUMMARY_SCREEN_HPP
#define TRACK_SUMMARY_SCREEN_HPP

#include <memory>
#include <vector>

#include "gui/screens/Screen.hpp"
#include "gui/widgets/Widgets.hpp"

class TrackSummaryScreen : public Screen
{
public:
    enum class Source : uint8_t { Track, LatestVo2 };

    TrackSummaryScreen(Model& model, Source source);

    void onShow() override;
    void onKey(uint8_t code) override;

protected:
    void build() override;

private:
    enum Face : uint8_t { FACE_VO2, FACE_MAP, FACE_OVERVIEW, FACE_HEARTRATE, FACE_LAPS, FACE_COUNT };

    static constexpr uint8_t kLapPageSize = 3;   ///< rows advanced per L1/L2 press
    static constexpr uint8_t kLapVisible  = 5;   ///< rows on screen at once
    static constexpr uint8_t kLapColumns  = 5;   ///< index, distance, units, time, pace

    void buildFaceVo2();
    void buildFaceMap();
    void buildFaceOverview();
    void buildFaceHeartRate();
    void buildFaceLaps();

    void setSummary(const ActivitySummary& s);
    void setVo2(const Vo2Result& v);
    void showFace(uint8_t face);
    void updateIndicator();
    void fillLapRows();
    void leave();

    lv_obj_t* mFaces[FACE_COUNT] = {};

    // VO2 max
    lv_obj_t* mVo2Date    = nullptr;
    lv_obj_t* mVo2Value   = nullptr;
    lv_obj_t* mVo2Unit    = nullptr;
    lv_obj_t* mVo2Status  = nullptr;
    lv_obj_t* mVo2Details = nullptr;

    // Map + overview headers
    lv_obj_t* mMapDistance      = nullptr;
    lv_obj_t* mMapUnits         = nullptr;
    lv_obj_t* mOverviewDistance = nullptr;
    lv_obj_t* mOverviewUnits    = nullptr;
    lv_obj_t* mAvgPace          = nullptr;
    lv_obj_t* mTimer            = nullptr;
    std::unique_ptr<Widgets::Map> mMap;

    // Heart rate
    lv_obj_t* mMaxHr = nullptr;
    lv_obj_t* mAvgHr = nullptr;

    // Laps
    lv_obj_t* mLapsCount = nullptr;
    lv_obj_t* mLapsTotal = nullptr;
    lv_obj_t* mLapRows[kLapVisible][kLapColumns] = {};
    const std::vector<LapSummary>* mLaps = nullptr;

    std::unique_ptr<Widgets::Buttons>         mButtons;
    std::unique_ptr<Widgets::ScrollIndicator> mIndicator;
    std::unique_ptr<Widgets::Title>           mTitles[FACE_COUNT];

    const Source mSource;
    uint8_t mFace       = FACE_VO2;
    bool    mIsImperial = false;
    bool    mExitOnR1   = false;
    uint8_t mLapPages   = 0;
    uint8_t mLapPage    = 0;
};

#endif // TRACK_SUMMARY_SCREEN_HPP
