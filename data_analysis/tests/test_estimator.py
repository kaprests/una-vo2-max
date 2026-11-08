"""Parity with Software/Tests/Vo2MaxEstimator_test.cpp: same scenarios, same expectations."""

import numpy as np
import pytest

from vo2est.estimator import WINDOW_SEC, Config, Confidence, Estimator, run

MAX_HR = 190.0
A, B = 70.0, 25.0  # HR = 70 + 25 v -> v at max HR = 4.8 m/s -> VO2 = 61.1
EXPECTED_VO2 = 61.1
PACES = [2.2, 2.8, 3.4, 4.0]
LEAD_IN = WINDOW_SEC - 1 + Config().settle_sec


def paces(seconds, values=PACES):
    v = np.repeat(values, seconds)
    return A + B * v, v


def test_clean_run_gives_expected_vo2_with_high_confidence():
    _, r = run(*paces(600), MAX_HR)
    assert r.reason == "OK"
    assert r.confidence == Confidence.HIGH
    assert r.vo2max == pytest.approx(EXPECTED_VO2, abs=0.1)
    assert r.r2 == pytest.approx(1.0, abs=1e-4)


def test_lead_in_is_excluded():
    _, r = run(*paces(400, [3.0]), MAX_HR)
    assert r.steady_sec == 400 - LEAD_IN


def test_single_pace_has_too_little_hr_range():
    _, r = run(*paces(1200, [3.0]), MAX_HR)
    assert r.reason == "LOW_HR_RANGE"


def test_max_hr_not_set():
    _, r = run(*paces(600), 0.0)
    assert r.reason == "NO_MAX_HR"
    assert r.confidence == Confidence.NONE


def test_too_short():
    _, r = run(*paces(200, [3.0]), MAX_HR)
    assert r.reason == "TOO_SHORT"


def test_hr_falling_with_speed_is_rejected():
    v = np.repeat(PACES, 600)
    _, r = run(220.0 - B * v, v, MAX_HR)
    assert r.reason == "BAD_SLOPE"


def test_long_gap_breaks_stretch():
    est = Estimator()
    for h, v in zip(*paces(400, [3.0])):
        est.add(h, v)
    for _ in range(Config().max_gap_sec + 1):
        est.add(0.0, 0.0)
    for h, v in zip(*paces(100, [3.0])):
        est.add(h, v)
    assert est.result(MAX_HR).steady_sec == 400 - LEAD_IN + 100 - LEAD_IN


def test_brief_hr_spike_does_not_widen_range():
    est = Estimator()
    for h, v in zip(*paces(600, [2.8, 3.4])):
        est.add(h, v)
    before = est.result(MAX_HR).hr_range_frac
    for _ in range(10):
        est.add(185.0, 3.4)
    assert before == pytest.approx(15.0 / MAX_HR, abs=1e-4)
    assert est.result(MAX_HR).hr_range_frac == pytest.approx(before)
