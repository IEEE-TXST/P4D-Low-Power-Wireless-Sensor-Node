/*
 * P4-D Session 1 reference: enter LLS (Low Leakage Stop) between wakeups,
 * wake every 5 seconds on an LPTMR/LLWU event, blink the red LED, print a
 * wake counter, and go back to sleep. No sensors, no Bluetooth yet, this
 * stage is purely about proving the sleep/wake cycle itself works and is
 * measurably lower current during the sleep half.
 *
 * Adapted from the verified sequence in
 * SDK_2_2_0_FRDM-KL26Z/boards/frdmkl26z/demo_apps/power_manager, which
 * exercises every KL26Z power mode; this project uses only the specific
 * LLS + LPTMR + LLWU path from that reference, not its generic
 * multi-mode power-manager framework. See the manual, Sections 6 to 8.
 *
 * Reference only. Get the stop-mode/LPTMR/LLWU sequence working yourself
 * first; open this only if stuck.
 */

/*
 * WHAT: Sleeps in a deep low-power mode almost all the time, waking only
 * once every 5 seconds to blink the LED and print a counter, then goes
 * straight back to sleep.
 *
 * HOW: A low-power timer (LPTMR) that keeps running even while the CPU is
 * asleep counts up to 5 seconds, then signals the LLWU (Low Leakage Wakeup
 * Unit), which is what actually wakes the core back up from LLS mode; the
 * main loop's only job is to enter that sleep mode, then react once woken.
 *
 * WHY: Every project so far in this series has assumed the board stays
 * powered and awake continuously; a battery-powered field sensor can't
 * afford that, most of its life should be spent drawing minimal current.
 * LLS (Low Leakage Stop) was deliberately chosen over the even
 * deeper VLLS mode because LLS keeps SRAM and register state intact and
 * doesn't reset on wake, while still using the same LLWU wake mechanism
 * the project spec calls for; see InitLptmrAndLlwu and EnterLlsUntilWake
 * below for how that translates into actual register-level setup.
 */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lptmr.h"
#include "fsl_llwu.h"
#include "fsl_smc.h"

#define WAKE_PERIOD_SECONDS 5U
#define LLWU_LPTMR_MODULE_IDX 0U /* LPTMR is internal wakeup module 0 on this chip */

static volatile uint32_t g_wakeCount = 0U;

/*
 * WHAT: Fires once the LLWU notices the LPTMR's wake event and has already
 * brought the core back up out of LLS.
 * HOW: Confirms the wakeup really was the LPTMR (not some other wakeup
 * source sharing this same handler), then disables the LPTMR's interrupt,
 * clears its compare flag, and stops it, resetting it to a clean state
 * ready for the next EnterLlsUntilWake call.
 * WHY: This ISR exists only because LLS (unlike plain Stop/VLPS) requires
 * LLWU, not the LPTMR's own NVIC line, to actually notice the wakeup
 * event and let the core resume. See the manual, Section 7. Note this
 * handler does NOT do the LED toggle or PRINTF, that happens back in
 * main()'s loop, right after SMC_SetPowerModeLls returns; this handler's
 * only job is cleaning up the timer that caused the wake.
 */
void LLWU_IRQHandler(void)
{
    /* This ISR exists only because LLS (unlike plain Stop/VLPS) requires
       LLWU, not the LPTMR's own NVIC line, to actually notice the wakeup
       event and let the core resume. See the manual, Section 7. */
    if (LLWU_GetInternalWakeupModuleFlag(LLWU, LLWU_LPTMR_MODULE_IDX))
    {
        LPTMR_DisableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
        LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
        LPTMR_StopTimer(LPTMR0);
    }
}

/*
 * WHAT: One-time setup: configures the LPTMR's clock source and period,
 * and tells the LLWU to treat the LPTMR as a valid wakeup source.
 * HOW: kLPTMR_PrescalerClock_1 selects LPO (a fixed, always-on 1kHz clock
 * that, critically, keeps ticking even in LLS mode, unlike the chip's main
 * system clocks); bypassing the prescaler means the timer counts LPO ticks
 * directly, so setting the period in milliseconds is a simple
 * multiplication rather than needing a divider calculation.
 * WHY: The choice of LPO as the clock source isn't arbitrary, it's the
 * one clock guaranteed to survive the CPU being asleep; using any of the
 * chip's higher-speed clocks here would fail silently in LLS mode, since
 * those clocks are exactly what LLS shuts down to save power.
 */
static void InitLptmrAndLlwu(void)
{
    lptmr_config_t lptmrConfig;

    LPTMR_GetDefaultConfig(&lptmrConfig);
    lptmrConfig.prescalerClockSource = kLPTMR_PrescalerClock_1; /* LPO, a fixed 1 kHz clock that keeps running in LLS */
    lptmrConfig.bypassPrescaler = true;                         /* count LPO ticks directly, no divider */
    LPTMR_Init(LPTMR0, &lptmrConfig);
    LPTMR_SetTimerPeriod(LPTMR0, (1000U * WAKE_PERIOD_SECONDS) - 1U); /* ticks are ms at 1 kHz */

    LLWU_EnableInternalModuleInterruptWakup(LLWU, LLWU_LPTMR_MODULE_IDX, true);
    NVIC_EnableIRQ(LLWU_IRQn);
}

/*
 * WHAT: Starts the LPTMR counting, then puts the chip into LLS mode until
 * the LLWU wakes it back up.
 * HOW: SMC_PreEnterStopModes/SMC_PostExitStopModes bracket the actual sleep
 * call, handling SDK-internal bookkeeping around the transition;
 * SMC_SetPowerModeLls(SMC) is the line that actually stops CPU execution,
 * this function call doesn't return until LLWU_IRQHandler above has
 * already run and the core has resumed.
 * WHY: The comment on SMC_SetPowerModeLls's line is worth taking
 * literally: from the CPU's perspective, this single function call can
 * take 5 real seconds to return, with the CPU doing nothing at all for
 * almost all of that time; that's the entire mechanism this project
 * demonstrates, reframed as a single blocking function call.
 */
static void EnterLlsUntilWake(void)
{
    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
    LPTMR_StartTimer(LPTMR0);

    SMC_PreEnterStopModes();
    SMC_SetPowerModeLls(SMC); /* blocks here; execution resumes on the next line once LLWU wakes the core */
    SMC_PostExitStopModes();
}

int main(void)
{
    gpio_pin_config_t ledConfig;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("\r\n=== P4-D Session 1 reference: LLS stop mode + LPTMR/LLWU wakeup ===\r\n");
    PRINTF("Waking every %u seconds. Measure current on the board's current-measurement\r\n", WAKE_PERIOD_SECONDS);
    PRINTF("jumper (commonly labeled J5 on Freedom boards; confirm against your board's\r\n");
    PRINTF("silkscreen/schematic) to see it drop between wakes.\r\n\r\n");

    ledConfig.pinDirection = kGPIO_DigitalOutput;
    ledConfig.outputLogic = 1U; /* off, active-low */
    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);

    /* Must be called once before entering any power mode beyond RUN. */
    /*
     * WHAT: Explicitly allows the chip to enter any power mode.
     * WHY: On this chip, low-power modes are disabled by default as a
     * safety measure (so a program can't accidentally sleep and never
     * come back without an explicit opt-in); without this call,
     * SMC_SetPowerModeLls below would simply fail to actually enter LLS.
     */
    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);

    InitLptmrAndLlwu();

    /*
     * WHAT: Repeatedly sleeps for ~5 seconds, then wakes, blinks, and
     * prints, forever.
     * WHY: This loop is almost entirely "asleep": EnterLlsUntilWake blocks
     * for nearly the entire 5-second period, and the three lines after it
     * (incrementing the counter, toggling the LED, printing) take a
     * negligible fraction of that time by comparison. This is the inverse
     * of every earlier project's superloop, which was awake and looping
     * continuously; here, being asleep as much as possible is the actual
     * design goal.
     */
    for (;;)
    {
        EnterLlsUntilWake();

        g_wakeCount++;
        GPIO_TogglePinsOutput(BOARD_LED_RED_GPIO, 1U << BOARD_LED_RED_GPIO_PIN);
        PRINTF("wake #%u, elapsed ~%u s\r\n", g_wakeCount, g_wakeCount * WAKE_PERIOD_SECONDS);
    }
}
