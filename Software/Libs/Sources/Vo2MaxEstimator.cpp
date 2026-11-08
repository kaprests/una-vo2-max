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
constexpr float kAcsmPerMps    = 0.2f * 60.0f; ///< VO2 per m/s of speed
constexpr float kGradeCostFrac = 0.045f;       ///< ACSM: 0.9 / 0.2 per unit grade, per %

/// ACSM running equation: 3.5 + 0.2 * speed in m/min.
float acsmVo2(float speedMps)
{
    return 3.5f + kAcsmPerMps * speedMps;
}

/// Weight of a first-order lag with time constant @p tau, for a 1 s step.
float lagStep(float tau)
{
    return 1.0f - std::exp(-1.0f / tau);
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
    *this = Vo2MaxEstimator(mConfig);
}

void Vo2MaxEstimator::skip(uint32_t seconds)
{
    mSpeed *= std::exp(-static_cast<float>(seconds) / mConfig.speedLagSec);
    mGrade *= std::exp(-static_cast<float>(seconds) / mConfig.gradeLagSec);
}

void Vo2MaxEstimator::addSample(const Sample& s)
{
    using State = Vo2Progress::State;
    const Config& c = mConfig;

    ++mElapsed;
    mGrade += lagStep(c.gradeLagSec) * ((s.gradeValid ? s.gradePct : 0.0f) - mGrade);
    if (s.speedMps <= c.maxSpeedMps) {
        const float effort = std::max(s.speedMps * (1.0f + kGradeCostFrac * mGrade), 0.0f);
        mSpeed += lagStep(c.speedLagSec) * (effort - mSpeed);
    }

    if (s.hrBpm < c.minHrBpm || s.hrTrust < c.minHrTrust || s.hrTrust > kMaxHrTrust) {
        mState = State::NO_HR;
    } else if (mElapsed <= c.warmupSec) {
        mState = State::WARMING_UP;
    } else if (mSpeed < c.minSpeedMps || std::fabs(mGrade) > c.maxGradePct) {
        mState = State::UNUSABLE;
    } else {
        const int i = std::clamp(static_cast<int>((s.hrBpm - kBandMinBpm) / kBandBpm), 0, static_cast<int>(kBands) - 1);
        Band& b = mBands[i];
        ++b.sec;
        b.speed += mSpeed;
        b.hr    += s.hrBpm;
        ++mValidSec;
        mState = State::COUNTING;
    }
}

Vo2MaxEstimator::Points Vo2MaxEstimator::points() const
{
    Points p;
    for (const Band& b : mBands) {
        if (b.sec < mConfig.minBandSec) {
            continue;
        }
        const Point pt{static_cast<float>(std::min(b.sec, mConfig.maxBandWeight)), b.speed / b.sec, b.hr / b.sec};
        p.low  = p.count ? std::min(p.low, pt.hr) : pt.hr;
        p.high = p.count ? std::max(p.high, pt.hr) : pt.hr;
        p.items[p.count++] = pt;
    }
    return p;
}

Vo2Result Vo2MaxEstimator::result(float maxHrBpm) const
{
    using Reason     = Vo2Result::Reason;
    using Confidence = Vo2Result::Confidence;
    const Config& c = mConfig;

    Vo2Result r;
    r.validSec = static_cast<uint16_t>(std::min<uint32_t>(mValidSec, std::numeric_limits<uint16_t>::max()));
    const auto fail = [&r](Reason reason) { r.reason = reason; return r; };

    if (maxHrBpm < c.minMaxHr) {
        return fail(Reason::NO_MAX_HR);
    }
    const Points p = points();
    r.hrRangeFrac  = (p.high - p.low) / maxHrBpm;
    if (r.validSec < c.low.validSec) {
        return fail(Reason::TOO_SHORT);
    }
    if (p.count < c.low.bands || r.hrRangeFrac < c.low.hrRangeFrac) {
        return fail(Reason::LOW_HR_RANGE);
    }

    // Weighted least squares through the band means.
    double w = 0, mx = 0, my = 0;
    for (std::size_t i = 0; i < p.count; ++i) {
        w  += p.items[i].weight;
        mx += p.items[i].weight * p.items[i].speed;
        my += p.items[i].weight * p.items[i].hr;
    }
    mx /= w;
    my /= w;
    double sxx = 0, sxy = 0, syy = 0;
    for (std::size_t i = 0; i < p.count; ++i) {
        const double dx = p.items[i].speed - mx, dy = p.items[i].hr - my;
        sxx += p.items[i].weight * dx * dx;
        sxy += p.items[i].weight * dx * dy;
        syy += p.items[i].weight * dy * dy;
    }
    const double slope = sxy / sxx;
    if (!(slope > 0.0)) {
        return fail(Reason::BAD_SLOPE);
    }
    const double intercept = my - slope * mx;
    const double speed     = (maxHrBpm - intercept) / slope;
    r.r2 = static_cast<float>(sxy * sxy / (sxx * syy));

    const float vo2 = acsmVo2(static_cast<float>(speed));
    if (vo2 < kMinVo2 || vo2 > kMaxVo2) {
        return fail(Reason::OUT_OF_RANGE);
    }

    // Standard error of the extrapolated speed (delta method), in VO2 units.
    double sse = 0;
    for (std::size_t i = 0; i < p.count; ++i) {
        const double e = p.items[i].hr - intercept - slope * p.items[i].speed;
        sse += p.items[i].weight * e * e;
    }
    const double s2  = sse / (p.count - 2);
    const double var = s2 * (1.0 / w + (speed - mx) * (speed - mx) / sxx) / (slope * slope);
    if (kAcsmPerMps * std::sqrt(var) > c.maxVo2Se) {
        return fail(Reason::POOR_FIT);
    }

    const auto meets = [&](const Config::Level& l) {
        return r.hrRangeFrac >= l.hrRangeFrac && p.count >= l.bands && r.validSec >= l.validSec;
    };
    r.vo2max     = vo2;
    r.reason     = Reason::OK;
    r.confidence = meets(c.high) ? Confidence::HIGH : meets(c.medium) ? Confidence::MEDIUM : Confidence::LOW;
    return r;
}

Vo2Progress Vo2MaxEstimator::progress(float maxHrBpm) const
{
    const Vo2Result r = result(maxHrBpm);
    const Points    p = points();

    Vo2Progress g;
    g.state      = maxHrBpm < mConfig.minMaxHr ? Vo2Progress::State::NO_MAX_HR : mState;
    g.confidence = r.confidence;
    g.reason     = r.reason;
    g.hrLow      = static_cast<uint8_t>(std::lround(p.low));
    g.hrHigh     = static_cast<uint8_t>(std::lround(p.high));
    g.bands      = p.count;
    g.validSec   = r.validSec;
    return g;
}
