# TXST IEEE Student Branch: FRDM-KL26Z Project Series
## Capstone Manual, P4-D: Low-Power Wireless Sensor Node

**Document status:** DRAFT v0.1, for project-leader review and bench testing before member use
**Track:** Embedded Systems | **Difficulty:** Hard | **Sessions:** 1 (WS8, Nov 5, 2026), plus independent build before the Nov 19 demo
**Groups:** 2 groups of 3, working independently
**Companion documents:** P0's manual (`P0_Board_Orientation_and_Toolchain_Setup/`, start at `P0_1_Start_Here.md`) through `P3_Project_Manual.md` (read all four first), *TXST IEEE FRDM-KL26Z Project Specification*
**Hardware:** FRDM-KL26Z plus an HC-05 Bluetooth module (roughly $6, shared per group; pre-configure before WS8, Section 9)
**Demo code:** see `demo_code/` in this project's folder

---

## How to Use This Manual

Same rule as every prior manual: a reference, not required reading. Build the sleep/wake cycle yourself first; open `demo_code/` only when stuck. This project's failure modes are unusually quiet, a stop-mode misconfiguration or a mismatched HC-05 baud rate doesn't produce an error message, it produces silence, so understanding each piece before combining them matters more here than in most projects.

Project leaders: bench-test both `demo_code/01_stop_mode_lptmr_wakeup/` and `demo_code/02_full_sensor_node_with_bluetooth/` on real hardware, with a real HC-05 module, before WS8. This project has no substitute for that bench test the way some earlier projects did: current draw during stop mode, LPTMR wake timing, and Bluetooth pairing all depend on real electrical and RF behavior that no amount of code review or compilation catches.

**A note on accuracy:** this project's power-management sequence is adapted from NXP's own verified `demo_apps/power_manager` example, which exercises every power mode this chip supports; this project uses only the specific LLS-plus-LPTMR-plus-LLWU path from it. Confirming which of two possible `SMC_SetPowerModeLls()` function signatures actually applies to this chip required checking the chip's own compile-time feature flags directly, not assuming, since guessing wrong would have been a build error at best and a subtly wrong power sequence at worst. Section 20 has the full verification trail. The HC-05 module itself is external, third-party hardware with real variation across cheap clone boards; Section 9 is explicit about what's verified, standard behavior versus what a group needs to confirm against their own specific module.

---

## 0. Why This Session Exists

Every prior project assumed the board stays powered and awake the whole time. Almost nothing that actually ships as a battery-powered product can afford that: a real wireless sensor node spends the overwhelming majority of its life asleep, waking only briefly to take a reading and report it, precisely because current drawn while asleep is current not drawn from a battery that has to last months or years. This project is the one place in the series where "does it work" and "does it work within a power budget" are both real, simultaneous requirements, and it deliberately combines P2's sleep/wake state-machine thinking with P1's sensor code to build something that looks and behaves like an actual commercial IoT device, not a simplified classroom version of one.

## 1. New Concepts You Need Before Starting

Builds on P1 (UART, I2C, ADC, all reused directly), P2 (state-machine thinking, specifically the sleep/wake cycle this project's main loop is built around), and P3 (DMA/ADC concepts, though see Section 12 for why this specific project doesn't actually use DMA). Everything below is new to P4-D.

**Power mode hierarchy.** This chip has more than just "on" and "off." From most to least power-hungry: **RUN** (everything on, full speed), **WAIT** (core clock stopped, peripherals and bus clock still running, wakes instantly on any enabled interrupt), **STOP**/**VLPS** (Very Low Power Stop: system clocks stopped too, deeper savings, still wakes directly through an interrupt from certain peripherals), and deeper still, **LLS** (Low Leakage Stop) and **VLLS** (Very Low Leakage Stop, with sub-variants). Each step down cuts current draw further and adds restrictions on what can wake the chip back up.

**PMC and SMC.** Two separate on-chip modules that together control power mode transitions: the **SMC** (System Mode Controller) is what you actually command to enter a given mode (`SMC_SetPowerModeLls()`, for instance) and reports which mode is currently active; the **PMC** (Power Management Controller) governs the voltage regulator and related analog behavior underneath that mode, mostly automatic once you've picked a target mode, but with a couple of housekeeping calls (`SMC_PreEnterStopModes()`/`SMC_PostExitStopModes()`) needed around any Stop-family transition.

**LLWU (Low Leakage Wakeup Unit).** In WAIT, STOP, or VLPS, the NVIC itself stays capable of noticing a peripheral's interrupt directly, so waking up works the same way it always has. In LLS and VLLS, that's no longer true: enough of the chip is powered down or clock-gated that the NVIC alone can't reliably catch a wakeup event. LLWU is a small, always-on peripheral built specifically to watch a configurable set of wakeup sources (certain pins, certain internal modules like the LPTMR) while everything else is asleep, and to bring the core back up when one of them fires. This is why this project, sleeping in LLS specifically, needs LLWU, while a project only going as deep as STOP or VLPS wouldn't.

**LPTMR (Low Power Timer).** An ordinary hardware timer with one crucial extra property: it can be clocked from a source that keeps running even in the deepest sleep modes (this project uses LPO, a fixed 1 kHz internal oscillator, specifically because it survives LLS, unlike the main system clock, which is what gets stopped in the first place). This is what makes "wake up automatically after N seconds" possible at all in a mode where the normal clock tree is off.

**Low-power state machine.** The actual application logic this project builds: a repeating **wake, read, transmit, sleep** cycle. This is the same "hardware event happens, do a bounded amount of work, then wait for the next event" shape P2's scheduler used for CPU time; here, the resource being managed isn't CPU time shared between tasks, it's total energy, and "sleep" isn't just idling, it's an active, deliberate transition to a specific low-power mode.

**HC-05 Bluetooth module and AT mode.** A small, extremely common (and inexpensive) external module that implements the Bluetooth radio and protocol stack entirely on its own, presenting a simple UART interface to whatever microcontroller it's attached to: bytes sent to it over UART go out over Bluetooth to a paired device, and bytes received over Bluetooth come back out over UART. Before it can be used this way, it needs to be configured once, in a special **AT mode**, where instead of transparently relaying bytes it accepts text commands (`AT+NAME=...`, `AT+UART=...`) to set its own Bluetooth name, baud rate, and other settings.

## 2. Purpose

By the end of this project, every group has: a board that spends nearly all of its time in LLS, waking only every 5 seconds; a working LPTMR-plus-LLWU wakeup sequence with a measurable current drop confirmed on real hardware; an HC-05 module configured and paired with a laptop; a wake-read-transmit-sleep cycle sending live accelerometer and light readings over Bluetooth; and a Python dashboard displaying that data live, demonstrated at the Nov 19 showcase.

## 3. Prerequisites

P1 (UART, I2C, ADC), P2 (the sleep/wake state-machine thinking this project's main loop directly reuses), and P3 all complete. This project reuses P1's exact, already-verified accelerometer register sequence and ADC read pattern without re-deriving either; if those feel shaky, revisit `P1_Project_Manual.md` first.

## 4. Additional Toolchain for This Project

Everything from prior projects still applies. New for P4-D: an **HC-05 Bluetooth module** (shared per group, roughly $6; pre-configure it, Section 9, before WS8 so session time isn't spent on AT-mode setup), and **Python 3** with **pyserial** and **matplotlib** for the dashboard (`pip3 install pyserial matplotlib`). A basic multimeter or the board's built-in current-measurement point (Section 8) is useful, though not strictly required, for confirming the current drop the exit criteria asks for.

## 5. Hardware and Register Reference

| Fact | Value | Verified against |
|---|---|---|
| Target power mode | LLS (Low Leakage Stop) | Chosen deliberately: deep enough to require LLWU (matching the project's stated concepts), shallow enough that SRAM and register state survive and execution resumes in place rather than through a reset vector, unlike VLLS |
| `SMC_SetPowerModeLls()` signature on this chip | `status_t SMC_SetPowerModeLls(SMC_Type *base)`, no config struct | `MKL26Z4_features.h`: `FSL_FEATURE_SMC_HAS_LLS_SUBMODE` and `FSL_FEATURE_SMC_HAS_LPOPO` are both `0` on this part, which selects the simpler of `fsl_smc.h`'s two declared overloads |
| Power mode protection | `SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll)`, once, before entering any non-RUN mode | `demo_apps/power_manager` |
| LPTMR clock source for a mode-surviving wakeup | `kLPTMR_PrescalerClock_1` = LPO, a fixed 1 kHz clock (`LPO_CLK_FREQ` = 1000 in `MKL26Z4_features.h`) | `demo_apps/power_manager`'s own LPTMR config, cross-checked against its `LPTMR_SetTimerPeriod(LPTMR0, (1000 * seconds) - 1)` arithmetic, which is only correct if the tick rate is exactly 1 kHz |
| LPTMR's LLWU internal module index | `0` (`LLWU_M0IF`) | `demo_apps/power_manager` |
| Only LLS/VLLS actually need LLWU set up | Confirmed directly in source: `demo_apps/power_manager` only calls `LLWU_EnableInternalModuleInterruptWakup()` when the target mode is none of Wait/VLPW/VLPS/Stop | `demo_apps/power_manager`, `APP_SetWakeupConfig()` |
| HC-05 UART pins | PTE0 = UART1_TX, PTE1 = UART1_RX, both `kPORT_MuxAlt3` | Reused verbatim from the pin mapping P1's UART1 exploration verified (`driver_examples/uart/interrupt`) |
| Light-sensor stand-in | ADC0_SE23, PTE30 | Same channel P1 and P3 used; see the callout in Section 11 |

## Session 1 (WS8, Nov 5): Stop Mode and LPTMR Wakeup

## 6. Configuring the Wakeup Timer and LLWU

`demo_code/01_stop_mode_lptmr_wakeup/main.c`'s `InitLptmrAndLlwu()`:

```c
LPTMR_GetDefaultConfig(&lptmrConfig);
lptmrConfig.prescalerClockSource = kLPTMR_PrescalerClock_1; /* LPO, survives LLS */
lptmrConfig.bypassPrescaler = true;                         /* count LPO ticks directly */
LPTMR_Init(LPTMR0, &lptmrConfig);
LPTMR_SetTimerPeriod(LPTMR0, (1000U * WAKE_PERIOD_SECONDS) - 1U); /* ticks are ms at 1 kHz */

LLWU_EnableInternalModuleInterruptWakup(LLWU, LLWU_LPTMR_MODULE_IDX, true);
NVIC_EnableIRQ(LLWU_IRQn);
```

**Why the clock source choice is load-bearing, not incidental:** the whole point of this timer is counting down while the rest of the chip is asleep. If it were clocked from the normal bus or core clock (the default for most timers), it would stop counting the instant the chip entered LLS, since that's exactly the clock domain LLS shuts down. LPO is specifically one of the small handful of clock sources this chip keeps alive through LLS, which is the only reason this works at all.

## 7. Entering and Leaving LLS

```c
static void EnterLlsUntilWake(void)
{
    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
    LPTMR_StartTimer(LPTMR0);

    SMC_PreEnterStopModes();
    SMC_SetPowerModeLls(SMC); /* blocks here; resumes on the next line once LLWU wakes the core */
    SMC_PostExitStopModes();
}
```

`SMC_SetPowerModeLls()` doesn't return early or asynchronously; it configures the mode, executes the CPU's own low-power wait instruction, and simply doesn't return until something wakes the chip back up, at which point execution continues on the very next line, `SMC_PostExitStopModes()`, as if the function call had just taken a very long time. This is why LLS specifically (as opposed to VLLS) is the right choice for a repeating wake-read-transmit-sleep loop: nothing about the code has to handle "did we just reset, or did we just wake up," because with LLS the answer is always the latter.

**The LLWU ISR** is what actually lets this resume:

```c
void LLWU_IRQHandler(void)
{
    if (LLWU_GetInternalWakeupModuleFlag(LLWU, LLWU_LPTMR_MODULE_IDX))
    {
        LPTMR_DisableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
        LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
        LPTMR_StopTimer(LPTMR0);
    }
}
```

Notice this ISR is genuinely tiny: check which source woke the chip, and clean up that source's own flags so it's ready to be armed again next cycle. All of the real work (reading sensors, transmitting) happens back in `main()`, after `SMC_SetPowerModeLls()` returns, not inside this ISR, the same discipline P1 through P3 established for every other interrupt handler in this series.

## 8. Confirming the Current Drop

The session's deliverable asks you to measure current on the board's current-measurement point during stop mode (commonly labeled J5 on Freedom-family boards; confirm the exact designator against your specific board's silkscreen or schematic, since this can vary by revision) and see it visibly drop compared to RUN mode. A basic multimeter in series, or even just watching a USB power meter if your board is bus-powered, is enough to see the difference; you don't need lab-grade equipment to observe LLS pulling meaningfully less current than RUN, that difference is large enough to be obvious.

**Session 1 deliverable check:** the board enters LLS between wakeups (confirmed by the LED only toggling once every 5 seconds, not continuously), the UART wake counter confirms the 5-second interval accurately, and a measurable current drop is visible during the sleep portion of each cycle.

## Session 2 (Independent, before Nov 19): HC-05 Bring-Up, Sensors, and the Dashboard

## 9. Bringing Up the HC-05

**This section describes standard, well-documented HC-05 behavior, not something specific to this project or verified against NXP's SDK; treat it as a starting point and confirm against your specific module's documentation, since cheap HC-05 breakout boards vary in exactly how you enter AT mode.**

1. Wire the module: VCC and GND to power, the module's TX to the board's UART1 RX (PTE1), the module's RX to the board's UART1 TX (PTE0).
2. Enter AT mode: on most HC-05 breakout boards, this means holding the module's KEY/EN pin high while it powers on (some boards instead have a small button that must be held during power-up). In AT mode, the module defaults to 38400 baud, 8-N-1.
3. Using a serial terminal at 38400 baud, send `AT` and confirm the module replies `OK`. Then send `AT+UART=9600,0,0` to set its data-mode baud rate to 9600 (matching `HC05_BAUD_BPS` in `demo_code/02_full_sensor_node_with_bluetooth/main.c`), and optionally `AT+NAME=YourGroupName` to make it easy to identify when pairing.
4. Power cycle the module without holding KEY/EN this time; it returns to normal data mode at the baud rate you just set.
5. Pair it with your laptop through the normal OS Bluetooth pairing flow (the default PIN on most HC-05 modules is `1234` or `0000`; check your module's documentation if neither works). Once paired, it appears as an ordinary serial port, the same way any other Bluetooth serial device would.

**Do this once, ahead of WS8**, not during the session; AT-mode configuration is a one-time setup step, and the project spec explicitly calls for pre-configuring the module before the session for exactly this reason.

## 10. Reading the Sensors on Wake

`demo_code/02_full_sensor_node_with_bluetooth/main.c` reuses P1's accelerometer code exactly (address probing via `WHO_AM_I`, the standby-then-active `CTRL_REG1` sequence, the 14-bit-to-milli-g conversion) and P1/P3's ADC read pattern for the light-sensor stand-in, both called fresh on every wake, after `EnterLlsUntilWake()` returns:

```c
EnterLlsUntilWake();
GPIO_TogglePinsOutput(BOARD_LED_RED_GPIO, 1U << BOARD_LED_RED_GPIO_PIN);

ReadAccelMg(&xMg, &yMg, &zMg);
lightCounts = ReadLightCounts();

SendPacket(xMg, yMg, zMg, lightCounts);
```

Neither the accelerometer's I2C peripheral nor the ADC needs any special handling for having just come out of LLS; both were left configured before sleeping and simply work again once the core clock is back, no re-initialization needed, since LLS (unlike VLLS) never actually powers down SRAM or loses peripheral configuration state.

## 11. The Ambient Light Sensor Question, Resolved Again

This is the third project in the series to need the substitute P0 first flagged as an open question: no dedicated ambient light sensor IC exists anywhere in the installed SDK for this board, and P1 and P3 both settled on ADC0_SE23 (PTE30) with an external photoresistor or potentiometer as the practical stand-in. This project uses the identical channel for the identical reason; if your group has a photoresistor available, wire it as a voltage divider (photoresistor in series with a fixed resistor, the midpoint to PTE30) so the reading actually responds to light rather than just reading electrical noise on a floating pin. This is now an established convention across P0, P1, P3, and this project, not a one-off decision.

## 12. Why This Project Doesn't Use P3's DMA Pipeline

The original project concept connects this project to P3's DMA ADC work, but for a single reading taken once every 5 seconds, DMA's actual benefit (offloading a continuous, high-rate sampling stream so the CPU never has to poll for it) doesn't apply; there's no stream here to offload, just one conversion per wake. `demo_code/02_full_sensor_node_with_bluetooth/main.c` uses a direct, polled ADC read instead, the same simple pattern P1 established, which is both simpler and, for this specific use case, not actually worse than DMA in any way that matters. If a group wants to use DMA anyway for the sake of practicing it, the added complexity (a TPM-based hardware trigger, which would itself need to keep running through stop mode, adding its own clock-source considerations on top of everything else in Sections 6 to 7) is worth attempting only after the core exit criteria are solid, as a genuine extension, not a requirement.

## 13. Transmitting the Packet

```c
static void SendPacket(int16_t xMg, int16_t yMg, int16_t zMg, uint16_t lightCounts)
{
    char line[48];
    int len = snprintf(line, sizeof(line), "%d,%d,%d,%u\r\n", xMg, yMg, zMg, lightCounts);
    UART_WriteBlocking(HC05_UART, (const uint8_t *)line, (size_t)len);
}
```

A plain CSV text line, not a binary protocol with framing and a checksum like P4-A's; that level of protocol rigor solves a different problem (reliable communication over a link with real transmission errors and independent request/response semantics) than this project has. Here, one board transmits, one laptop listens, once every 5 seconds, and a malformed or lost line just means the dashboard skips a single point, not a meaningful failure. Matching complexity to the actual problem, not defaulting to the most sophisticated tool available, is itself a real engineering judgment worth noticing.

## 14. The Python Dashboard

```bash
pip3 install pyserial matplotlib   # once
python3 demo_code/02_full_sensor_node_with_bluetooth/python/dashboard.py /dev/tty.HC-05-DevB
```

(Use your paired module's actual port name; on Windows this looks like `COM7` or similar.) The script opens the port with a generous 6-second read timeout, since the board only transmits once every 5 seconds and long silent gaps between lines are completely normal, not a sign anything is wrong. It splits each line on commas, silently skips anything that isn't exactly four valid integers, and plots accelerometer X/Y/Z and the light reading side by side, live.

**The CSV-parsing and rolling-window logic was unit-tested in isolation**, feeding it a mix of banner text, malformed lines, and enough valid data points to confirm the rolling window correctly evicts old points once full; the plotting itself, which depends on `matplotlib`, could not be end-to-end tested in the environment this was authored in. Bench-test the full script against a real, paired board before WS8.

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
- `P1_Project_Manual.md` (this repository): the accelerometer and ADC code this project reuses without re-deriving.
- HC-05 documentation from your specific module's vendor: the authoritative source for that module's exact AT-mode entry procedure, which varies across clone boards (Section 9).

## 22. Developer Notes

- **Both firmware stages were compiled successfully** against the real toolchain (12 KB and 21 KB of 128 KB flash), with no warnings introduced by this project's own code. **Determining which `SMC_SetPowerModeLls()` overload applies to this chip required checking `MKL26Z4_features.h` directly** rather than assuming; the header declares two possible signatures depending on feature flags this specific part doesn't set the same way every Kinetis part does. Neither stage has been flashed to physical hardware, and this project's actual exit criteria (current draw, wake timing accuracy, Bluetooth pairing) are not the kind of thing that can be verified any way other than a real bench test; a leader's hands-on check before WS8 matters more here than for almost any other project in this series.
- Section 9's HC-05 instructions are standard, well-documented behavior for this class of module, not something verified against NXP's SDK or this specific board; different clone boards genuinely do vary in AT-mode entry procedure, and this manual says so rather than presenting one specific method as universally guaranteed.
- Section 12's decision not to use P3's DMA pipeline is a deliberate engineering judgment call, not an oversight; if a future revision of this project wants to demonstrate DMA specifically, that's a real, worthwhile extension, but it should be scoped as one, not silently assumed into the base exit criteria.
- This is the last of the four capstones in this series (P4-A through P4-D); like the others, it reuses P1's sensor code and prior projects' verified pin mappings rather than re-deriving anything already established.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
