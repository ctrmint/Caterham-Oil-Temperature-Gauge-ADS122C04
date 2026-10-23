#pragma once
#include <stddef.h>

namespace gauge {
struct CalibrationPoint { float temperature_c; float resistance_ohms; };
// IMPORTANT: Start without any verified Caterham 71167 resistance/temperature points.
// scripts/build_calibration.py generates include/CalibrationData.h from measurements.
// Until then, firmware displays resistance and CALIBRATION REQUIRED, never degrees.
#include "CalibrationData.h"

bool calibrationValid();
bool temperatureFromResistance(float resistance, float &temperature);
float resistanceFromTemperature(float temperature); // only for demo/simulation tests
}
