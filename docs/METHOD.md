# How the estimate works

## Estimating VO2 max from a run

Heart rate rises roughly linearly with running speed, up to max heart rate. The app fits that line from a normal, submaximal run and extends it to your max heart rate.

1. **Lag speed.** Heart rate follows a change in pace with a delay, so speed passes through a 40 s first-order lag before it is paired with heart rate. Uphill counts as faster: speed is raised 4.5 % per 1 % of grade (the ACSM grade term), using grade smoothed over 30 s.
2. **Keep usable seconds.** After the first 90 s, a second counts if heart rate is trusted and above 60 bpm, lagged speed is at least 1.8 m/s, and the smoothed grade is within ±8 %. No steady pace is needed.
3. **Group into heart-rate bands.** Seconds go into 5 bpm bands; bands with at least 20 s are used, each as one point (mean speed, mean heart rate) weighted by its time, at most 60 s. Long easy running therefore can't outweigh a short hard effort.
4. **Fit and extend.** A weighted least-squares line HR = a + b·speed through the band points, solved for the speed at your max heart rate (from settings, at least 120 bpm).
5. **Convert to VO2.** ACSM running equation: VO2 = 3.5 + 0.2·v, with v in m/min.

### Confidence

| Level  | HR range (of max HR) | HR bands | Running used |
|--------|----------------------|----------|--------------|
| Low    | ≥ 15 %               | ≥ 5      | ≥ 10 min     |
| Medium | ≥ 20 %               | ≥ 7      | ≥ 20 min     |
| High   | ≥ 25 %               | ≥ 9      | ≥ 25 min     |

The range is between the lowest and highest band means. Runs below Low give no result, and so does a line whose standard error exceeds 4 ml/kg/min.

Confidence says how well the run pins down the line, not how close the result is to a lab value. On lab treadmill data (data_analysis/RESULTS.md) the uncalibrated error was about ±6 ml/kg/min whatever the confidence; a lab result narrows that for one person.

The run's GUIDE screen shows progress live and suggests the heart rate to hold next: faster first (up to 90 % of max HR), then easier, until the range reaches 20 %.

Results recorded before version 0.1.4 used a different method. They are shown grey and are not used for calibration.

### Assumptions

- Max heart rate is correct. This is the largest source of error: 5 bpm off shifts the result by about 1.5–2 ml/kg/min.
- Grade is corrected with the ACSM term, but wind and surface are not.
- Heart rate follows speed with a single 40 s lag. Slower components (drift on long or hard runs) are not modelled.
- Average running economy. Efficient runners are underestimated, inefficient ones overestimated. Calibration corrects for this.

## Calibrating with lab results

If you record a lab-measured VO2 max, the app corrects later and earlier estimates against it.

1. **Pair.** Each lab result is paired with runs within ±6 weeks that reached at least Medium confidence, recorded with the current method.
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
