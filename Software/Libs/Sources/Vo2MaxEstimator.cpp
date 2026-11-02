/**
 ******************************************************************************
 * @file    Vo2MaxEstimator.cpp
 * @brief   VO2 max from a submaximal run (see Vo2MaxEstimator.hpp).
 ******************************************************************************
 */

#include "Vo2MaxEstimator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr float kMinVo2 = 20.0f; // ml/kg/min
constexpr float kMaxVo2 = 90.0f;

/// ACSM running equation: 3.5 + 0.2 * speed in m/min.
float acsmVo2(float speedMps)
{
    return 3.5f + 0.2f * speedMps * 60.0f;
}
} // namespace

void Vo2MaxEstimator::LinearFit::add(double x, double y)
{
    ++n;
    sx += x;  sy += y;
    sxx += x * x;  sxy += x * y;  syy += y * y;
}

Vo2MaxEstimator::Vo2MaxEstimator()
    : Vo2MaxEstimator(Config{})
{
}

Vo2MaxEstimator::Vo2MaxEstimator(const Config& config)
    : mConfig(config)
{
}

void Vo2MaxEstimator::reset()
{
    *this = Vo2MaxEstimator(mConfig);
}

void Vo2MaxEstimator::breakStretch()
{
    mWindowCount = 0;
    mWindowHead  = 0;
    mSteadySec   = 0;
    mGapSec      = 0;
}

bool Vo2MaxEstimator::hrUsable(const Sample& s) const
{
    return s.hrBpm >= mConfig.minHrBpm && s.hrTrust >= mConfig.minHrTrust && s.hrTrust <= kMaxHrTrust;
}

bool Vo2MaxEstimator::speedUsable(const Sample& s) const
{
    const Config& c = mConfig;
    return s.speedMps >= c.minSpeedMps && s.speedMps <= c.maxSpeedMps
        && (!s.gradeValid || std::fabs(s.gradePct) <= c.maxGradePct);
}

/// Steady: the window's halves agree, and the newest speed is near the mean
/// (catches a pace change on its first second).
bool Vo2MaxEstimator::windowSteady(float newest) const
{
    constexpr std::size_t half = kWindowSec / 2;
    float older = 0.0f, newer = 0.0f;
    for (std::size_t i = 0; i < kWindowSec; ++i) {
        (i < half ? older : newer) += mWindow[(mWindowHead + i) % kWindowSec];
    }
    const float mean = (older + newer) / kWindowSec;
    return std::fabs(newer / (kWindowSec - half) - older / half) <= mConfig.steadyDriftMps
        && std::fabs(newest - mean) <= mConfig.steadyJumpMps;
}

void Vo2MaxEstimator::addSample(const Sample& s)
{
    using State = Vo2Progress::State;

    const bool hrOk = hrUsable(s);
    if (!hrOk || !speedUsable(s)) {
        mState = hrOk ? State::UNUSABLE : State::NO_HR;
        if (++mGapSec > mConfig.maxGapSec) {
            breakStretch();
        }
        return;
    }
    mGapSec = 0;

    mWindow[mWindowHead] = s.speedMps;
    mWindowHead  = (mWindowHead + 1) % kWindowSec;
    mWindowCount = std::min(mWindowCount + 1, kWindowSec);
    if (mWindowCount < kWindowSec) {
        mState = State::SETTLING;
        return;
    }

    if (!windowSteady(s.speedMps)) {
        mSteadySec = 0;
        mState     = State::UNSTEADY;
    } else if (mSteadySec < mConfig.settleSec) {
        ++mSteadySec;
        mState = State::SETTLING;
    } else {
        mFit.add(s.speedMps, s.hrBpm);
        const int bin = static_cast<int>((s.hrBpm - kBinMinBpm) / kBinBpm);
        ++mHrBins[std::clamp(bin, 0, static_cast<int>(kBins) - 1)];
        mState = State::COUNTING;
    }
}

Vo2MaxEstimator::Band Vo2MaxEstimator::hrBand() const
{
    const auto full = [this](uint16_t sec) { return sec >= mConfig.minBinSec; };
    const auto low  = std::find_if(mHrBins.begin(), mHrBins.end(), full);
    if (low == mHrBins.end()) {
        return {};
    }
    const auto high   = std::find_if(mHrBins.rbegin(), mHrBins.rend(), full);
    const auto centre = [](std::ptrdiff_t bin) { return kBinMinBpm + (bin + 0.5f) * kBinBpm; };
    return {centre(low - mHrBins.begin()), centre(mHrBins.rend() - high - 1)};
}

Vo2Result Vo2MaxEstimator::result(float maxHrBpm) const
{
    using Reason = Vo2Result::Reason;
    using Confidence = Vo2Result::Confidence;

    Vo2Result r;
    r.steadySec = static_cast<uint16_t>(std::min<uint32_t>(mFit.n, std::numeric_limits<uint16_t>::max()));

    const auto fail = [&r](Reason reason) { r.reason = reason; return r; };

    if (maxHrBpm < mConfig.minMaxHr) {
        return fail(Reason::NO_MAX_HR);
    }
    const Band band = hrBand();
    r.hrRangeFrac   = (band.high - band.low) / maxHrBpm;

    if (mFit.n < std::max<uint32_t>(mConfig.minimum.steadySec, 2)) {
        return fail(Reason::TOO_SHORT);
    }
    if (r.hrRangeFrac < mConfig.minimum.hrRangeFrac || mFit.varX() <= 0.0 || mFit.varY() <= 0.0) {
        return fail(Reason::LOW_HR_RANGE);
    }

    const double slope = mFit.slope();
    r.r2 = static_cast<float>(mFit.r2());
    if (slope <= 0.0) {
        return fail(Reason::BAD_SLOPE);
    }
    if (r.r2 < mConfig.minimum.r2) {
        return fail(Reason::POOR_FIT);
    }

    const float vo2 = acsmVo2(static_cast<float>((maxHrBpm - mFit.intercept(slope)) / slope));
    if (vo2 < kMinVo2 || vo2 > kMaxVo2) {
        return fail(Reason::OUT_OF_RANGE);
    }

    const auto meets = [&r](const Config::Level& l) {
        return r.steadySec >= l.steadySec && r.hrRangeFrac >= l.hrRangeFrac && r.r2 >= l.r2;
    };
    r.vo2max     = vo2;
    r.reason     = Reason::OK;
    r.confidence = meets(mConfig.high)   ? Confidence::HIGH
                 : meets(mConfig.medium) ? Confidence::MEDIUM
                                         : Confidence::LOW;
    return r;
}

Vo2Progress Vo2MaxEstimator::progress(float maxHrBpm) const
{
    const Vo2Result   r      = result(maxHrBpm);
    const Band        band   = hrBand();
    const bool        fits   = mFit.n > 1 && mFit.varX() > 0.0 && mFit.varY() > 0.0;
    const std::size_t settle = mWindowCount < kWindowSec ? kWindowSec - mWindowCount + mConfig.settleSec
                                                         : mConfig.settleSec - mSteadySec;

    Vo2Progress p;
    p.state      = maxHrBpm < mConfig.minMaxHr ? Vo2Progress::State::NO_MAX_HR : mState;
    p.confidence = r.confidence;
    p.settleSec  = static_cast<uint8_t>(std::min<std::size_t>(settle, std::numeric_limits<uint8_t>::max()));
    p.hrLow      = static_cast<uint8_t>(std::lround(band.low));
    p.hrHigh     = static_cast<uint8_t>(std::lround(band.high));
    p.steadySec  = r.steadySec;
    p.r2         = fits ? static_cast<float>(mFit.r2()) : 0.0f;
    return p;
}
