#ifndef SERVICE_HPP
#define SERVICE_HPP

#include <ctime>   // std::time_t (mLastCalibUtc, startTrack, ...)

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/SensorLayer/SensorConnection.hpp"
#include "SDK/SensorLayer/SensorDataBatch.hpp"
#include "SDK/TrackMap/TrackMapBuilder.hpp"
#include "SDK/Metrics/MonotonicTime.hpp"
#include "SDK/Metrics/MonotonicCounter.hpp"
#include "SDK/Metrics/VariableCounter.hpp"
#include "SDK/Metrics/SpeedSmoother.hpp"
#include "SDK/Metrics/DeltaCounter.hpp"
#include "SDK/Metrics/ThrottledSample.hpp"
#include "SDK/Filters/SimpleLPF.hpp"

#include "SDK/Calibration/OutdoorStrideCalibrator.hpp"

#include "SettingsSerializer.hpp"
#include "ActivitySummarySerializer.hpp"
#include "ActivityWriter.hpp"
#include "Commands.hpp"
#include "WristTiltDetector.hpp"
#include "Vo2MaxEstimator.hpp"
#include "Vo2HistorySerializer.hpp"

class Service : public WristTiltDetector::IListener
{
public:
    Service(SDK::Kernel &kernel);

    virtual ~Service();

    void run();

private:
    // -- Constants ------------------------------------------------------------

    static constexpr uint32_t skBacklightTimeout     = 5000;
    static constexpr uint32_t skSamplePeriod         = 1000;
    static constexpr uint32_t skSampleLatency        = 1000;

    static constexpr float    skMapDistanceThreshold = 10.0f; // meters
    static constexpr uint32_t skMapMaxPoints         = 70;

    static constexpr uint32_t skBatteryLogPeriodMs   = 5 * 60 * 1000;
    static constexpr float    skFusionSampleRateHz   = 100.0f;

    /// Window, in 1 Hz track ticks, over which the live pace / speed readout is
    /// averaged. Ten seconds cuts the GPS speed noise to about a third -- enough
    /// to hold a target pace by -- while still tracking a real change of effort
    /// fast enough to be useful inside an interval repeat.
    static constexpr std::size_t skPaceSmoothingTicks = 10;

    // -- Infrastructure -------------------------------------------------------

    SDK::Kernel&          mKernel;
    bool                  mGuiStarted;

    // -- Settings & persistence -----------------------------------------------

    Settings                  mSettings;
    bool                      mIsImperial = false;
    bool                      mTimeFormat12h = false;
    uint8_t                   mMaxHrBpm = 0;
    SettingsSerializer        mSettingsSerializer;
    ActivitySummary           mSummary;
    ActivitySummarySerializer mActivitySummarySerializer;
    Vo2History                mVo2History;
    Vo2HistorySerializer      mVo2HistorySerializer;
    ActivityWriter            mActivityWriter;
    SDK::TrackMapBuilder      mTrackMapBuilder;

    // -- Sensors --------------------------------------------------------------

    SDK::Sensor::Connection mSensorGpsLocation;
    SDK::Sensor::Connection mSensorGpsSpeed;
    SDK::Sensor::Connection mSensorGpsDistance;
    SDK::Sensor::Connection mSensorPressure;
    SDK::Sensor::Connection mSensorHr;
    SDK::Sensor::Connection mSensorBatteryLevel;
    SDK::Sensor::Connection mSensorBatteryMetrics;
    SDK::Sensor::Connection mSensorWristMotion;
    SDK::Sensor::Connection mSensorFusion;
    SDK::Sensor::Connection mSensorRunningCadence;
    SDK::Sensor::Connection mSensorGrade;
    bool                    mIsSensorsConnected = false;

    struct {
        float cadenceSpm      = 0.0f;
        bool  cadenceValid    = false;
    } mRunningCadence{};

    // -- Outdoor stride calibration inputs (latched per stream) ---------
    struct {
        float gradePct        = 0.0f;
        bool  gradeValid      = false;
    } mGradeData{};
    float       mGpsSpeedMs       = 0.0f; ///< Latest raw GPS speed (instantaneous source).
    bool        mGpsSpeedValid    = false;
    bool        mGpsSpeedFresh    = false; ///< A speed sample arrived since the last track tick.
    bool        mGpsDeadReckoning = false;
    std::time_t mLastCalibUtc     = 0;   ///< For per-tick delta_t.

    // -- Metrics --------------------------------------------------------------

    SDK::Metric::MonotonicTime<SDK::Interface::ISystem> mTimeTracker;
    SDK::Metric::MonotonicCounter<std::time_t>          mTimeCounter;
    SDK::Metric::MonotonicCounter<float>                mDistanceCounter;
    SDK::Metric::VariableCounter                        mSpeedCounter;
    /// Smooths the GPS speed for the live pace / speed readout only; the FIT
    /// record series and the maxima stay on the unsmoothed samples in
    /// mSpeedCounter. The averages are not involved either way -- they come
    /// from the distance and time totals, not from a mean of these samples.
    SDK::Metric::SpeedSmoother<skPaceSmoothingTicks>    mSpeedSmoother;
    SDK::Metric::VariableCounter                        mHrCounter;
    uint8_t                                             mHrSource = 0;      ///< Latest HR source (HeartRateEx::Source) for the icon + FIT hr_source.
    uint8_t                                             mHrOpticalBpm = 0;  ///< Latest raw optical (PPG) bpm, for the FIT hr_optical series.
    uint8_t                                             mHrExternalBpm = 0; ///< Latest raw external (strap) bpm, for the FIT hr_external series.
    SDK::Filter::SimpleLPF                              mAltitudeFilter;
    SDK::Metric::DeltaCounter                           mAltitudeCounter;

    // Battery SoC and voltage are sampled independently;
    // a FIT record is written only when both are due.
    SDK::Metric::ThrottledSample<float, SDK::Interface::ISystem> mBatterySoc;     ///< State of charge, percent
    SDK::Metric::ThrottledSample<float, SDK::Interface::ISystem> mBatteryVoltage; ///< Voltage, volts

    // -- GPS state ------------------------------------------------------------

    struct {
        bool     fix;       // Actual GPS fix
        float    latitude;  // degrees
        float    longitude; // degrees
        float    altitude;  // meters
        uint32_t timestamp; // ms

        void reset()
        {
            fix       = false;
            latitude  = 0.0f;
            longitude = 0.0f;
            altitude  = 0.0f;
            timestamp = 0;
        }
    } mGps{};

    float mSeaLevelPressure = 0.0f; // Pa

    // -- Track state ----------------------------------------------------------

    enum class LapDivSource {
        OFF = 0,
        DISTANCE,
        TIME,
    };

    LapDivSource mLapDivSource        = LapDivSource::OFF;
    Track::State mTrackState          = Track::State::INACTIVE;
    bool         mPreviousGpsFixState = false;
    bool         mGpsInitialConnectFailed = false;  ///< GPS_LOCATION subscribe lost the startup ack race; the retry logs the recovery.
    bool         mGpsWanted = false;                 ///< GPS_LOCATION should stay connected (pre-activity + active track); cleared in disconnect() so the retry never re-wakes the GNSS post-activity.
    bool         mSessionNotEmpty     = false;
    bool         mLapNotEmpty         = false;
    Track::Data  mTrackData{};

    // -- Wrist tilt -----------------------------------------------------------

    WristTiltDetector mWristTiltDetector;
    Vo2MaxEstimator   mVo2Estimator;

    // -- Outdoor stride calibrator ---------------------------------------

    SDK::Calibration::OutdoorStrideCalibrator mCalibrator;

    // -- Lifecycle ------------------------------------------------------------

    void connectGps();
    void connectSensors(); // All except GPS
    void disconnect();
    void onStartGUI();
    void onStopGUI();

    // -- Sensor data dispatch -------------------------------------------------

    void handleSensorsData(uint16_t handle, SDK::Sensor::DataBatch& data);

    // -- Event handlers -------------------------------------------------------

    void handleEvent(const CustomMessage::TrackStart& event);
    void handleEvent(const CustomMessage::TrackStop& event);
    void handleEvent(const CustomMessage::SettingsSave& event);
    void handleEvent(const CustomMessage::TrackPause& event);
    void handleEvent(const CustomMessage::TrackResume& event);
    void handleEvent(const CustomMessage::ManualLap& event);

    // -- Track control --------------------------------------------------------

    void sendInitialInfoToGui();
    void startTrack(std::time_t utc);
    void processTrack();
    void saveLap(float autoLapDistanceM = 0.0f);
    void stopTrack(bool discard);
    void pauseTrack(bool pause);
    void buildPartialSummary();
    ActivityWriter::RecordData prepareRecordData();
    LapDivSource getLapDivSource();

    // -- Notifications --------------------------------------------------------

    void setCapabilities();
    void requestAccessoryPrepare();   // opt in to external HR (pre-warm at GUI start)
    void requestAccessoryRelease();
    void notifyFirstFix();
    void notifyLapEnd();
    void notifyNewActivity();
    void backlightOn(uint32_t timeoutMs = skBacklightTimeout);
    void playBuzzerPattern(uint16_t beepMs, uint8_t count = 1, uint16_t silenceMs = 100);
    void playVibroPattern(SDK::Message::RequestVibroPlay::Effect effect, uint8_t count = 1, uint16_t silenceMs = 100);

    // -- WristTilt callback ---------------------------------------------------

    virtual void onWristTilt(uint32_t timestampMs) override;
};

#endif // SERVICE_HPP
