#pragma once
#include <stddef.h>

namespace gauge {
struct CalibrationPoint { float temperature_c; float resistance_ohms; };
// IMPORTANT: Start without any verified Caterham 71167 resistance/temperature points.
// scripts/build_calibration.py generates main/CalibrationData.h from measurements.
// Until then, firmware displays resistance and CALIBRATION REQUIRED, never degrees.
#include "CalibrationData.h"

bool calibrationValid();
bool temperatureFromResistance(float resistance, float &temperature);
float resistanceFromTemperature(float temperature); // only for demo/simulation tests
// Calibrated temperature span; NAN when no valid calibration is installed.
float calibrationMinC();
float calibrationMaxC();
// <0: colder than the table (resistance above the first point), >0: hotter than the
// table (resistance below the last point), 0: inside the table or no calibration.
int calibrationSide(float resistance);
}
