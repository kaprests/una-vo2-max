"""Compare VO2 max methods against lab values on the Malaga treadmill tests.

Usage: uv run --project data_analysis python data_analysis/compare.py
Prints error tables for the full tests and for data truncated at a fraction of
max HR (a stand-in for easy-only runs). Adult ramp tests only: the step tests
show almost no relation between peak speed and lab VO2 max (likely an
undocumented incline), so they cannot test a speed-based method.
"""

import numpy as np
import pandas as pd

from vo2est import estimator as E
from vo2est.malaga import load

FRACS = (1.0, 0.85, 0.80, 0.75)  # data up to this fraction of max HR
LAG_TAU = 40.0                   # s, speed lag for "band_lag"
REST_HR = 60.0                   # bpm, assumed resting HR for "anchored"
FOLDS = 5
DEMOGRAPHIC = ["age", "female", "bmi"]
METHODS = ["app", "per_second", "band", "band_lag", "anchored"]


def estimates(test, frac):
    hr, v = test.hr, test.speed
    stop = np.argmax(hr >= frac * test.max_hr) if frac < 1 and (hr >= frac * test.max_hr).any() else len(hr)
    if frac == 1.0 and (hr >= 0.95 * test.max_hr).any():
        stop = np.argmax(hr >= 0.95 * test.max_hr)  # drop the plateau near max
    hr, v = hr[:stop], v[:stop]
    if len(hr) < 120:
        return None
    lagged = E.lag(v, LAG_TAU)
    _, app = E.run(hr, v, test.max_hr)
    return dict(
        app=app.vo2max if app.reason == "OK" else np.nan,
        per_second=E.per_second(hr, v, test.max_hr),
        band=E.band_fit(hr, v, test.max_hr),
        band_lag=E.band_fit(hr, lagged, test.max_hr),
        anchored=E.anchored(hr, lagged, test.max_hr, REST_HR),
        span=hr.max() - hr.min(),
    )


def table(tests):
    rows = []
    for t in tests:
        base = dict(test=t.id_test, person=t.person, lab=t.vo2max, age=t.age, female=float(t.female),
                    bmi=t.weight / (t.height / 100) ** 2)
        for frac in FRACS:
            e = estimates(t, frac)
            if e:
                rows.append(base | e | dict(frac=frac))
    df = pd.DataFrame(rows)
    people = df.person.unique()
    df["fold"] = df.person.map(dict(zip(people, np.random.default_rng(0).integers(0, FOLDS, len(people)))))
    return df


def calibrated(g, cols):
    """lab ~ a + b * cols, cross-validated by person. Returns (RMSE, r)."""
    g = g.dropna(subset=cols)
    X = np.c_[np.ones(len(g)), g[cols].to_numpy(float)]
    y, pred = g.lab.to_numpy(), np.empty(len(g))
    for k in range(FOLDS):
        train = (g.fold != k).to_numpy()
        pred[~train] = X[~train] @ np.linalg.lstsq(X[train], y[train], rcond=None)[0]
    return np.sqrt(np.mean((pred - y) ** 2)), np.corrcoef(pred, y)[0, 1]


def main():
    tests = [t for t in load() if t.ramp and t.age >= 18]
    df = table(tests)
    print(f"{len(tests)} adult ramp tests, {df.person.nunique()} people\n")
    print("raw: bias / RMSE of the method as is.  cal: RMSE / r after lab ~ a + b*estimate (CV by person)")
    for frac, g in df.groupby("frac", sort=False):
        rmse, r = calibrated(g, DEMOGRAPHIC)
        print(f"\ndata up to {frac:.0%} of max HR, {len(g)} tests, median HR span {g.span.median():.0f} bpm")
        print(f"  {'age/sex/BMI':11s} {'':27s} cal {rmse:4.1f} / {r:.2f}")
        for m in METHODS:
            err = (g[m] - g.lab).dropna()
            if len(err) < 30:
                print(f"  {m:11s} {len(err)} estimates")
                continue
            rmse, r = calibrated(g, [m])
            both, rb = calibrated(g, [m] + DEMOGRAPHIC)
            print(f"  {m:11s} n={len(err):3d} bias {err.mean():+5.1f} RMSE {np.sqrt((err ** 2).mean()):4.1f}"
                  f"  cal {rmse:4.1f} / {r:.2f}  + age/sex/BMI {both:4.1f} / {rb:.2f}")


if __name__ == "__main__":
    main()
