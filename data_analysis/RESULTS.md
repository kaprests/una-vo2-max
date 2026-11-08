# Málaga comparison: results

Reproduce: `mise run analysis-data`, then
`uv run --project data_analysis python data_analysis/compare.py`. The numbers below
are from the dataset fetched on 8 Nov 2026 (SHA256 checked).

## Data used

- 548 adult ramp tests from 465 people. Steps of 0.1 km/h, lab VO2 max is the highest
  30 s VO2 divided by weight, true max HR is the highest 5 s HR.
- **Excluded:**
  - **The 322 step tests.** Even measured peak speed barely predicts their lab VO2 max
    (r 0.19 for adults, against 0.72 for the ramp tests). Most likely an undocumented
    incline, which no speed-based method can handle.
  - **Under-18s.**

## Results (true max HR used)

**What the columns mean:**
- **Bias:** the method's average error as it stands.
- **RMSE / r:** error and correlation after one population calibration
  (`lab ≈ a + b·estimate`), cross-validated by person.

**Baseline:** age, sex and BMI alone reach RMSE 6.8, r 0.57.

| Method | All data: bias | All data: RMSE / r | Up to 80 % of max HR: bias | Up to 80 % of max HR: RMSE / r |
|---|---|---|---|---|
| App today (with steadiness rules) | +11.4 | 6.5 / 0.39 | 17 of 494 estimated | — |
| Per-second fit | +8.8 | 6.4 / 0.62 | +4.2 | 7.0 / 0.45 |
| Band averages | +8.4 | 6.2 / 0.65 | +3.9 | 6.9 / 0.46 |
| Band averages, lagged speed | +6.0 | 6.2 / 0.66 | +0.3 | 6.8 / 0.48 |
| Anchored, resting HR 60 | +5.5 | 6.4 / 0.63 | +5.1 | 7.1 / 0.44 |

Adding age, sex and BMI to a method gets RMSE down to 5.9–6.4 and r up to 0.57–0.70.

## Findings

1. **HR–speed alone barely beats a demographic guess here.**
   - With a full test it ranks people a little better than age, sex and BMI (r 0.66
     against 0.57).
   - With easy-run data, at or below 80 % of max HR, it ranks them worse (r 0.48 against
     0.50).
   - Combining both is consistently best.
2. **The current app method is the weakest candidate.** Its steadiness rules discard
   ramp data: it gave an estimate for 397 of 547 full tests and 17 of 494 easy-only ones.
   Its bias is the largest, +11.4.
3. **A large positive bias is common to all methods.**
   - ACSM over-predicts treadmill VO2. Measured at 10 km/h it is 32.6 against 36.8, and
     at 12 km/h 37.0 against 43.5 (adult ramp tests).
   - On a ramp, HR lags speed, which also pushes the estimate up.
   - A population calibration or a per-user lab calibration removes a constant bias.
     Lagging speed by about 40 s removes much of it on its own.
4. **Band averages on lagged speed are the most accurate HR–speed method.** They give the
   lowest bias, the best ranking and no resting HR is needed. The resting-HR anchor is no
   better, which agrees with the preference not to depend on it.
5. **The lab reference is noisy.** Between a person's consecutive tests, lab VO2 max
   changes with an SD of 6.9 ml/kg/min. Peak speed changes with an SD of only 1 km/h, and
   the two changes correlate weakly (r 0.26). So part of every error above is noise in
   the reference. The rankings are a pessimistic bound, and the size of a person's change
   over time can't be validated here.

## Update for app 0.1.4

- **The app now uses the band-averages-on-lagged-speed method** ("band_lag" above). Its
  speed starts from zero rather than from the first sample, and it adds confidence
  gates.
- **`compare.py` now runs the 0.1.4 estimator in its "app" row.** The table above shows
  the method before 0.1.4 and can be reproduced from commit `37fdd89`.
- **The new gates need at least 600 s of usable data.** That excludes most of these lab
  ramps, which last about 9 minutes before reaching 95 % of max HR: 516 of 548 tests are
  "too short".
  - That's intended: the gates are sized for outdoor runs of 30 minutes or more.
  - Its accuracy here is the band_lag row.

## Limits

- **Treadmill ramps, not outdoor steady running:** no GPS noise, no hills, chest-strap
  quality HR, and a fitter-than-average lab population.
- **True max HR is used.** In the app, max HR error adds roughly 2.5 ml/kg/min per 5 bpm.
- **One fixed choice of lag, band size and calibration.** Nothing was tuned per method.

## Implications for the app

- Replace the per-second fit with band averages on lagged speed, and drop most of the
  steadiness gating, which throws data away for no accuracy gain.
- Expect accuracy of about ±6–7 ml/kg/min against lab values without calibration.
  Communicate the estimate as a trend indicator; a lab result is what makes it accurate
  for one person.
- Consider blending in age, sex and BMI as a prior. It was the best combination here, but
  it needs new inputs, which the companion app's profile may already provide.
