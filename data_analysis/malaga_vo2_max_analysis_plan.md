# Málaga VO2 max dataset: analysis plan

## Dataset

- **Source:** PhysioNet, *Treadmill Maximal Exercise Tests from the Exercise
  Physiology and Human Performance Lab of the University of Malaga*, v1.0.1,
  https://doi.org/10.13026/7ezk-j442
- **Licence:** PhysioNet Contributor Review Health Data License 1.5.0. Open access;
  cite the dataset and the paper (Mongin et al. 2021, *Res Sports Med*,
  doi:10.1080/15438627.2021.1954513), and make no attempt to identify participants.
- **Size:** 992 maximal graded treadmill tests (2008–2018), 857 people aged 10–63,
  amateur and professional athletes, 15 % women.
- **`subject-info.csv`** (one row per test): `ID_test`, `ID`, age, weight, height,
  sex, humidity, temperature.
- **`test_measure.csv`** (575,087 rows, one per breath): `time` (s), `Speed` (km/h),
  `HR` (bpm), `VO2`/`VCO2` (ml/min), `RR`, `VE`, `ID`, `ID_test`.
  - Median test: 580 breaths, about 18 min.
  - 30 tests have no VO2.
- **Protocol:** walking warm-up at 5 km/h, then a continuous ramp or steps of
  0.5–1 km/h until exhaustion, then a walk-back at 5 km/h.
  - HR from a 12-lead ECG, VO2 from a breath-by-breath gas analyser.
  - Treadmill incline is not stated; assume flat.

## Relevance to the app

The data has the same inputs as our estimator (speed and HR) plus ground truth: lab
VO2 max (peak VO2 / weight) and true max HR (peak HR). So the core method can be
tested directly: fit HR against speed below maximum effort, extend the line to max
HR, and convert that speed to VO2 with ACSM.

Limits on what carries over to the app:

- Ramp and step protocols, not steady outdoor runs, so the steadiness filter cannot
  be tested here.
- Treadmill: no hills and no GPS speed noise.
- Chest-strap-quality ECG HR, not the watch's optical wrist HR.
- A lab population, possibly fitter than average.

## Research questions

1. **Baseline accuracy.** With true max HR, how far off is the current method's VO2
   max (bias, mean absolute error, RMSE, limits of agreement)?
2. **Effect of max HR error.** How much worse with max HR from an age formula
   (Tanaka, 208 − 0.7·age; Fox, 220 − age), or with ±5 bpm added to the true max HR?
3. **How much of the test to use.** How does accuracy change when only data up to
   70, 75, 80 and 85 % of max HR (or of peak speed) is used? This mirrors the "easy
   plus moderate pace" runs the app asks for.
4. **Quality thresholds.** Do the minimum HR range and R² in `Config` (Low / Medium /
   High) separate good estimates from bad ones? Which values separate them better?
5. **Speed-to-VO2 conversion.** Does ACSM's 3.5 + 0.2·v hold? Fit
   `VO2max_lab ~ a + b·vMax`, possibly with sex, age or body mass if that helps.
   Cross-validate grouped by person, so no one's tests are in both training and test.
6. **Running speed vs. true VO2 max.** Compare measured peak running speed with lab
   VO2 max, as an upper bound on how well any speed-based method can do.
7. **Bias vs. VO2 max level.** Does the error depend on the lab VO2 max (low values
   overestimated, high ones underestimated)? If so, fit `lab ≈ α + β·estimate` as a
   correction for all users, applied before any per-user lab calibration.
8. **Signs of a poor fit.** Does curvature in the HR–speed relationship (HR
   flattening near maximum) or HR drift predict bad estimates?

## Method

- **Data preparation:**
  - Drop tests without VO2.
  - Find the exercise segment: from the end of the warm-up walk to peak speed; drop
    the 5 km/h warm-up and recovery walks.
  - Resample to 1 Hz to match the app.
  - Lab VO2 max = highest 30 s rolling mean of VO2, divided by weight. True max HR =
    highest 5 s rolling mean of HR.
- **Estimator:** a Python port of the C++ `Vo2MaxEstimator` core (line fit,
  extension to max HR, ACSM conversion, confidence levels), with the steadiness
  filter optional and unused on ramp data.
- **Evaluation:**
  - Bland–Altman plots and error statistics against lab values.
  - Splits by sex, age group, fitness level, and ramp vs. step protocol.
  - Everything grouped by person (`ID`), so repeat tests are not counted as
    independent.
- **Outputs:** suggested `Config` defaults and an optional fitted conversion, carried
  back to the C++ estimator and its unit tests.

## Layout

```
data_analysis/
├── malaga_vo2_max_analysis_plan.md   # this plan
├── vo2est/                           # Python package
│   ├── estimator.py                  # port of Vo2MaxEstimator (fit, extend, ACSM, confidence)
│   └── malaga.py                     # load, segment, resample, ground truth
├── tests/test_estimator.py           # checks against the C++ unit-test cases (e.g. 61.1)
├── malaga_analysis.ipynb             # analysis + report
└── data/malaga_vo2_max/              # downloaded CSVs (gitignored)
```

- **Dependencies** in a separate `[dependency-groups] analysis` in `pyproject.toml`
  (numpy, pandas, scipy, matplotlib, jupyterlab, pytest), so the firmware tooling
  stays lean.
- **Tasks:**
  - `mise run analysis-data`: download the zip and check it against `SHA256SUMS`.
  - `mise run analysis`: start JupyterLab.
  - `mise run analysis-test`: run the Python parity tests.
- **Keeping the port in line with C++:** the parity tests reuse the C++ test
  scenarios and expected values. A change to either side must pass the same cases.

## Out of scope

- Testing the steadiness filter; needs real outdoor data, ideally from the community.
- Wrist-HR noise models.
