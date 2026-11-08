"""Python port of the C++ Vo2MaxEstimator, plus candidate fits for comparison.

`Estimator` mirrors Software/Libs/Sources/Vo2MaxEstimator.cpp and must stay in
line with it (tests/test_estimator.py reuses the C++ test cases). The other
functions are alternatives evaluated in the analysis, not app code.
"""

from dataclasses import dataclass, field
from enum import IntEnum

import numpy as np

MIN_VO2, MAX_VO2 = 20.0, 90.0
MAX_HR_TRUST = 3
BIN_MIN_BPM, BIN_BPM, BINS = 60.0, 5.0, 32
GRADE_COST = 0.045  # ACSM: 4.5 % more effort per 1 % uphill


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
    hr_range_frac: float
    bands: int
    valid_sec: int


@dataclass(frozen=True)
class Config:
    min_speed: float = 1.8
    max_speed: float = 6.5
    max_grade: float = 8.0
    min_hr: float = 60.0
    min_hr_trust: int = 1
    speed_lag: float = 40.0
    grade_lag: float = 30.0
    warmup_sec: int = 90
    min_band_sec: int = 20
    max_band_weight: int = 60
    max_vo2_se: float = 4.0
    min_max_hr: float = 120.0
    low: Level = Level(0.15, 5, 600)
    medium: Level = Level(0.20, 7, 1200)
    high: Level = Level(0.25, 9, 1500)


@dataclass
class Result:
    reason: str = "TOO_SHORT"
    confidence: Confidence = Confidence.NONE
    vo2max: float = 0.0
    valid_sec: int = 0
    hr_range_frac: float = 0.0
    r2: float = 0.0


@dataclass
class Estimator:
    """Sample-by-sample port of Vo2MaxEstimator (1 Hz)."""

    cfg: Config = field(default_factory=Config)

    def __post_init__(self):
        self.speed = 0.0
        self.grade = 0.0
        self.elapsed = 0
        self.valid = 0
        self.sec = np.zeros(BINS, dtype=int)
        self.sum_speed = np.zeros(BINS)
        self.sum_hr = np.zeros(BINS)

    def skip(self, seconds):
        self.speed *= np.exp(-seconds / self.cfg.speed_lag)
        self.grade *= np.exp(-seconds / self.cfg.grade_lag)

    def add(self, hr, speed, trust=MAX_HR_TRUST, grade=0.0, grade_valid=False):
        c = self.cfg
        self.elapsed += 1
        self.grade += (1 - np.exp(-1 / c.grade_lag)) * ((grade if grade_valid else 0.0) - self.grade)
        if speed <= c.max_speed:
            effort = max(speed * (1 + GRADE_COST * self.grade), 0.0)
            self.speed += (1 - np.exp(-1 / c.speed_lag)) * (effort - self.speed)
        hr_ok = hr >= c.min_hr and c.min_hr_trust <= trust <= MAX_HR_TRUST
        if not hr_ok or self.elapsed <= c.warmup_sec or self.speed < c.min_speed or abs(self.grade) > c.max_grade:
            return
        b = min(max(int((hr - BIN_MIN_BPM) / BIN_BPM), 0), BINS - 1)
        self.sec[b] += 1
        self.sum_speed[b] += self.speed
        self.sum_hr[b] += hr
        self.valid += 1

    def points(self):
        """Bands with enough data: rows of (weight, mean speed, mean HR)."""
        sel = self.sec >= self.cfg.min_band_sec
        n = self.sec[sel]
        return np.c_[np.minimum(n, self.cfg.max_band_weight), self.sum_speed[sel] / n, self.sum_hr[sel] / n]

    def result(self, max_hr):
        c, r = self.cfg, Result(valid_sec=self.valid)
        if max_hr < c.min_max_hr:
            r.reason = "NO_MAX_HR"
            return r
        p = self.points()
        r.hr_range_frac = (p[:, 2].max() - p[:, 2].min()) / max_hr if len(p) else 0.0
        if r.valid_sec < c.low.valid_sec:
            return r
        if len(p) < c.low.bands or r.hr_range_frac < c.low.hr_range_frac:
            r.reason = "LOW_HR_RANGE"
            return r
        w, x, y = p.T
        slope, intercept, r.r2 = fit(x, y, w)
        if not slope > 0:
            r.reason = "BAD_SLOPE"
            return r
        speed = (max_hr - intercept) / slope
        vo2 = float(acsm_vo2(speed))
        if not MIN_VO2 <= vo2 <= MAX_VO2:
            r.reason = "OUT_OF_RANGE"
            return r
        mx = np.average(x, weights=w)
        s2 = np.sum(w * (y - intercept - slope * x) ** 2) / (len(p) - 2)
        var = s2 * (1 / w.sum() + (speed - mx) ** 2 / np.sum(w * (x - mx) ** 2)) / slope ** 2
        if 12 * np.sqrt(var) > c.max_vo2_se:
            r.reason = "POOR_FIT"
            return r
        meets = lambda l: r.hr_range_frac >= l.hr_range_frac and len(p) >= l.bands and r.valid_sec >= l.valid_sec
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
