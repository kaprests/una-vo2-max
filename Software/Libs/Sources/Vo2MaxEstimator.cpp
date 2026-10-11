/**
 ******************************************************************************
 * @file    Vo2MaxEstimator.cpp
 * @brief   VO2 max from a submaximal run (see Vo2MaxEstimator.hpp).
 ******************************************************************************
 */

#include "Vo2MaxEstimator.hpp"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kMinVo2 = 20.0f;  // ml/kg/min; outside this the fit is wrong
constexpr float kMaxVo2 = 90.0f;

// ACSM running equation, horizontal component plus resting 3.5.
float acsmVo2(float speedMs)
{
    return 3.5f + 0.2f * speedMs * 60.0f;
}
} // namespace

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
    breakStretch();
    mN = 0;
    mSx = mSy = mSxx = mSxy = mSyy = 0;
    mHrMin = mHrMax = 0.0f;
}

void Vo2MaxEstimator::breakStretch()
{
    mWindowCount = 0;
    mWindowHead  = 0;
    mSteadyRun   = 0;
}

bool Vo2MaxEstimator::isUsable(const Sample& s) const
{
    if (s.speedMs < mConfig.minSpeedMs || s.speedMs > mConfig.maxSpeedMs) {
        return false;
    }
    if (s.hrBpm < mConfig.minHrBpm || s.hrTrust < mConfig.minHrTrust || s.hrTrust > 3) {
        return false;
    }
    if (s.gradeValid) {
        if (std::fabs(s.gradePct) > mConfig.maxGradePct) {
            return false;
        }
    } else if (mConfig.requireGrade) {
        return false;
    }
    return true;
}

void Vo2MaxEstimator::addSample(const Sample& s)
{
    if (!isUsable(s)) {
        breakStretch();
        return;
    }

    mWindow[mWindowHead] = s.speedMs;
    mWindowHead = (mWindowHead + 1) % kWindowSec;
    if (mWindowCount < kWindowSec) {
        ++mWindowCount;
    }
    if (mWindowCount < kWindowSec) {
        return;
    }

    // Oldest-to-newest order starts at mWindowHead (the buffer is full).
    constexpr std::size_t half = kWindowSec / 2;
    float older = 0.0f, newer = 0.0f;
    for (std::size_t i = 0; i < kWindowSec; ++i) {
        const float v = mWindow[(mWindowHead + i) % kWindowSec];
        (i < half ? older : newer) += v;
    }
    const float mean = (older + newer) / kWindowSec;
    older /= half;
    newer /= kWindowSec - half;
    if (std::fabs(newer - older) > mConfig.steadyDriftMs ||
        std::fabs(s.speedMs - mean) > mConfig.steadyJumpMs) {
        mSteadyRun = 0;
        return;
    }
    if (mSteadyRun < mConfig.settleSec) {
        ++mSteadyRun;
        return;
    }

    const double x = s.speedMs;
    const double y = s.hrBpm;
    if (mN == 0) {
        mHrMin = mHrMax = s.hrBpm;
    } else {
        mHrMin = std::min(mHrMin, s.hrBpm);
        mHrMax = std::max(mHrMax, s.hrBpm);
    }
    ++mN;
    mSx += x;  mSy += y;
    mSxx += x * x;  mSxy += x * y;  mSyy += y * y;
}

Vo2MaxEstimator::Result Vo2MaxEstimator::result(float maxHrBpm) const
{
    Result r;
    r.steadySec = static_cast<uint16_t>(std::min<uint32_t>(mN, UINT16_MAX));
    r.hrMin     = mHrMin;
    r.hrMax     = mHrMax;

    if (maxHrBpm < mConfig.minMaxHr) {
        r.reason = Reason::NO_MAX_HR;
        return r;
    }
    r.hrRangeFrac = (mHrMax - mHrMin) / maxHrBpm;

    if (mN < mConfig.minSteadySec || mN < 2) {
        r.reason = Reason::TOO_SHORT;
        return r;
    }
    if (r.hrRangeFrac < mConfig.minHrRangeFrac) {
        r.reason = Reason::LOW_HR_RANGE;
        return r;
    }

    const double n   = mN;
    const double sxx = mSxx - mSx * mSx / n;
    const double sxy = mSxy - mSx * mSy / n;
    const double syy = mSyy - mSy * mSy / n;
    if (sxx <= 0.0 || syy <= 0.0) {
        r.reason = Reason::LOW_HR_RANGE;
        return r;
    }
    const double slope     = sxy / sxx;
    const double intercept = (mSy - slope * mSx) / n;
    r.slope = static_cast<float>(slope);
    r.r2    = static_cast<float>((sxy * sxy) / (sxx * syy));

    if (slope <= 0.0) {
        r.reason = Reason::BAD_SLOPE;
        return r;
    }
    if (r.r2 < mConfig.minR2) {
        r.reason = Reason::POOR_FIT;
        return r;
    }

    r.vMaxMs = static_cast<float>((maxHrBpm - intercept) / slope);
    const float vo2 = acsmVo2(r.vMaxMs);
    if (vo2 < kMinVo2 || vo2 > kMaxVo2) {
        r.reason = Reason::OUT_OF_RANGE;
        return r;
    }
    r.vo2max = vo2;
    r.reason = Reason::OK;

    const Config& c = mConfig;
    if (r.steadySec >= c.highSteadySec && r.hrRangeFrac >= c.highHrRangeFrac && r.r2 >= c.highR2) {
        r.confidence = Confidence::HIGH;
    } else if (r.steadySec >= c.mediumSteadySec && r.hrRangeFrac >= c.mediumHrRangeFrac && r.r2 >= c.mediumR2) {
        r.confidence = Confidence::MEDIUM;
    } else {
        r.confidence = Confidence::LOW;
    }
    return r;
}
