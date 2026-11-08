"""Parity with Software/Tests/Vo2MaxEstimator_test.cpp: same scenarios, same expectations."""

import numpy as np
import pytest

from vo2est.estimator import Config, Confidence, Estimator, MAX_HR_TRUST

MAX_HR = 190.0
A, B = 70.0, 25.0  # HR = 70 + 25 * lagged speed -> 4.8 m/s at max HR -> VO2 61.1
EXPECTED_VO2 = 61.1


def feed(est, speeds, noise=0.0, seed=7, slope=B, intercept=A):
    """A runner whose HR follows speed through the estimator's lag."""
    rng, a, lagged = np.random.default_rng(seed), 1 - np.exp(-1 / Config().speed_lag), 0.0
    for v in speeds:
        lagged += a * (v - lagged)
        est.add(intercept + slope * lagged + noise * rng.normal(), v, MAX_HR_TRUST)
    return est


def ramp(seconds):
    return 2.0 + 2.4 * np.arange(seconds) / seconds


def test_clean_ramp_gives_expected_vo2_with_high_confidence():
    r = feed(Estimator(), ramp(2400)).result(MAX_HR)
    assert r.reason == "OK"
    assert r.confidence == Confidence.HIGH
    assert r.vo2max == pytest.approx(EXPECTED_VO2, abs=0.3)
    assert r.r2 > 0.99


@pytest.mark.parametrize("minutes, level", [(12, Confidence.LOW), (23, Confidence.MEDIUM), (28, Confidence.HIGH)])
def test_confidence_grows_with_running_time(minutes, level):
    assert feed(Estimator(), ramp(minutes * 60)).result(MAX_HR).confidence == level


def test_long_easy_running_does_not_shift_the_estimate():
    def with_easy(seconds):
        return feed(Estimator(), np.r_[ramp(1800), np.full(seconds, 2.6)], noise=5.0).result(MAX_HR).vo2max
    assert with_easy(5 * 60) == pytest.approx(with_easy(40 * 60), abs=0.5)


def test_single_pace_gives_no_estimate():
    r = feed(Estimator(), np.full(2400, 3.0), noise=2.5).result(MAX_HR)
    assert r.reason == "LOW_HR_RANGE"
    assert r.confidence == Confidence.NONE


def test_max_hr_not_set():
    assert feed(Estimator(), ramp(2400)).result(0.0).reason == "NO_MAX_HR"


def test_too_short():
    assert feed(Estimator(), ramp(300)).result(MAX_HR).reason == "TOO_SHORT"


def test_hr_falling_with_speed_is_rejected():
    assert feed(Estimator(), ramp(2400), slope=-B, intercept=240.0).result(MAX_HR).reason == "BAD_SLOPE"


def test_brief_hr_spike_does_not_widen_range():
    est = feed(Estimator(), ramp(2400))
    before = est.result(MAX_HR).hr_range_frac
    for _ in range(10):
        est.add(185.0, 3.0)
    assert est.result(MAX_HR).hr_range_frac == pytest.approx(before)
