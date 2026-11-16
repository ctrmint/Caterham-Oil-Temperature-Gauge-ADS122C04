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
g++ -std=c++17 -Wall -Wextra -Werror -Imain \
  tests/layout_choice_test.cpp main/core/LayoutChoice.cpp \
  -o /tmp/caterham-oil-gauge-tests/layout_choice_tests
/tmp/caterham-oil-gauge-tests/layout_choice_tests
g++ -std=c++17 -Wall -Wextra -Werror -Imain \
  tests/adc_frame_test.cpp \
  -o /tmp/caterham-oil-gauge-tests/adc_frame_tests
/tmp/caterham-oil-gauge-tests/adc_frame_tests
# Both front-end options simulated against convertSample (-O2: it steps the circuit in 10 us steps).
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Imain \
  tests/front_end_sim_test.cpp main/core/Calibration.cpp main/core/Temperature.cpp \
  -o /tmp/caterham-oil-gauge-tests/front_end_sim_tests
/tmp/caterham-oil-gauge-tests/front_end_sim_tests
python3 -m unittest discover -s tests -p 'test_*.py' -v
