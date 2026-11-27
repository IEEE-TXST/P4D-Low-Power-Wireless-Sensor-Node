# P4-D: Concepts and Hardware

*Part of the P4-D manual split. See `P4D_1_Start_Here.md` for the full file list and how to use this manual.*

---

## 1. New Concepts You Need Before Starting

Builds on P1 (UART, I2C, ADC, all reused directly), P2 (state-machine thinking, specifically the sleep/wake cycle this project's main loop is built around), and P3 (DMA/ADC concepts, though see Section 12 for why this specific project doesn't actually use DMA). Everything below is new to P4-D.

#### Power mode hierarchy

This chip has more than just "on" and "off." From most to least power-hungry: **RUN** (everything on, full speed), **WAIT** (core clock stopped, peripherals and bus clock still running, wakes instantly on any enabled interrupt), **STOP**/**VLPS** (Very Low Power Stop: system clocks stopped too, deeper savings, still wakes directly through an interrupt from certain peripherals), and deeper still, **LLS** (Low Leakage Stop) and **VLLS** (Very Low Leakage Stop, with sub-variants).

Each step down cuts current draw further and adds restrictions on what can wake the chip back up.

#### PMC and SMC

Two separate on-chip modules that together control power mode transitions: the **SMC** (System Mode Controller) is what you actually command to enter a given mode (`SMC_SetPowerModeLls()`, for instance) and reports which mode is currently active.

The **PMC** (Power Management Controller) governs the voltage regulator and related analog behavior underneath that mode, mostly automatic once you've picked a target mode, but with a couple of housekeeping calls (`SMC_PreEnterStopModes()`/`SMC_PostExitStopModes()`) needed around any Stop-family transition.

#### LLWU (Low Leakage Wakeup Unit)

In WAIT, STOP, or VLPS, the NVIC itself stays capable of noticing a peripheral's interrupt directly, so waking up works the same way it always has. In LLS and VLLS, that's no longer true: enough of the chip is powered down or clock-gated that the NVIC alone can't reliably catch a wakeup event.

LLWU is a small, always-on peripheral built specifically to watch a configurable set of wakeup sources (certain pins, certain internal modules like the LPTMR) while everything else is asleep, and to bring the core back up when one of them fires. This is why this project, sleeping in LLS specifically, needs LLWU, while a project only going as deep as STOP or VLPS wouldn't.

#### LPTMR (Low Power Timer)

An ordinary hardware timer with one crucial extra property: it can be clocked from a source that keeps running even in the deepest sleep modes (this project uses LPO, a fixed 1 kHz internal oscillator, specifically because it survives LLS, unlike the main system clock, which is what gets stopped in the first place).

This is what makes "wake up automatically after N seconds" possible at all in a mode where the normal clock tree is off.

#### Low-power state machine

The actual application logic this project builds: a repeating **wake, read, transmit, sleep** cycle. This is the same "hardware event happens, do a bounded amount of work, then wait for the next event" shape P2's scheduler used for CPU time.

Here, the resource being managed isn't CPU time shared between tasks, it's total energy, and "sleep" isn't just idling, it's an active, deliberate transition to a specific low-power mode.

#### HC-05 Bluetooth module and AT mode

A small, extremely common (and inexpensive) external module that implements the Bluetooth radio and protocol stack entirely on its own, presenting a simple UART interface to whatever microcontroller it's attached to: bytes sent to it over UART go out over Bluetooth to a paired device, and bytes received over Bluetooth come back out over UART.

Before it can be used this way, it needs to be configured once, in a special **AT mode**, where instead of transparently relaying bytes it accepts text commands (`AT+NAME=...`, `AT+UART=...`) to set its own Bluetooth name, baud rate, and other settings.

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

---

**Next:** `P4D_3_Setup_and_Walkthrough.md` for the hands-on session steps.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
