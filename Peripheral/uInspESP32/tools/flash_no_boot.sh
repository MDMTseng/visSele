#!/usr/bin/env bash
# Flash without a BOOT press.
#
# This board's auto-reset circuit is half wired: DTR->EN resets the chip, but
# RTS->IO0 does nothing, so esptool always finds boot:0x1 (SPI boot) instead of
# 0x3 (download). Verified by holding IO0 for 0.05s, 0.3s and 0.8s -- identical
# every time. No timing setting fixes a missing connection.
#
# So the running firmware puts itself into the ROM bootloader instead
# (RTC_CNTL_FORCE_DOWNLOAD_BOOT), which needs no pin at all. Then flash with
# --before no_reset, because a reset would take it straight back out.
#
# Requires firmware that already has the reboot_bootloader command. If it does
# not answer, fall back to a physical BOOT press once to install it.
set -euo pipefail
PORT="${1:-/dev/cu.usbserial-0001}"
PY=~/.platformio/penv/bin/python
cd "$(dirname "$0")/.."

echo "==> asking the device to enter download mode"
$PY - "$PORT" <<'PYEOF'
import serial, sys, time
port = sys.argv[1]
p = serial.Serial(port, 115200, timeout=1)
p.write(b'{"type":"reboot_bootloader"}\n')
p.flush()
time.sleep(0.6)
print("   device said:", p.read(p.in_waiting or 1)[:120])
p.close()
PYEOF

echo "==> flashing (no reset -- the chip is already waiting)"
pio run -t upload --upload-port "$PORT" \
  --project-option="upload_flags=--before=no_reset" \
  --project-option="upload_resetmethod=no_reset"
