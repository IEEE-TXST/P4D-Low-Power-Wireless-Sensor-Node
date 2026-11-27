/* ==========================================================================
 *  P4D DEMO 2: THE WIRELESS SENSOR NODE, A STEP-BY-STEP LADDER
 * ==========================================================================
 *
 *  Goal: every 5 seconds, wake up, read the accelerometer and a light
 *  sensor, send both readings over Bluetooth to your laptop, and go back
 *  to sleep. A Python program on the laptop plots the readings live.
 *
 *  This is the practical shape of a real battery powered sensor: asleep
 *  almost all the time (Demo 1), but still delivering a steady stream of
 *  live data.
 *
 *  THE HARDWARE
 *
 *    The board itself        the accelerometer is already on it
 *    A light sensor stand-in a potentiometer or a photoresistor wired to
 *                            pin PTE30
 *    An HC-05 module         a small Bluetooth board, wired to the board's
 *                            UART1 pins. It turns the serial bytes into
 *                            a wireless link to your laptop.
 *
 *    The HC-05 must be set up BEFORE this firmware can talk to it: paired
 *    with the laptop and set to 9600 baud. That is a one-time job, covered
 *    in the manual (Section 9). Ask a project leader if unsure.
 *
 *  GOOD NEWS: you can do almost this whole demo WITHOUT the HC-05. The
 *  board also prints every line to the normal terminal, so you can watch
 *  the sensor readings on your computer first, and plug in Bluetooth last.
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
 *    4. Build, flash, and watch the terminal.
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
 *    PART A (STEPS 1 to 5, above main): builds the toolbox. Nothing runs
 *    yet. Each step just needs to build with no errors.
 *
 *    PART B (STEPS 6 to 8, inside main): starts everything, then runs the
 *    sleep, wake, read and send cycle.
 *
 *  A WARNING ABOUT SLEEPING BOARDS
 *    While the chip is asleep, the debugger may lose its connection. That
 *    is normal. If it happens, press the reset button on the board.
 * ========================================================================== */


/* --------------------------------------------------------------------------
 *  INCLUDES (already on, nothing to do)
 *
 *  stdio.h gives us snprintf, which builds a line of text in memory. The
 *  rest are drivers: the timer and wake-up pieces from Demo 1, I2C for the
 *  accelerometer, the ADC for the light sensor, and UART for the HC-05.
 * -------------------------------------------------------------------------- */

#include <stdio.h>
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lptmr.h"
#include "fsl_llwu.h"
#include "fsl_smc.h"
#include "fsl_i2c.h"
#include "fsl_adc16.h"
#include "fsl_uart.h"





/* ==========================================================================
 *  PART A, STEP 1: THE NUMBERS
 *
 *  CONCEPT
 *    WAKE_PERIOD_SECONDS 5     how long to sleep between readings
 *    LLWU_LPTMR_MODULE_IDX 0   the LPTMR's number in the wake-up unit
 *    HC05_UART UART1           which serial port the Bluetooth module is on
 *    HC05_BAUD_BPS 9600        its speed. It MUST match what the HC-05 was
 *                              set to, or the laptop receives garbage.
 *
 *  NEEDS: nothing. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the block on and build.
 *
 *  NEXT: go to STEP 2.
 * ========================================================================== */

//#define WAKE_PERIOD_SECONDS 5U
//#define LLWU_LPTMR_MODULE_IDX 0U
//
//#define HC05_UART UART1
//#define HC05_BAUD_BPS 9600U





/* ==========================================================================
 *  PART A, STEP 2: SLEEP AND WAKE (THE SAME AS DEMO 1)
 *
 *  CONCEPT
 *    These three pieces are copied unchanged from Demo 1. Read Demo 1's
 *    boxes if any of this is unfamiliar. In short:
 *
 *      LLWU_IRQHandler     runs when the wake-up unit wakes the chip, and
 *                          tidies up the timer that caused it
 *      InitLptmrAndLlwu    sets up the alarm clock: the always-running
 *                          1 kHz LPO clock, a 5 second period, and the
 *                          wake-up unit
 *      EnterLlsUntilWake   starts the timer and puts the chip into deep
 *                          sleep. It takes about 5 real seconds to return.
 *
 *    What is new in this demo is only what happens right AFTER each wake.
 *
 *  NEEDS: STEP 1 already on. Safe to flash right away. Defined, not run.
 *
 *  TRY IT
 *    Turn the block on and build.
 *
 *  NEXT: go to STEP 3.
 * ========================================================================== */

//void LLWU_IRQHandler(void)
//{
//    if (LLWU_GetInternalWakeupModuleFlag(LLWU, LLWU_LPTMR_MODULE_IDX))
//    {
//        LPTMR_DisableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
//        LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
//        LPTMR_StopTimer(LPTMR0);
//    }
//}
//
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
//
//void EnterLlsUntilWake(void)
//{
//    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
//    LPTMR_StartTimer(LPTMR0);
//
//    SMC_PreEnterStopModes();
//    SMC_SetPowerModeLls(SMC);
//    SMC_PostExitStopModes();
//}





/* ==========================================================================
 *  PART A, STEP 3: THE ACCELEROMETER (REUSED FROM P1 DEMO 4)
 *
 *  CONCEPT
 *    Nothing new. This is the same accelerometer code you have seen in P1
 *    and P4A, just in a small package. Read P1 Demo 4's boxes if any of it
 *    is unfamiliar. In short:
 *
 *      AccelReadRegs / AccelWriteReg   talk to the chip's numbered
 *                                      "mailboxes" over I2C
 *      InitAccelerometer               find the chip on one of four
 *                                      possible addresses, put it in
 *                                      standby, set +/- 4 g, switch it on
 *      ReadAccelMg                     read X, Y, Z and convert to
 *                                      milli-g (thousandths of Earth's
 *                                      gravity)
 *
 *    If the chip is not found, g_accelAddr stays 0 and ReadAccelMg returns
 *    zeros, so the node keeps working.
 *
 *  NEEDS: STEPS 1 and 2 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the block on and build.
 *
 *  NEXT: go to STEP 4.
 * ========================================================================== */

//uint8_t g_accelAddr = 0U;
//i2c_master_transfer_t g_accelXfer;
//
//bool AccelReadRegs(uint8_t reg, uint8_t *dst, uint32_t count)
//{
//    g_accelXfer.slaveAddress = g_accelAddr;
//    g_accelXfer.direction = kI2C_Read;
//    g_accelXfer.subaddress = reg;
//    g_accelXfer.subaddressSize = 1U;
//    g_accelXfer.data = dst;
//    g_accelXfer.dataSize = count;
//    g_accelXfer.flags = kI2C_TransferDefaultFlag;
//
//    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
//}
//
//bool AccelWriteReg(uint8_t reg, uint8_t value)
//{
//    uint8_t buf[1];
//
//    buf[0] = value;
//
//    g_accelXfer.slaveAddress = g_accelAddr;
//    g_accelXfer.direction = kI2C_Write;
//    g_accelXfer.subaddress = reg;
//    g_accelXfer.subaddressSize = 1U;
//    g_accelXfer.data = buf;
//    g_accelXfer.dataSize = 1U;
//    g_accelXfer.flags = kI2C_TransferDefaultFlag;
//
//    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
//}
//
//void InitAccelerometer(void)
//{
//    static const uint8_t kAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
//    i2c_master_config_t masterConfig;
//    uint32_t i;
//
//    I2C_MasterGetDefaultConfig(&masterConfig);
//    masterConfig.baudRate_Bps = 100000U;
//    I2C_MasterInit(I2C0, &masterConfig, CLOCK_GetFreq(I2C0_CLK_SRC));
//
//    for (i = 0U; i < sizeof(kAccelAddresses); i++)
//    {
//        uint8_t whoAmI = 0U;
//
//        g_accelAddr = kAccelAddresses[i];
//
//        if (AccelReadRegs(0x0DU, &whoAmI, 1U) && (whoAmI == 0xC7U))
//        {
//            break;
//        }
//
//        g_accelAddr = 0U;
//    }
//
//    if (g_accelAddr != 0U)
//    {
//        AccelWriteReg(0x2AU, 0x00U);
//        AccelWriteReg(0x0EU, 0x01U);
//        AccelWriteReg(0x2AU, 0x0DU);
//    }
//}
//
//void ReadAccelMg(int16_t *xMg, int16_t *yMg, int16_t *zMg)
//{
//    uint8_t raw[6];
//    int16_t xRaw, yRaw, zRaw;
//
//    if ((g_accelAddr == 0U) || !AccelReadRegs(0x01U, raw, 6U))
//    {
//        *xMg = 0;
//        *yMg = 0;
//        *zMg = 0;
//        return;
//    }
//
//    xRaw = (int16_t)((uint16_t)(raw[0] << 8) | raw[1]) / 4;
//    yRaw = (int16_t)((uint16_t)(raw[2] << 8) | raw[3]) / 4;
//    zRaw = (int16_t)((uint16_t)(raw[4] << 8) | raw[5]) / 4;
//
//    *xMg = (int16_t)((int32_t)xRaw * 488 / 1000);
//    *yMg = (int16_t)((int32_t)yRaw * 488 / 1000);
//    *zMg = (int16_t)((int32_t)zRaw * 488 / 1000);
//}





/* ==========================================================================
 *  PART A, STEP 4: THE LIGHT SENSOR (REUSED FROM P1 DEMO 3)
 *
 *  CONCEPT
 *    Nothing new. This is the same ADC code from P1 Demo 3. A potentiometer
 *    or photoresistor on pin PTE30 stands in for whatever real analog sensor
 *    a deployed node would carry.
 *
 *    InitAdc sets the ADC up. ReadLightCounts triggers one conversion on
 *    channel 23 (pin PTE30), waits for it to finish, and returns the raw
 *    reading, 0 to 4095. More light usually means a different number,
 *    depending on how the sensor is wired.
 *
 *  NEEDS: STEPS 1 to 3 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the block on and build.
 *
 *  NEXT: go to STEP 5.
 * ========================================================================== */

//void InitAdc(void)
//{
//    adc16_config_t adcConfig;
//
//    ADC16_GetDefaultConfig(&adcConfig);
//    adcConfig.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
//    ADC16_Init(ADC0, &adcConfig);
//    ADC16_SetHardwareAverage(ADC0, kADC16_HardwareAverageDisabled);
//}
//
//uint16_t ReadLightCounts(void)
//{
//    adc16_channel_config_t channelConfig;
//
//    channelConfig.channelNumber = 23U;
//    channelConfig.enableInterruptOnConversionCompleted = false;
//
//    ADC16_SetChannelConfig(ADC0, 0U, &channelConfig);
//
//    while (0U == (kADC16_ChannelConversionDoneFlag & ADC16_GetChannelStatusFlags(ADC0, 0U)))
//    {
//    }
//
//    return ADC16_GetChannelConversionValue(ADC0, 0U);
//}





/* ==========================================================================
 *  PART A, STEP 5: TALKING TO THE BLUETOOTH MODULE
 *
 *  CONCEPT
 *    Two functions.
 *
 *    InitHc05Uart
 *        Sets up UART1 at 9600 baud (bits per second), TRANSMIT ONLY.
 *        enableRx = false is on purpose. This node only ever sends data
 *        out. It never needs to hear anything back, so there is no reason
 *        to spend power or setup on a receiver it would never use.
 *
 *    SendPacket
 *        Turns four numbers into one line of plain text and sends it:
 *
 *              X,Y,Z,light\r\n         for example:   12,-8,1004,2150
 *
 *        snprintf builds the text into a small buffer. It returns how many
 *        characters it made, and that length (not the whole buffer size) is
 *        what gets sent, so only the real characters go out. The
 *        "if (len > 0)" check skips the send if snprintf reported an
 *        error, which it does with a negative number. Better to send
 *        nothing than send garbage through the air.
 *
 *        Why plain text and not a binary packet like P4A? Because the HC-05
 *        passes bytes straight through to a normal serial terminal or
 *        Python script on the laptop, where readable text is the easiest
 *        thing to receive and parse.
 *
 *  NEEDS: STEPS 1 to 4 already on. Safe to flash right away.
 *
 *  TRY IT
 *    Turn the block on and build. That finishes the toolbox. Part B starts
 *    it. Look at the format "%d,%d,%d,%u". Can you say what each piece
 *    prints?
 *
 *  NEXT: go to STEP 6 inside main().
 * ========================================================================== */

//void InitHc05Uart(void)
//{
//    uart_config_t config;
//
//    UART_GetDefaultConfig(&config);
//    config.baudRate_Bps = HC05_BAUD_BPS;
//    config.enableTx = true;
//    config.enableRx = false;
//
//    UART_Init(HC05_UART, &config, CLOCK_GetFreq(BUS_CLK));
//}
//
//void SendPacket(int16_t xMg, int16_t yMg, int16_t zMg, uint16_t lightCounts)
//{
//    char line[48];
//    int len = snprintf(line, sizeof(line), "%d,%d,%d,%u\r\n", xMg, yMg, zMg, lightCounts);
//
//    if (len > 0)
//    {
//        UART_WriteBlocking(HC05_UART, (const uint8_t *)line, (size_t)len);
//    }
//}





int main(void)
{

    /* ======================================================================
     *  PART B, STEP 6: WAKE UP THE BOARD AND GET READY TO SLEEP
     *
     *  CONCEPT
     *    The usual three setup calls (pins, clock, debug console), then a
     *    banner. After that, the same preparation as Demo 1:
     *
     *      the red LED as an output, starting OFF
     *      SMC_SetPowerModeProtection   unlock the deep sleep modes (they
     *                                   are locked by default, as a safety
     *                                   measure)
     *      InitLptmrAndLlwu             the alarm clock
     *
     *  NEEDS: STEPS 1 to 5 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the block on, flash, and open the terminal. You should see the
     *    banner. Nothing else yet.
     *
     *  NEXT: go to STEP 7.
     * ==================================================================== */

    //gpio_pin_config_t ledConfig;
    //
    //BOARD_InitPins();
    //BOARD_BootClockRUN();
    //BOARD_InitDebugConsole();
    //
    //PRINTF("\r\n=== P4-D Session 2: full wireless sensor node ===\r\n");
    //PRINTF("Waking every %u seconds, transmitting X,Y,Z,light over UART1/HC-05.\r\n\r\n", WAKE_PERIOD_SECONDS);
    //
    //ledConfig.pinDirection = kGPIO_DigitalOutput;
    //ledConfig.outputLogic = 1U;
    //GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);
    //
    //SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
    //InitLptmrAndLlwu();





    /* ======================================================================
     *  PART B, STEP 7: START THE SENSORS AND THE BLUETOOTH LINK
     *
     *  CONCEPT
     *    Start the accelerometer, the ADC and the HC-05 serial port. All
     *    of this happens ONCE, BEFORE the first sleep. The sleep loop does
     *    not repeat any of it. Only the read-and-send work happens again
     *    after every wake.
     *
     *  NEEDS: STEPS 1 to 6 already on. Safe to flash right away.
     *
     *  TRY IT
     *    Turn the block on and flash. Still nothing visible. Everything is
     *    ready, and nothing has been asked to run yet.
     *
     *  NEXT: go to STEP 8.
     * ==================================================================== */

    //InitAccelerometer();
    //InitAdc();
    //InitHc05Uart();





    /* ======================================================================
     *  PART B, STEP 8: THE FULL CYCLE
     *
     *  CONCEPT
     *    Each time round the loop:
     *
     *      1. EnterLlsUntilWake()   sleep about 5 seconds
     *      2. toggle the LED        right away, so you get an instant sign
     *                               that the chip woke up, even before the
     *                               slower work below finishes
     *      3. read the accelerometer and the light sensor
     *      4. SendPacket            send one line over Bluetooth
     *      5. PRINTF                also print the same line to the terminal
     *
     *    Step 5 is a convenience: it lets you check the readings on your
     *    computer without needing Bluetooth at all.
     *
     *  NEEDS: STEPS 1 to 7 already on. Safe to flash right away. The HC-05
     *  does not have to be connected for this to run.
     *
     *  TRY IT
     *    Turn the block on and flash. Open the terminal. Every 5 seconds the
     *    red LED should toggle and a "sent:" line should appear with four
     *    numbers. Tilt the board and watch X, Y and Z change. Cover the
     *    light sensor or turn the potentiometer and watch the last number
     *    change.
     *
     *  NEXT: go to the section below to see it on a graph.
     * ==================================================================== */

    //for (;;)
    //{
    //    int16_t xMg, yMg, zMg;
    //    uint16_t lightCounts;
    //
    //    EnterLlsUntilWake();
    //
    //    GPIO_TogglePinsOutput(BOARD_LED_RED_GPIO, 1U << BOARD_LED_RED_GPIO_PIN);
    //
    //    ReadAccelMg(&xMg, &yMg, &zMg);
    //    lightCounts = ReadLightCounts();
    //
    //    SendPacket(xMg, yMg, zMg, lightCounts);
    //
    //    PRINTF("sent: %d,%d,%d,%u\r\n", xMg, yMg, zMg, lightCounts);
    //}

}





/* ==========================================================================
 *  SEE IT ON YOUR LAPTOP
 *
 *  Once the terminal output looks right, add Bluetooth:
 *
 *  1. Make sure the HC-05 is wired to the board's UART1 pins and has been
 *     set up (paired, and set to 9600 baud). Your project leader or the
 *     manual's Section 9 covers this.
 *  2. On your laptop, the paired HC-05 appears as a virtual serial port.
 *     Find its name.
 *  3. In the python/ folder next to this file, run dashboard.py and point
 *     it at that port.
 *  4. Every 5 seconds a new point appears on the live graph.
 *
 *  If nothing arrives, check in this order: the baud rate matches (9600),
 *  the TX wire goes to the HC-05's RX pin (the wires cross over), and the
 *  laptop is really paired.
 *
 *
 *  EXERCISES
 *
 *  1. Change the report rate. In STEP 1, change WAKE_PERIOD_SECONDS to 2
 *     and then 20. How does the graph change? What would you pick for a
 *     sensor that has to run a month on one battery? Why?
 *
 *  2. Add a field. Add a fifth number to the line, such as a counter of how
 *     many wakes have happened. You must change THREE places: the
 *     snprintf format and its list of values in SendPacket (STEP 5), the
 *     arguments in the call (STEP 8), and the Python dashboard so it
 *     reads five values instead of four.
 *
 *  3. Make it report only when something changes. Keep the previous
 *     readings. If the new Z reading is within 50 of the old one, skip the
 *     send. What would that do to the battery life, and what might you
 *     miss?
 *
 *  4. Measure the cost of the radio. Using a meter on the current
 *     measurement jumper (as in Demo 1), compare the current during the
 *     send with the current while sleeping. Which part of the cycle uses
 *     the most power?
 *
 *  5. Break the baud rate on purpose. In STEP 1, change HC05_BAUD_BPS to
 *     115200 without changing the HC-05. What does the laptop receive?
 *     Why? (Put it back afterward.)
 *
 *  6. Challenge: swap the CSV text for a small binary packet like the one
 *     in P4A, with a start byte and a CRC. What do you gain, and what do
 *     you lose, compared with plain text?
 *
 *  THE END OF THE SERIES
 *    You now have a battery-friendly node that senses, sleeps, and reports
 *    wirelessly. That same shape sits inside real IoT devices, weather
 *    stations and field monitors.
 * ========================================================================== */
