# P4-D Demo Code

Reference material only, and this project's own failure modes (Section 17 of the manual)
mean it's worth trying yourself first more than most: a stop-mode bug or an HC-05
misconfiguration tends to look like "nothing happens," not a clean error. Get Session 1's
sleep/wake cycle working and understood before opening `02_full_sensor_node_with_bluetooth/`.

- `01_stop_mode_lptmr_wakeup/`: Session 1. LLS (Low Leakage Stop) entered every cycle, LPTMR
  wakes the board every 5 seconds via LLWU, the red LED toggles and a wake counter prints over
  UART0. No sensors, no Bluetooth: this stage is purely about the sleep/wake mechanism itself.
  Adapted from NXP's own verified `demo_apps/power_manager` example (the specific LLS + LPTMR +
  LLWU path from it, not that example's generic multi-mode power-manager framework).

- `02_full_sensor_node_with_bluetooth/`: the full node. Same sleep/wake cycle, plus an
  accelerometer read (I2C0, reused from P1), a light-sensor-stand-in read (ADC0_SE23, reused
  from P1/P3), and a CSV line transmitted over UART1 to an HC-05 Bluetooth module every wake.
  `python/dashboard.py` reads that stream over the paired laptop's Bluetooth serial port and
  plots it live.

**Both firmware stages were compiled and checked, not just written.** They build cleanly
against `SDK_2_2_0_FRDM-KL26Z` (12 KB and 21 KB of the 128 KB flash budget respectively). Every
SMC/LPTMR/LLWU call was verified against NXP's own working `power_manager` demo, including
confirming via the chip's own feature-flag header (`MKL26Z4_features.h`) which
`SMC_SetPowerModeLls()` function signature actually applies to this specific chip (it has two,
selected by a compile-time feature flag). Neither stage has been flashed to physical hardware,
and this project in particular has no substitute for a real bench test: current draw during
stop mode, LPTMR wake timing accuracy, and HC-05 pairing all depend on real electrical and RF
behavior no simulation captures. A project leader must bench-test both stages, with a real
HC-05 module, before WS8.

The Python dashboard's CSV-parsing and rolling-window logic was unit-tested standalone
(feeding it synthetic banner text, malformed lines, and enough valid data to exercise the
window actually evicting old points); its `matplotlib` dependency wasn't installed in the
environment this was authored in, so the live plot itself couldn't be smoke-tested end to end.
Install `pyserial` and `matplotlib` before running it (`pip3 install pyserial matplotlib`).

To build the firmware, see the P0 manual, Section 8, for the general `cmake` / `make` /
`objcopy` flow; each `armgcc/` folder here already has its build scripts patched for this
repository's folder layout.
