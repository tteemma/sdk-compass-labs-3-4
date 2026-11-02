#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$base/Compass_LR3_V2"
model=${MCU:-STM32F427xx}
backend=${UART_BACKEND:-HAL}
case "$model" in STM32F427xx) cfg=SDK427.cfg;; STM32F407xx) cfg=SDK407.cfg;; *) echo "MCU: STM32F407xx or STM32F427xx" >&2; exit 2;; esac
case "$backend" in HAL|REGISTER) ;; *) echo 'UART_BACKEND: HAL or REGISTER' >&2; exit 2;; esac
make MCU="$model" UART_BACKEND="$backend"
openocd_bin=${OPENOCD:-openocd}
if ! command -v "$openocd_bin" >/dev/null 2>&1; then
  echo 'OpenOCD not found. Install STM32CubeIDE or: brew install open-ocd; set OPENOCD to its executable.' >&2
  exit 1
fi
if [ "${1:-}" = '--check-config' ]; then
  exec "$openocd_bin" -s openocd -f "openocd/$cfg" -c shutdown
fi
if [ "${1:-}" = '--debug' ]; then
  exec "$openocd_bin" -s openocd -f "openocd/$cfg"
fi
exec "$openocd_bin" -s openocd -f "openocd/$cfg" -c "program Debug/Compass_LR3_V2_${backend}_${model}.elf verify reset exit"
