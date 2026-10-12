# How the estimate works

## Estimating VO2 max from a run

Heart rate rises roughly linearly with running speed, up to max heart rate. The app fits that line from a normal, submaximal run and extends it to your max heart rate.

1. **Keep steady running.** A second counts only if speed is 1.8–6.5 m/s, grade is under 3 % (when known), and heart rate is trusted and above 60 bpm. Speed must be steady over a 30 s window, and the first 60 s after a pace change are skipped while heart rate catches up. Gaps of up to 5 s are bridged.
2. **Fit the line.** A least-squares fit of HR = a + b·speed over the kept seconds.
3. **Extend to max heart rate.** Solve for the speed at your max heart rate (from settings, at least 120 bpm).
4. **Convert to VO2.** ACSM running equation: VO2 = 3.5 + 0.2·v, with v in m/min.

### Confidence

| Level  | Steady time | HR range (of max HR) | Fit R² |
|--------|-------------|----------------------|--------|
| Low    | ≥ 5 min     | ≥ 10 %               | ≥ 0.3  |
| Medium | ≥ 10 min    | ≥ 15 %               | ≥ 0.6  |
| High   | ≥ 20 min    | ≥ 20 %               | ≥ 0.8  |

Runs below Low give no result. A wider heart rate range means less extrapolation.

### Assumptions

- Max heart rate is correct. This is the largest source of error: 5 bpm off shifts the result by about 2–3 ml/kg/min for a typical runner.
- Running on flat, firm ground. The ACSM equation doesn't account for hills, wind or surface.
- Average running economy. Efficient runners are underestimated, inefficient ones overestimated. Calibration corrects for this.

## Calibrating with lab results

If you record a lab-measured VO2 max, the app corrects later and earlier estimates against it.

1. **Pair.** Each lab result is paired with runs within ±6 weeks that reached at least Medium confidence.
2. **Fit one parameter.** Per run, compute the parameter that would make the estimate match the lab value, then take the lower median over the paired runs. The median keeps one odd run from dominating.
3. **Bound.** Parameters outside 0.8–1.25 of the default are rejected as likely errors.
4. **Interpolate over time.** Between lab results the parameter is interpolated linearly by date. Before the first and after the last it stays constant.

The parameter is one of two options (set at build time):

- **Factor** (default): scales the estimate. calibrated = factor · raw.
- **Running cost:** replaces 0.2 in the ACSM equation, i.e. your personal oxygen cost per metre (allowed range 0.16–0.25).

History stores raw estimates, and calibration is applied when shown. Adding or editing a lab result therefore updates all past values.

To start over from a "clean slate", use **Reset calibration** in Settings.
This is something one may want to do e.g. after a large correction of max heart rate. 
Earlier lab results stay visible (grey) but no longer count towards the calibration factor.
Results are in this case uncalibrated until you add a new lab test.
Re-entering old lab results counts as adding a new test.

## Related work

Firstbeat's method (used in Garmin and other watches) uses the same heart rate–speed principle on free-form runs. It adds heart rate variability, personal details and more careful data selection, and isn't fully published. Their white paper reports about 5 % mean error against lab tests, notably worse when max heart rate is estimated rather than measured. Those numbers are theirs, not this app's.

## References

- American College of Sports Medicine. *ACSM's Guidelines for Exercise Testing and Prescription*. Wolters Kluwer. (Metabolic equations.)
- Firstbeat Technologies. *Automated Fitness Level (VO2max) Estimation with Heart Rate and Speed Data*. White paper, 2014, updated 2017. https://www.firstbeat.com/wp-content/uploads/2017/06/white_paper_VO2max_30.6.2017.pdf
