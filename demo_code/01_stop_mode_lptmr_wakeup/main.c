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
    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);

    InitLptmrAndLlwu();

    for (;;)
    {
        EnterLlsUntilWake();

        g_wakeCount++;
        GPIO_TogglePinsOutput(BOARD_LED_RED_GPIO, 1U << BOARD_LED_RED_GPIO_PIN);
        PRINTF("wake #%lu, elapsed ~%lu s\r\n", g_wakeCount, g_wakeCount * WAKE_PERIOD_SECONDS);
    }
}
