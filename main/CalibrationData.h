#pragma once
// UNCALIBRATED DEFAULT: replace via python3 scripts/build_calibration.py measurements.csv
inline constexpr bool kCalibrationVerified = false;
inline constexpr gauge::CalibrationPoint kCalibrationPoints[] = {
  // Dummy non-operational sentinel: all conversions blocked until verified.
  {0.0f, 0.0f},
};
