#include "Vo2Guidance.hpp"

#include <gtest/gtest.h>

namespace
{
using State = Vo2Progress::State;
using Tip   = Vo2Guidance::Tip;

constexpr float kMaxHr = 190.0f; // 20 % range = 38 bpm, cap 171, floor 114

Vo2Progress counting(uint8_t low, uint8_t high, Vo2Result::Reason reason = Vo2Result::Reason::OK)
{
    Vo2Progress p;
    p.state  = State::COUNTING;
    p.hrLow  = low;
    p.hrHigh = high;
    p.reason = reason;
    return p;
}
} // namespace

TEST(Vo2Guidance, SendsFasterToTheCap)
{
    const auto g = vo2Guidance(counting(125, 140), kMaxHr);
    EXPECT_EQ(g.tip, Tip::GO_FASTER);
    EXPECT_EQ(g.targetHr, 171);
}

TEST(Vo2Guidance, SendsEasierWhenTheCapIsReached)
{
    const auto g = vo2Guidance(counting(150, 170), kMaxHr);
    EXPECT_EQ(g.tip, Tip::GO_EASIER);
    EXPECT_EQ(g.targetHr, 132);
}

TEST(Vo2Guidance, WideRangeChecksFitThenData)
{
    EXPECT_EQ(vo2Guidance(counting(125, 165, Vo2Result::Reason::POOR_FIT), kMaxHr).tip, Tip::HR_OFF_PACE);
    EXPECT_EQ(vo2Guidance(counting(125, 165), kMaxHr).tip, Tip::KEEP_GOING);
    auto p = counting(125, 165);
    p.confidence = Vo2Result::Confidence::MEDIUM;
    EXPECT_EQ(vo2Guidance(p, kMaxHr).tip, Tip::DONE);
}

TEST(Vo2Guidance, StateOverridesTipButKeepsTarget)
{
    auto p = counting(125, 140);
    p.state = State::UNUSABLE;
    const auto g = vo2Guidance(p, kMaxHr);
    EXPECT_EQ(g.tip, Tip::FLAT_GROUND);
    EXPECT_EQ(g.targetHr, 171);

    p.state = State::NO_MAX_HR;
    EXPECT_EQ(vo2Guidance(p, 0.0f).tip, Tip::SET_MAX_HR);
    EXPECT_EQ(vo2Guidance(Vo2Progress{}, kMaxHr).targetHr, 0);
}
