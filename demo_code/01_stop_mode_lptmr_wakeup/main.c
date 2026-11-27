/* ==========================================================================
 *  P4D DEMO 1: SLEEP AND WAKE (LOW POWER), A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: make the board sleep almost all the time, wake up once every 5
 *  seconds, blink the red LED, print a wake counter, and go straight back
 *  to sleep.
 *
 *  Why? Every project so far kept the chip awake and busy forever. A
 *  battery-powered sensor in a field cannot afford that. It must spend
 *  nearly its whole life asleep, drawing almost no current, and wake up only
 *  to do a quick job. This demo proves the sleep and wake cycle works,
 *  before any sensors or radios are added.
 *
 *  THE CAST
 *
 *    LPTMR  a Low Power Timer. It keeps counting even while the CPU is
 *           asleep. It is the alarm clock.
 *    LLWU   the Low Leakage Wake-Up Unit. It listens for the alarm and
 *           wakes the CPU. It is the person who shakes you awake.
 *    LLS    Low Leakage Stop, the deep sleep mode we use. The CPU stops
 *           completely, but memory and variables are kept, so the program
 *           carries on exactly where it left off after waking.
 *
 *  This file starts almost empty on purpose. The code for each step is
 *  already here, but turned OFF (commented out). You switch it on one step
 *  at a time, run it on the board, and see what changed.
 *
 *  BEFORE YOU START: SET UP YOUR PROJECT (3 quick steps)
 *
 *    1. Create a new project with the SDK wizard. The P0 manual, Section 9,
 *       Step 1, shows exactly how.
 *    2. Paste this whole file over the contents of your project's main
 *       source file.
 *    3. Replace your project's pin_mux.c with the pin_mux.c that sits next
 *       to this file. It tells the chip which pins are the LED, the button
 *       and so on. Forget it and the program still builds and runs, but
 *       the LED or button does nothing, with no error message at all.
 *
 *  HOW TO USE THIS FILE
 *
 *    1. Read the STEP box. It explains the idea in plain words.
 *    2. Select ALL the grey "//" code lines under it (the whole block).
 *    3. Press  Cmd + /  (Mac)  or  Ctrl + /  (Windows)  to turn them ON.
 *    4. Build, flash, and watch the board and the terminal.
 *    5. Do the "TRY IT" exercise, then move on to the next STEP.
 *
 *  Do the steps IN ORDER. Each step is one self-contained block, so after
 *  you turn on a step the program always builds and runs with no errors.
 *  Each box has a "NEEDS" line telling you which earlier steps must be on.
 *
 *  THE ONE RULE
 *
 *    Text inside  / * ... * /  boxes is explanation. Leave it alone.
 *    Lines starting with  //  are code that is switched off. Those are the
 *    only lines you ever turn on.
 *
 *  HOW THIS FILE IS LAID OUT
 *
 *    PART A (STEPS 1 to 3, above main): builds the sleep and wake pieces.
 *    Nothing runs yet. Each step just needs to build with no errors.
 *
 *    PART B (STEPS 4 to 6, inside main): sets things up and runs the cycle.
 *
 *  A WARNING ABOUT SLEEPING BOARDS
 *    While the chip is asleep, the debugger may lose its connection to it.
 *    That is normal and not a bug. If it happens, press the reset button on
 *    the board and carry on.
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  fsl_lptmr.h  the low power timer driver
 *  fsl_llwu.h   the wake-up unit driver
 *  fsl_smc.h    the System Mode Controller, the part of the chip that
 *               switches power modes
 * -------------------------------------------------------------------------- */

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lptmr.h"
#include "fsl_llwu.h"
#include "fsl_smc.h"





/* ==========================================================================
 *  PART A, STEP 1: THE NUMBERS AND THE WAKE-UP HANDLER
 *
 *  CONCEPT
 *    WAKE_PERIOD_SECONDS 5       how long to sleep each time
 *    LLWU_LPTMR_MODULE_IDX 0     on this chip, the LPTMR is "internal wake
 *                                up module number 0". The wake-up unit
 *                                knows its sources by number.
 *    g_wakeCount                 counts how many times we have woken up.
 *                                "volatile" because it can change behind
 *                                the compiler's back.
 *
 *    LLWU_IRQHandler runs when the wake-up unit has just woken the chip. It:
 *      1. checks that the LPTMR was really the reason for the wake (other
 *         sources share this handler too)
 *      2. turns off the timer's interrupt, clears its "time is up" marker,
 *         and stops the timer
 *    That leaves the timer clean for the next sleep. Notice what the
 *    handler does NOT do: it does not blink the LED or print. That
 *    happens back in main(), right after the sleep call returns. The
 *    handler's only job is to tidy up the timer that caused the wake.
 *
 *    Why is a special handler needed at all? In this deep sleep, the
 *    timer's own interrupt line is switched off. The wake-up unit is the
 *    only thing awake and listening.
 *
 *  NEEDS: nothing. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the whole block on and build.
 *
 *  NEXT: go to STEP 2.
 * ========================================================================== */

//#define WAKE_PERIOD_SECONDS 5U
//#define LLWU_LPTMR_MODULE_IDX 0U
//
//volatile uint32_t g_wakeCount = 0U;
//
//void LLWU_IRQHandler(void)
//{
//    if (LLWU_GetInternalWakeupModuleFlag(LLWU, LLWU_LPTMR_MODULE_IDX))
//    {
//        LPTMR_DisableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
//        LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
//        LPTMR_StopTimer(LPTMR0);
//    }
//}





/* ==========================================================================
 *  PART A, STEP 2: SET UP THE ALARM CLOCK
 *
 *  CONCEPT
 *    InitLptmrAndLlwu does a one-time setup in two parts.
 *
 *    The timer. The important choice is its CLOCK SOURCE.
 *      kLPTMR_PrescalerClock_1   selects the LPO, a slow, fixed 1 kHz clock
 *                                (1000 ticks a second) that keeps running
 *                                even in deep sleep.
 *      bypassPrescaler = true    count LPO ticks directly, with no divider.
 *
 *    Why LPO? In deep sleep the chip shuts down its fast clocks to save
 *    power. If the timer used a fast clock, it would silently stop counting
 *    the moment we slept, and the alarm would never ring. LPO is the one
 *    clock guaranteed to survive.
 *
 *    Because LPO ticks 1000 times a second, the period in ticks is simply
 *    1000 times the seconds. The "- 1" is because counting starts at zero.
 *
 *    The wake-up unit. LLWU_EnableInternalModuleInterruptWakup tells the
 *    LLWU "treat the LPTMR as something that can wake me", and
 *    NVIC_EnableIRQ switches on the interrupt line for the LLWU itself.
 *
 *  NEEDS: STEP 1 already on. Safe to flash right away. Defined, not run.
 *
 *  TRY IT
 *    Turn the block on and build. Find the line that works out the timer's
 *    period. If you wanted a 2 second sleep, which single number changes?
 *
 *  NEXT: go to STEP 3.
 * ========================================================================== */

//void InitLptmrAndLlwu(void)
//{
//    lptmr_config_t lptmrConfig;
//
//    LPTMR_GetDefaultConfig(&lptmrConfig);
//    lptmrConfig.prescalerClockSource = kLPTMR_PrescalerClock_1;
//    lptmrConfig.bypassPrescaler = true;
//
//    LPTMR_Init(LPTMR0, &lptmrConfig);
//    LPTMR_SetTimerPeriod(LPTMR0, (1000U * WAKE_PERIOD_SECONDS) - 1U);
//
//    LLWU_EnableInternalModuleInterruptWakup(LLWU, LLWU_LPTMR_MODULE_IDX, true);
//    NVIC_EnableIRQ(LLWU_IRQn);
//}





/* ==========================================================================
 *  PART A, STEP 3: GO TO SLEEP
 *
 *  CONCEPT
 *    EnterLlsUntilWake does one full sleep, in order:
 *
 *      1. turn on the timer's interrupt and START the timer counting
 *      2. SMC_PreEnterStopModes     get ready for sleep (SDK bookkeeping)
 *      3. SMC_SetPowerModeLls       THE line that actually stops the CPU
 *      4. SMC_PostExitStopModes     tidy up after waking (SDK bookkeeping)
 *
 *    The surprising part is line 3. As far as your code is concerned, it is
 *    just a function call, but it takes about 5 REAL SECONDS to return.
 *    The CPU does nothing at all for nearly the whole time. When the timer
 *    rings and the LLWU wakes the chip, the handler from STEP 1 runs, and
 *    only then does line 3 finally "return", and the program continues on
 *    the next line as if nothing had happened.
 *
 *    That one blocking function call is the entire trick of this project.
 *
 *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away. Defined, not
 *  run.
 *
 *  TRY IT
 *    Turn the block on and build. That finishes Part A. Which line do you
 *    think is the one that takes 5 seconds to return?
 *
 *  NEXT: go to STEP 4 inside main().
 * ========================================================================== */

//void EnterLlsUntilWake(void)
//{
//    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
//    LPTMR_StartTimer(LPTMR0);
//
//    SMC_PreEnterStopModes();
//    SMC_SetPowerModeLls(SMC);
//    SMC_PostExitStopModes();
//}





int main(void)
{

    /* ======================================================================
     *  PART B, STEP 4: WAKE UP THE BOARD
     *
     *  CONCEPT
     *    Three setup calls run once, in this order, before anything else:
     *
     *      BOARD_InitPins()          decide what each pin does
     *      BOARD_BootClockRUN()      start the clock (the chip's heartbeat)
     *      BOARD_InitDebugConsole()  open the serial line to your computer
     *
     *    Then a banner. It mentions measuring the current: boards like this
     *    have a jumper (often labelled J5, but check your board's
     *    silkscreen) where you can connect a meter and watch the current
     *    drop while the chip sleeps.
     *
     *  NEEDS: nothing from Part A. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the block on, flash, and open the terminal. You should see
     *    the banner. Change the title to your own words.
     *
     *  NEXT: go to STEP 5.
     * ==================================================================== */

    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //PRINTF("\r\n=== P4-D Session 1: LLS stop mode + LPTMR/LLWU wakeup ===\r\n");
    //PRINTF("Waking every %u seconds. Measure current on the board's current-measurement\r\n", WAKE_PERIOD_SECONDS);
    //PRINTF("jumper (commonly labeled J5 on Freedom boards; confirm against your board's\r\n");
    //PRINTF("silkscreen/schematic) to see it drop between wakes.\r\n\r\n");





    /* ======================================================================
     *  PART B, STEP 5: GET READY TO SLEEP
     *
     *  CONCEPT
     *    Three jobs, done once.
     *
     *    1. Set up the red LED as an output, starting OFF (1 means off on
     *       this board).
     *
     *    2. SMC_SetPowerModeProtection(... kSMC_AllowPowerModeAll). On this
     *       chip, deep sleep modes are LOCKED by default, as a safety
     *       measure, so a program cannot put itself to sleep by accident
     *       and never wake. You must explicitly unlock them. Skip this
     *       and the sleep call in STEP 3 simply will not enter the deep
     *       sleep.
     *
     *    3. InitLptmrAndLlwu(): the alarm clock from STEP 2.
     *
     *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the block on and flash. Nothing visible changes. The board is
     *    ready, but it has not been told to sleep yet.
     *
     *  NEXT: go to STEP 6.
     * ==================================================================== */

    //gpio_pin_config_t ledConfig;
    //
    //ledConfig.pinDirection = kGPIO_DigitalOutput;
    //ledConfig.outputLogic = 1U;
    //GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);
    //
    //SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
    //
    //InitLptmrAndLlwu();





    /* ======================================================================
     *  PART B, STEP 6: SLEEP, WAKE, BLINK, REPEAT
     *
     *  CONCEPT
     *    The loop is almost entirely asleep.
     *
     *      EnterLlsUntilWake()     sleeps about 5 seconds (nearly all the
     *                              time is spent right here)
     *      g_wakeCount++           count the wake
     *      toggle the LED          show that we woke
     *      PRINTF                  report the wake number and the elapsed
     *                              time
     *
     *    The three lines after the sleep take a tiny fraction of a second.
     *    Every earlier project's loop was awake and busy all the time. In
     *    this one, being asleep as much as possible is the whole goal.
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the block on and flash. Open the terminal. Every 5 seconds
     *    the red LED should toggle and a new "wake #" line should appear.
     *    Watch for a full minute. Does the counter keep up with a real
     *    clock?
     *
     *  NEXT: go to the exercises below.
     * ==================================================================== */

    //for (;;)
    //{
    //    EnterLlsUntilWake();
    //
    //    g_wakeCount++;
    //
    //    GPIO_TogglePinsOutput(BOARD_LED_RED_GPIO, 1U << BOARD_LED_RED_GPIO_PIN);
    //
    //    PRINTF("wake #%u, elapsed ~%u s\r\n", g_wakeCount, g_wakeCount * WAKE_PERIOD_SECONDS);
    //}

}





/* ==========================================================================
 *  EXERCISES
 *
 *  1. Change the sleep time. In STEP 1, change WAKE_PERIOD_SECONDS to 2,
 *     then to 10. Flash each time. Does the LED follow? Does the printed
 *     "elapsed" time still match the real time?
 *
 *  2. Measure the saving. If your project leader has a multimeter or a
 *     current meter, connect it across the board's current-measurement
 *     jumper (see the STEP 4 note) and watch the reading. You should see
 *     it spike briefly at each wake and sit very low in between. Write
 *     down both numbers. How many times smaller is the sleeping current?
 *
 *  3. Compare with staying awake. Replace EnterLlsUntilWake() in STEP 6
 *     with a plain busy-wait that counts to a big number (like P0's blink).
 *     Measure the current again. This is what every earlier project cost
 *     in power.
 *
 *  4. Predict, then see. Before you flash, answer: what will happen if you
 *     turn OFF the SMC_SetPowerModeProtection line in STEP 5? Write down
 *     your prediction, flash it, and compare. Put the line back afterward.
 *
 *  5. Wake faster than the blink. Make the board wake every second but
 *     toggle the LED only on every 5th wake. You will need to use
 *     g_wakeCount and the % (remainder) operator.
 *
 *  6. Challenge: how much of the time is the chip actually awake? Add up
 *     roughly how long the three lines after the sleep take. Compared with
 *     a 5 second sleep, what percentage of its time is it asleep? Why does
 *     this matter for battery life?
 *
 *  WHERE TO GO NEXT
 *    Demo 2: add real sensors and a Bluetooth radio to this sleep and wake
 *    cycle, and send live readings to your laptop.
 * ========================================================================== */
