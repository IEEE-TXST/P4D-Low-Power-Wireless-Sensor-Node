# P4-D Demo Code

These are **step-by-step code files**, not answer keys. The code is already in each file, but
switched off (commented out). You switch it on one step at a time and do a small exercise. All
the explanation is written inside the file itself.

This project's failure modes (Section 17 of the manual) tend to look like "nothing happens"
rather than a clean error, so go in order and finish demo 01 before starting demo 02.

## How to use a demo

1. Create a new project with the SDK wizard. The P0 manual, Section 9, Step 1, shows exactly how.
2. Open the demo's `main.c`, copy everything, and paste it over your project's main source file.
3. Replace your project's `pin_mux.c` with the `pin_mux.c` in the same demo folder. It sets up the
   LED, I2C and (for demo 02) the Bluetooth serial pins. If you forget this step the program still
   builds and runs, but the LED or sensors do nothing, with no error message.
4. Read the first step in the file, select its grey `//` lines, and press **Cmd + /** (Mac) or
   **Ctrl + /** (Windows) to turn them on. Build, flash, and watch the terminal. Do the
   exercise and move on.

Lines inside `/* ... */` boxes are explanation. Leave them alone.

## What is here

- `01_stop_mode_lptmr_wakeup/main.c`: 6 steps. The board enters LLS (Low Leakage Stop) deep sleep,
  an LPTMR wakes it every 5 seconds via the LLWU, the red LED toggles, and a wake counter prints.
  No sensors and no Bluetooth: this stage is only about the sleep and wake mechanism. Adapted
  from NXP's own verified `demo_apps/power_manager` example (the LLS + LPTMR + LLWU path only).
- `02_full_sensor_node_with_bluetooth/main.c`: 8 steps. The same sleep and wake cycle, plus an
  accelerometer read (I2C0), a light sensor stand-in read (ADC0_SE23, a potentiometer or
  photoresistor on PTE30), and a CSV line sent over UART1 to an HC-05 Bluetooth module on every
  wake. Each reading is also printed to the terminal, so you can check it without Bluetooth.
  - `python/dashboard.py` reads that stream over the paired laptop's Bluetooth serial port and
    plots it live. Install `pyserial` and `matplotlib` first (`pip3 install pyserial matplotlib`).

The HC-05 must be set up before demo 02 can reach your laptop: paired, and set to 9600 baud. See
the manual, Section 9, or ask a project leader.

While the chip is asleep, the debugger may lose its connection. That is normal. Press the reset
button on the board and carry on.

## For project leaders

- The steps have not yet been built and flashed one by one on real hardware. Current draw in
  stop mode, LPTMR wake timing, and HC-05 pairing have no substitute for a real bench test with
  a real HC-05 module. Please test both demos before the session.
- The SMC, LPTMR and LLWU calls were checked against NXP's `power_manager` demo earlier,
  including which `SMC_SetPowerModeLls()` signature applies to this chip.
- The Python dashboard's CSV parsing was unit-tested standalone, but its live plot has not been
  smoke-tested end to end.
