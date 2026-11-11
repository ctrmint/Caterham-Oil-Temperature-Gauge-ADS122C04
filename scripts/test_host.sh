#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p /tmp/caterham-oil-gauge-tests
g++ -std=c++17 -Wall -Wextra -Werror -Imain \
  tests/temperature_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp \
  -o /tmp/caterham-oil-gauge-tests/core_tests
/tmp/caterham-oil-gauge-tests/core_tests
# Synthetic sender R/T curve, deliberately never used in live firmware.
g++ -std=c++17 -Wall -Wextra -Werror -DGAUGE_DEMO -Imain \
  tests/demo_temperature_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp \
  -o /tmp/caterham-oil-gauge-tests/demo_tests
/tmp/caterham-oil-gauge-tests/demo_tests
g++ -std=c++17 -Wall -Wextra -Werror -Imain \
  tests/display_text_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp main/core/DisplayText.cpp \
  -o /tmp/caterham-oil-gauge-tests/display_text_tests
/tmp/caterham-oil-gauge-tests/display_text_tests
g++ -std=c++17 -Wall -Wextra -Werror -DGAUGE_DEMO -Imain \
  tests/display_text_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp main/core/DisplayText.cpp \
  -o /tmp/caterham-oil-gauge-tests/display_text_demo_tests
/tmp/caterham-oil-gauge-tests/display_text_demo_tests
g++ -std=c++17 -Wall -Wextra -Werror -Imain \
  tests/sweep_model_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp main/core/DisplayText.cpp main/core/SweepModel.cpp \
  -o /tmp/caterham-oil-gauge-tests/sweep_model_tests
/tmp/caterham-oil-gauge-tests/sweep_model_tests
g++ -std=c++17 -Wall -Wextra -Werror -DGAUGE_DEMO -Imain \
  tests/sweep_model_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp main/core/DisplayText.cpp main/core/SweepModel.cpp \
  -o /tmp/caterham-oil-gauge-tests/sweep_model_demo_tests
/tmp/caterham-oil-gauge-tests/sweep_model_demo_tests
python3 -m unittest discover -s tests -p 'test_*.py' -v
