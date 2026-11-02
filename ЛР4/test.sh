#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p artifacts
CC=${HOST_CC:-clang}
"$CC" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -ICompass_RTOS_LR4_V2/Core/Inc tests/test_compass.c Compass_RTOS_LR4_V2/Core/Src/compass.c -lm -o artifacts/test_core
./artifacts/test_core
"$CC" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/mocks -ICompass_RTOS_LR4_V2/Core/Inc tests/test_devices.c Compass_RTOS_LR4_V2/Core/Src/devices.c Compass_RTOS_LR4_V2/Core/Src/compass.c -lm -o artifacts/test_devices
./artifacts/test_devices
python_bin=${PYTHON:-../tools/venv/bin/python}
"$python_bin" -m unittest discover -s tests -p 'test_*.py' -v
