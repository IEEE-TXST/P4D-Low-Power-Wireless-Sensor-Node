# TXST IEEE Student Branch: FRDM-KL26Z Project Series
## Capstone Manual, P4-D: Low-Power Wireless Sensor Node

**Document status:** DRAFT v0.1, for project-leader review and bench testing before member use
**Track:** Embedded Systems | **Difficulty:** Hard | **Sessions:** 1 (WS8, Nov 5, 2026), plus independent build before the Nov 19 demo
**Groups:** 2 groups of 3, working independently
**Companion documents:** P0's manual (`P0_Board_Orientation_and_Toolchain_Setup/`, start at `P0_1_Start_Here.md`) through P3's manual (`P3_DMA_ADC_FIR_Filter/`) (read all four first), *TXST IEEE FRDM-KL26Z Project Specification*
**Hardware:** FRDM-KL26Z plus an HC-05 Bluetooth module (roughly $6, shared per group; pre-configure before WS8, Section 9)
**Demo code:** see `demo_code/` in this project's folder

---

## This Manual Is Split Into 4 Files

Long single files invite procrastination. Read only what you need, when you need it:

1. **`P4D_1_Start_Here.md`** (this file) — how to use this manual, why P4-D exists, purpose, prerequisites.
2. **`P4D_2_Concepts_and_Hardware.md`** — background theory (power mode hierarchy, SMC/PMC, LLWU, LPTMR, HC-05) and the hardware/register reference. Read once if any term below is new to you.
3. **`P4D_3_Setup_and_Walkthrough.md`** — the actual hands-on steps for both sessions. **This is the file you follow during the sessions.**
4. **`P4D_4_Reference.md`** — code structure explanation, sample output, session plan, milestones, debugging table, glossary, references, developer notes. Look things up here when stuck.

Section numbers (0-22) are kept consistent across all 4 files, so "see Section 9" always means the same section no matter which file you're in.

---

## How to Use This Manual

Same rule as every prior manual: a reference, not required reading. Build the sleep/wake cycle yourself first; open `demo_code/` only when stuck. This project's failure modes are unusually quiet, a stop-mode misconfiguration or a mismatched HC-05 baud rate doesn't produce an error message, it produces silence, so understanding each piece before combining them matters more here than in most projects.

Project leaders: bench-test both `demo_code/01_stop_mode_lptmr_wakeup/` and `demo_code/02_full_sensor_node_with_bluetooth/` on real hardware, with a real HC-05 module, before WS8. This project has no substitute for that bench test the way some earlier projects did: current draw during stop mode, LPTMR wake timing, and Bluetooth pairing all depend on real electrical and RF behavior that no amount of code review or compilation catches.

#### A note on accuracy

This project's power-management sequence is adapted from NXP's own verified `demo_apps/power_manager` example, which exercises every power mode this chip supports; this project uses only the specific LLS-plus-LPTMR-plus-LLWU path from it.

Confirming which of two possible `SMC_SetPowerModeLls()` function signatures actually applies to this chip required checking the chip's own compile-time feature flags directly, not assuming, since guessing wrong would have been a build error at best and a subtly wrong power sequence at worst. Section 20 has the full verification trail.

The HC-05 module itself is external, third-party hardware with real variation across cheap clone boards; Section 9 is explicit about what's verified, standard behavior versus what a group needs to confirm against their own specific module.

---

## 0. Why This Session Exists

Every prior project assumed the board stays powered and awake the whole time. Almost nothing that actually ships as a battery-powered product can afford that: a real wireless sensor node spends the overwhelming majority of its life asleep, waking only briefly to take a reading and report it, precisely because current drawn while asleep is current not drawn from a battery that has to last months or years.

This project is the one place in the series where "does it work" and "does it work within a power budget" are both real, simultaneous requirements, and it deliberately combines P2's sleep/wake state-machine thinking with P1's sensor code to build something that looks and behaves like an actual commercial IoT device, not a simplified classroom version of one.

## 2. Purpose

By the end of this project, every group has:

- A board that spends nearly all of its time in LLS, waking only every 5 seconds.
- A working LPTMR-plus-LLWU wakeup sequence with a measurable current drop confirmed on real hardware.
- An HC-05 module configured and paired with a laptop.
- A wake-read-transmit-sleep cycle sending live accelerometer and light readings over Bluetooth.
- A Python dashboard displaying that data live, demonstrated at the Nov 19 showcase.

## 3. Prerequisites

P1 (UART, I2C, ADC), P2 (the sleep/wake state-machine thinking this project's main loop directly reuses), and P3 all complete. This project reuses P1's exact, already-verified accelerometer register sequence and ADC read pattern without re-deriving either; if those feel shaky, revisit P1's manual first.

---

**Next:** `P4D_2_Concepts_and_Hardware.md` for the concepts and hardware reference, or skip straight to `P4D_3_Setup_and_Walkthrough.md` if you're already comfortable with power modes and LLWU.

---
*IEEE Texas State University Student Branch. Connect. Build. Inspire.*
