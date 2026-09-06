# P4-D: Setup and Walkthrough

*Part of the P4-D manual split. See `P4D_1_Start_Here.md` for the full file list and how to use this manual.*

---

## 4. Additional Toolchain for This Project

Everything from prior projects still applies. New for P4-D: an **HC-05 Bluetooth module** (shared per group, roughly $6; pre-configure it, Section 9, before WS8 so session time isn't spent on AT-mode setup), and **Python 3** with **pyserial** and **matplotlib** for the dashboard (`pip3 install pyserial matplotlib`). A basic multimeter or the board's built-in current-measurement point (Section 8) is useful, though not strictly required, for confirming the current drop the exit criteria asks for.

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

---

**Next:** `P4D_4_Reference.md` for code structure, sample output, debugging, and the glossary.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
