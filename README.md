# P4-D: Low-Power Wireless Sensor Node

*Stop-mode power management, Bluetooth data transmission, live PC dashboard.*

IEEE TXST Student Branch, Fall 2026 FRDM-KL26Z project series. Capstone option, 2 independent groups.

**Platform:** NXP FRDM-KL26Z (ARM Cortex-M0+, 48 MHz, 128 KB Flash) with an HC-05 Bluetooth module (roughly $6, shared, pre-configured before build day). Prerequisite: P1 (UART, I2C, ADC), P2 (scheduler, sleep/wake state machine), P3 (DMA ADC) complete.

## What this project is

Members build a wireless sensor node that wakes from ARM LLS (Low Leakage Stop) on a periodic LPTMR timer, reads the accelerometer and a light sensor, transmits a data packet over UART to an HC-05 Bluetooth module, then returns to sleep. A Python script on a paired laptop receives the data over Bluetooth and plots a live dashboard. Current consumption visibly drops between transmissions. This is the architecture of every battery-operated IoT sensor, from industrial monitors to medical wearables, and it integrates P2's scheduler concepts (as a sleep/wake state machine) with P3's ADC pipeline ideas.

## Exit criteria

Board enters LLS stop mode between transmissions. LPTMR wakes the board every 5 seconds. Accelerometer XYZ and light data transmit over HC-05 Bluetooth to a paired laptop. Python dashboard displays live sensor readings. Current draw is visibly lower during stop mode.

## Repo layout

- `P4D_1_Start_Here.md` through `P4D_4_Reference.md`: the Project Manual, split into four files.
- `demo_code/01_stop_mode_lptmr_wakeup/`: proves the sleep/wake cycle alone, no sensors or Bluetooth yet. Wakes every 5 seconds on an LPTMR/LLWU event, blinks the LED, prints a wake counter, goes back to sleep.
- `demo_code/02_full_sensor_node_with_bluetooth/`: the full node. Every 5 seconds: wake, read the accelerometer and light sensor, transmit one CSV line over UART1 to the HC-05, sleep again.
  - `python/dashboard.py`: live matplotlib dashboard receiving data over the paired Bluetooth serial port.
- `demo_code/_reference_drivers/`: vendored NXP SDK driver source, reference only.

## Getting started

Read `P4D_1_Start_Here.md` first, then `P4D_3_Setup_and_Walkthrough.md`. Get Session 1's sleep/wake cycle solid, and measure the current drop yourself on the board's current-measurement jumper, before adding sensors or the HC-05.

## Resume line

> Built low-power wireless sensor node on ARM Cortex-M0+ using stop-mode power management, LPTMR wakeup, and HC-05 Bluetooth; live Python sensor dashboard displays data wirelessly with measurable current reduction between transmissions.
