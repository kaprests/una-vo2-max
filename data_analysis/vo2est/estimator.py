"""Python port of the C++ Vo2MaxEstimator, plus candidate fits for comparison.

`Estimator` mirrors Software/Libs/Sources/Vo2MaxEstimator.cpp and must stay in
line with it (tests/test_estimator.py reuses the C++ test cases). The other
functions are alternatives evaluated in the analysis, not app code.
"""

from dataclasses import dataclass, field
from enum import IntEnum

import numpy as np

MIN_VO2, MAX_VO2 = 20.0, 90.0
WINDOW_SEC = 30
MAX_HR_TRUST = 3
BIN_MIN_BPM, BIN_BPM, BINS = 60.0, 5.0, 32


def acsm_vo2(speed_mps):
    """ACSM running equation: 3.5 + 0.2 * speed in m/min."""
    return 3.5 + 0.2 * np.asarray(speed_mps) * 60.0


class Confidence(IntEnum):
    NONE = 0
    LOW = 1
    MEDIUM = 2
    HIGH = 3


@dataclass(frozen=True)
class Level:
    steady_sec: int
    hr_range_frac: float
    r2: float


@dataclass(frozen=True)
class Config:
    min_speed: float = 1.8
    max_speed: float = 6.5
    max_grade: float = 3.0
    min_hr: float = 60.0
    min_hr_trust: int = 1
    steady_drift: float = 0.25
    steady_jump: float = 0.5
    settle_sec: int = 60
    max_gap_sec: int = 5
    min_max_hr: float = 120.0
    min_bin_sec: int = 20
    low: Level = Level(300, 0.10, 0.30)
    medium: Level = Level(600, 0.15, 0.60)
    high: Level = Level(1200, 0.20, 0.80)


@dataclass
class Result:
    reason: str = "TOO_SHORT"
    confidence: Confidence = Confidence.NONE
    vo2max: float = 0.0
    steady_sec: int = 0
    hr_range_frac: float = 0.0
    r2: float = 0.0


@dataclass
class Estimator:
    """Sample-by-sample port of Vo2MaxEstimator (1 Hz)."""

    cfg: Config = field(default_factory=Config)

    def __post_init__(self):
        self.window = []
        self.steady = 0
        self.gap = 0
        self.speeds = []
        self.hrs = []
        self.bins = np.zeros(BINS, dtype=int)

    def _break(self):
        self.window.clear()
        self.steady = 0
        self.gap = 0

    def add(self, hr, speed, trust=MAX_HR_TRUST, grade=0.0, grade_valid=False):
        c = self.cfg
        hr_ok = hr >= c.min_hr and c.min_hr_trust <= trust <= MAX_HR_TRUST
        speed_ok = c.min_speed <= speed <= c.max_speed and (not grade_valid or abs(grade) <= c.max_grade)
        if not (hr_ok and speed_ok):
            self.gap += 1
            if self.gap > c.max_gap_sec:
                self._break()
            return
        self.gap = 0
        self.window.append(speed)
        if len(self.window) > WINDOW_SEC:
            self.window.pop(0)
        if len(self.window) < WINDOW_SEC:
            return
        half = WINDOW_SEC // 2
        older = sum(self.window[:half]) / half
        newer = sum(self.window[half:]) / (WINDOW_SEC - half)
        mean = sum(self.window) / WINDOW_SEC
        if abs(newer - older) > c.steady_drift or abs(speed - mean) > c.steady_jump:
            self.steady = 0
        elif self.steady < c.settle_sec:
            self.steady += 1
        else:
            self.speeds.append(speed)
            self.hrs.append(hr)
            self.bins[min(max(int((hr - BIN_MIN_BPM) / BIN_BPM), 0), BINS - 1)] += 1

    def counted(self):
        """Counted samples as (speed, hr) arrays."""
        return np.array(self.speeds), np.array(self.hrs)

    def band(self):
        full = np.flatnonzero(self.bins >= self.cfg.min_bin_sec)
        if not full.size:
            return 0.0, 0.0
        centre = lambda b: BIN_MIN_BPM + (b + 0.5) * BIN_BPM
        return centre(full[0]), centre(full[-1])

    def result(self, max_hr):
        c, r = self.cfg, Result()
        x, y = self.counted()
        r.steady_sec = len(x)
        if max_hr < c.min_max_hr:
            r.reason = "NO_MAX_HR"
            return r
        low, high = self.band()
        r.hr_range_frac = (high - low) / max_hr
        if len(x) < max(c.low.steady_sec, 2):
            return r
        if r.hr_range_frac < c.low.hr_range_frac or x.var() <= 0 or y.var() <= 0:
            r.reason = "LOW_HR_RANGE"
            return r
        slope, intercept, r.r2 = fit(x, y)
        if slope <= 0:
            r.reason = "BAD_SLOPE"
            return r
        if r.r2 < c.low.r2:
            r.reason = "POOR_FIT"
            return r
        vo2 = float(acsm_vo2((max_hr - intercept) / slope))
        if not MIN_VO2 <= vo2 <= MAX_VO2:
            r.reason = "OUT_OF_RANGE"
            return r
        meets = lambda l: r.steady_sec >= l.steady_sec and r.hr_range_frac >= l.hr_range_frac and r.r2 >= l.r2
        r.vo2max, r.reason = vo2, "OK"
        r.confidence = Confidence.HIGH if meets(c.high) else Confidence.MEDIUM if meets(c.medium) else Confidence.LOW
        return r


def fit(x, y, w=None):
    """Weighted least squares y = a + b x. Returns (b, a, r2)."""
    x, y = np.asarray(x, float), np.asarray(y, float)
    w = np.ones_like(x) if w is None else np.asarray(w, float)
    mx, my = np.average(x, weights=w), np.average(y, weights=w)
    sxx = np.sum(w * (x - mx) ** 2)
    syy = np.sum(w * (y - my) ** 2)
    sxy = np.sum(w * (x - mx) * (y - my))
    b = sxy / sxx
    return b, my - b * mx, sxy * sxy / (sxx * syy)


def run(hr, speed, max_hr, cfg=None, **sample):
    """Feed 1 Hz arrays through the ported estimator."""
    est = Estimator(cfg or Config())
    for h, v in zip(hr, speed):
        est.add(float(h), float(v), **sample)
    return est, est.result(max_hr)


# --- Candidate methods: (hr, speed) 1 Hz arrays in, VO2 max out (NaN if none) ---

def _extend(slope, intercept, max_hr):
    if not slope > 0:
        return np.nan
    vo2 = float(acsm_vo2((max_hr - intercept) / slope))
    return vo2 if MIN_VO2 <= vo2 <= MAX_VO2 else np.nan


def per_second(hr, speed, max_hr):
    """Today's fit, without the steadiness rules: every sample counts."""
    slope, intercept, _ = fit(speed, hr)
    return _extend(slope, intercept, max_hr)


def bands(hr, speed, min_sec=20, cap=60):
    """5 bpm HR bands: (weight, mean speed, mean HR), bands with >= min_sec samples."""
    idx = ((np.asarray(hr) - BIN_MIN_BPM) // BIN_BPM).astype(int)
    out = []
    for b in np.unique(idx):
        sel = idx == b
        n = int(sel.sum())
        if n >= min_sec:
            out.append((min(n, cap), float(np.mean(speed[sel])), float(np.mean(hr[sel]))))
    return np.array(out).reshape(-1, 3)


def band_fit(hr, speed, max_hr, min_bands=3):
    """Weighted fit through 5 bpm band averages."""
    b = bands(np.asarray(hr), np.asarray(speed))
    if len(b) < min_bands:
        return np.nan
    slope, intercept, _ = fit(b[:, 1], b[:, 2], b[:, 0])
    return _extend(slope, intercept, max_hr)


def lag(speed, tau):
    """First-order lag of speed (time constant tau s), as HR responds to it."""
    out = np.empty(len(speed))
    a, y = 1.0 - np.exp(-1.0 / tau), speed[0]
    for i, v in enumerate(speed):
        y += a * (v - y)
        out[i] = y
    return out


def anchored(hr, speed, max_hr, intercept):
    """Line forced through HR = intercept at speed 0; only the slope is fitted."""
    hr, speed = np.asarray(hr, float), np.asarray(speed, float)
    slope = np.sum(speed * (hr - intercept)) / np.sum(speed * speed)
    return _extend(slope, intercept, max_hr)
