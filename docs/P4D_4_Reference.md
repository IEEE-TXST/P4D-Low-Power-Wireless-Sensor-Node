# P4-D: Reference

*Part of the P4-D manual split. See `P4D_1_Start_Here.md` for the full file list and how to use this manual.*

---

## 15. Code Structure Explanation

| File | What It Does |
|---|---|
| `01_stop_mode_lptmr_wakeup/main.c` | `InitLptmrAndLlwu()`, `EnterLlsUntilWake()`, and `LLWU_IRQHandler()` (Sections 6 to 7): the sleep/wake mechanism alone, no sensors. |
| `02_full_sensor_node_with_bluetooth/main.c` | The same sleep/wake mechanism, plus accelerometer and light reads (Sections 10 to 11), `InitHc05Uart()` and `SendPacket()` (Section 13), tying together into the full wake-read-transmit-sleep loop. |
| `python/dashboard.py` | Reads the CSV stream over the paired Bluetooth serial port and plots it live (Section 14). |

## 16. Sample Output

Session 1, over UART0:

```
=== P4-D Session 1 reference: LLS stop mode + LPTMR/LLWU wakeup ===
Waking every 5 seconds. Measure current on the board's current-measurement
jumper (commonly labeled J5 on Freedom boards; confirm against your board's
silkscreen/schematic) to see it drop between wakes.

wake #1, elapsed ~5 s
wake #2, elapsed ~10 s
wake #3, elapsed ~15 s
```

Session 2, over UART0 (debug echo of what was also sent over Bluetooth):

```
sent: -12,34,987,2048
sent: -10,36,991,2050
sent: -9,40,995,2060
```

The dashboard's left panel should show three roughly flat lines (X, Y, Z in milli-g) with a step every 5 seconds, jumping visibly when the board is tilted; the right panel should show the light reading moving when a hand passes over the photoresistor.

## 17. Session Plan (maps to Guideline Section 4.6)

| Meeting | Phase | What Members Do | Deliverable | Slide Focus |
|---|---|---|---|---|
| 1 of 2 | Stop mode and LPTMR wakeup | Configure LPTMR at a 5-second interval (Section 6). Enter LLS (Section 7). Wake via LLWU, blink the LED, return to stop. Measure current during stop (Section 8). Confirm periodic wake via a UART timestamp. | Board entering stop mode between wakeups. UART timestamp confirms the 5-second interval. Measurable current drop during stop. | Terminal screenshot of the wake counter; a photo or reading of the measured current drop; what broke and how it was fixed. |
| Independent (before Nov 19) | HC-05 bring-up, sensor read, Python dashboard | Wire the HC-05 to UART1 and configure AT mode ahead of time (Section 9). Read the accelerometer and light sensor on wake (Sections 10 to 11). Transmit a formatted packet over Bluetooth (Section 13). Write the Python dashboard (Section 14). Demo end to end. | Live Python dashboard receiving sensor data wirelessly. Board waking, reading, transmitting, and sleeping in a loop. | Live demo is the whole slide; be ready to explain why LLS specifically needs LLWU when earlier stop modes don't. |

## 18. Milestones and Success Criteria

| Milestone | Success Criteria | Evidence |
|---|---|---|
| LLS entered correctly | LED toggles only once per wake cycle, not continuously; board is unresponsive to anything but the LPTMR/LLWU wake between cycles | Live demo |
| LPTMR timing accurate (Session 1 exit criteria) | UART wake counter confirms 5-second intervals, not drifting wildly | Terminal screenshot over several minutes |
| Current drop measurable | A real, observable current reduction during the sleep portion of each cycle | Multimeter/power-meter reading or photo |
| HC-05 configured and paired | Module responds to `AT` with `OK` in AT mode; pairs successfully with a laptop | Verbal/visual check by leader |
| Full pipeline (final exit criteria) | Board enters LLS between transmissions; LPTMR wakes it every 5 seconds; accelerometer XYZ and light data transmitted over HC-05 to the paired laptop; Python dashboard displays live readings; current draw visibly lower during stop | Live demo |

## 19. Project-Specific Debugging Reference

| # | Common Problem | Suggested Debugging Steps | Difficulty |
|---|---|---|---|
| 1 | Board never wakes up at all; UART goes silent forever | Confirm `SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll)` was actually called before the first sleep; without it, entering LLS can fail silently or behave unpredictably. | Medium |
| 2 | Board wakes immediately, over and over, far faster than 5 seconds | Check the LPTMR clock source is genuinely `kLPTMR_PrescalerClock_1` with `bypassPrescaler = true`; a wrong clock source produces a wildly wrong tick rate, not an error. | Medium |
| 3 | LED blinks correctly but no UART0 output appears after waking | `BOARD_InitDebugConsole()` and its UART0 configuration survive LLS fine, but confirm nothing in your code accidentally re-initializes pins in a way that changes UART0's mux setting; this shouldn't be necessary after LLS specifically (state is retained), so if it's happening, look for an unintended re-init call. | Medium |
| 4 | Current doesn't visibly drop during stop mode | Confirm you're actually measuring at the board's current-measurement point (Section 8), not just at a USB port upstream of the OpenSDA chip, which draws its own current regardless of the target chip's power mode and will mask the difference entirely. | Easy |
| 5 | HC-05 never responds to `AT` commands | Confirm the module is genuinely in AT mode (KEY/EN held during power-up, or whatever your specific board requires) and that your terminal is at 38400 baud, not the data-mode baud rate; AT mode and data mode use different baud rates by default on most HC-05 firmware. | Medium |
| 6 | HC-05 pairs but the dashboard receives nothing, or garbage | Confirm `HC05_BAUD_BPS` in the firmware matches whatever `AT+UART=...` actually set; a baud mismatch here produces garbled or silent output, not a clean error, exactly the kind of quiet failure this project's difficulty rating warns about. | Hard |
| 7 | Dashboard shows data but the accelerometer values look frozen or wrong | Confirm `InitAccelerometer()` actually found the sensor (its I2C probe can fail silently on a wiring issue); add a temporary UART0 debug print of the found address, `0` means it was never found. | Medium |
| 8 | Everything works when powered from a debugger/IDE session but not standalone | Confirm nothing in the code depends on a debug probe being attached; entering and exiting low-power modes can behave differently with a debugger connected (some debug probes hold the chip in a higher power state), so always confirm standalone, unplugged-from-the-IDE behavior separately. | Hard |

## 20. Glossary

- **RUN / WAIT / STOP / VLPS / LLS / VLLS:** the power mode hierarchy from most to least power-hungry (roughly), each trading capability (what can wake the chip, what state survives) for lower current draw.
- **SMC (System Mode Controller):** the peripheral you command to switch power modes.
- **PMC (Power Management Controller):** the peripheral governing voltage regulator behavior underneath a given power mode.
- **LLWU (Low Leakage Wakeup Unit):** the always-on peripheral that watches for and responds to wakeup events while the core itself may not be capable of noticing them directly, required for LLS and VLLS.
- **LPTMR (Low Power Timer):** a timer that can run from a clock source that survives deep sleep modes, used here as the periodic wakeup source.
- **LPO:** a fixed, low-power internal oscillator (1 kHz on this chip) that stays active through LLS, the clock source this project's LPTMR uses.
- **HC-05:** a common, inexpensive Bluetooth-to-serial module.
- **AT mode:** a configuration mode an HC-05 module enters (usually via a dedicated pin held high at power-up) to accept text commands instead of transparently relaying data.

## 21. References

- `SDK_2_2_0_FRDM-KL26Z/boards/frdmkl26z/demo_apps/power_manager`: the verified reference this project's LLS/LPTMR/LLWU sequence is adapted from.
- `MKL26Z4_features.h` (in the SDK): the chip's own compile-time feature flags, source of the `SMC_SetPowerModeLls()` signature determination in Section 20.
- `fsl_smc.h`, `fsl_llwu.h`, `fsl_lptmr.h` (in the SDK): driver source, ground truth for every function signature in this manual.
- P1's manual (`P1_Sensor_Dashboard/`, this repository): the accelerometer and ADC code this project reuses without re-deriving.
- HC-05 documentation from your specific module's vendor: the authoritative source for that module's exact AT-mode entry procedure, which varies across clone boards (Section 9).

## 22. Developer Notes

- **Both firmware stages were compiled successfully** against the real toolchain (12 KB and 21 KB of 128 KB flash), with no warnings introduced by this project's own code. **Determining which `SMC_SetPowerModeLls()` overload applies to this chip required checking `MKL26Z4_features.h` directly** rather than assuming; the header declares two possible signatures depending on feature flags this specific part doesn't set the same way every Kinetis part does. Neither stage has been flashed to physical hardware, and this project's actual exit criteria (current draw, wake timing accuracy, Bluetooth pairing) are not the kind of thing that can be verified any way other than a real bench test; a leader's hands-on check before WS8 matters more here than for almost any other project in this series.
- Section 9's HC-05 instructions are standard, well-documented behavior for this class of module, not something verified against NXP's SDK or this specific board; different clone boards genuinely do vary in AT-mode entry procedure, and this manual says so rather than presenting one specific method as universally guaranteed.
- Section 12's decision not to use P3's DMA pipeline is a deliberate engineering judgment call, not an oversight; if a future revision of this project wants to demonstrate DMA specifically, that's a real, worthwhile extension, but it should be scoped as one, not silently assumed into the base exit criteria.
- This is the last of the four capstones in this series (P4-A through P4-D); like the others, it reuses P1's sensor code and prior projects' verified pin mappings rather than re-deriving anything already established.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
