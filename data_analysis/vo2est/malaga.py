"""Load the Malaga treadmill tests as 1 Hz exercise segments with lab ground truth."""

from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd

DATA = Path(__file__).resolve().parent.parent / "data" / "malaga_vo2_max"
WARMUP_KMH = 5.5  # the warm-up and recovery walks are at about 5 km/h


@dataclass
class Test:
    id_test: str
    person: int
    age: float
    weight: float
    height: float
    female: bool
    t: np.ndarray       # s, 1 Hz, from the start of the exercise segment
    speed: np.ndarray   # m/s
    hr: np.ndarray      # bpm
    vo2max: float       # ml/kg/min, highest 30 s mean VO2 / weight
    max_hr: float       # bpm, highest 5 s mean HR
    ramp: bool          # continuous ramp rather than steps


def _rolling_max(t, x, window):
    """Highest mean of x over any window of `window` seconds (t in s, 1 Hz)."""
    s = pd.Series(x, index=pd.to_timedelta(t, unit="s"))
    return float(s.rolling(f"{window}s").mean().max())


def load(min_minutes=4.0):
    """All usable tests. Drops tests without VO2 or with too short an exercise segment."""
    info = pd.read_csv(DATA / "subject-info.csv").set_index("ID_test")
    measures = pd.read_csv(DATA / "test_measure.csv")
    tests = []
    for id_test, m in measures.groupby("ID_test", sort=False):
        if m.VO2.isna().all():
            continue
        m = m.sort_values("time")
        peak = m.Speed.idxmax()
        ex = m.loc[:peak]
        ex = ex[ex.Speed > WARMUP_KMH]
        if ex.empty or ex.time.iloc[-1] - ex.time.iloc[0] < min_minutes * 60:
            continue
        t0, t1 = ex.time.iloc[0], ex.time.iloc[-1]
        grid = np.arange(t0, t1 + 1)
        hr = ex.dropna(subset=["HR"])
        vo2 = m.dropna(subset=["VO2"])
        if len(hr) < 10:
            continue
        row = info.loc[id_test]
        tests.append(Test(
            id_test=id_test,
            person=int(row.ID),
            age=float(row.Age),
            weight=float(row.Weight),
            height=float(row.Height),
            female=bool(row.Sex == 1),
            t=grid - t0,
            speed=np.interp(grid, ex.time, ex.Speed) / 3.6,
            hr=np.interp(grid, hr.time, hr.HR),
            vo2max=_rolling_max(vo2.time.to_numpy(), vo2.VO2.to_numpy(), 30) / row.Weight,
            max_hr=_rolling_max(m.dropna(subset=["HR"]).time.to_numpy(), m.dropna(subset=["HR"]).HR.to_numpy(), 5),
            ramp=ex.Speed.diff()[lambda d: d > 0].median() < 0.3,  # 0.1 km/h ramp vs 0.5-1 km/h steps
        ))
    return tests
