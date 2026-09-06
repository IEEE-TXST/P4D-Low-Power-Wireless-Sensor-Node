/*
 * P4-D Session 2 reference: the full wireless sensor node. Every 5
 * seconds: wake from LLS, read the accelerometer and the light-sensor
 * stand-in, transmit one CSV line over UART1 to the HC-05 Bluetooth
 * module, then go back to sleep. python/dashboard.py receives that line
 * over the paired laptop's Bluetooth virtual COM port and plots it live.
 *
 * The HC-05 itself must already be configured (Section 9 of the manual):
 * paired with the laptop, and set to 9600 baud data mode, before this
 * firmware's UART1 traffic will actually reach it correctly.
 *
 * Reference only. Get Session 1's sleep/wake cycle solid first; open this
 * only if stuck on the sensor reads, UART1/HC-05 wiring, or the packet
 * format specifically.
 */
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

#define WAKE_PERIOD_SECONDS 5U
#define LLWU_LPTMR_MODULE_IDX 0U

#define HC05_UART UART1
#define HC05_BAUD_BPS 9600U /* must match whatever AT+UART=... set the module to; see manual Section 9 */

/* ---------------- Power management: identical to Session 1 ---------------- */

void LLWU_IRQHandler(void)
{
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
    lptmrConfig.prescalerClockSource = kLPTMR_PrescalerClock_1;
    lptmrConfig.bypassPrescaler = true;
    LPTMR_Init(LPTMR0, &lptmrConfig);
    LPTMR_SetTimerPeriod(LPTMR0, (1000U * WAKE_PERIOD_SECONDS) - 1U);

    LLWU_EnableInternalModuleInterruptWakup(LLWU, LLWU_LPTMR_MODULE_IDX, true);
    NVIC_EnableIRQ(LLWU_IRQn);
}

static void EnterLlsUntilWake(void)
{
    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);
    LPTMR_StartTimer(LPTMR0);

    SMC_PreEnterStopModes();
    SMC_SetPowerModeLls(SMC);
    SMC_PostExitStopModes();
}

/* ---------------- Accelerometer: reused verbatim from P1 ---------------- */

static uint8_t g_accelAddr = 0U;
static i2c_master_transfer_t g_accelXfer;

static bool AccelReadRegs(uint8_t reg, uint8_t *dst, uint32_t count)
{
    g_accelXfer.slaveAddress = g_accelAddr;
    g_accelXfer.direction = kI2C_Read;
    g_accelXfer.subaddress = reg;
    g_accelXfer.subaddressSize = 1U;
    g_accelXfer.data = dst;
    g_accelXfer.dataSize = count;
    g_accelXfer.flags = kI2C_TransferDefaultFlag;
    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
}

static bool AccelWriteReg(uint8_t reg, uint8_t value)
{
    uint8_t buf[1];
    buf[0] = value;
    g_accelXfer.slaveAddress = g_accelAddr;
    g_accelXfer.direction = kI2C_Write;
    g_accelXfer.subaddress = reg;
    g_accelXfer.subaddressSize = 1U;
    g_accelXfer.data = buf;
    g_accelXfer.dataSize = 1U;
    g_accelXfer.flags = kI2C_TransferDefaultFlag;
    return I2C_MasterTransferBlocking(I2C0, &g_accelXfer) == kStatus_Success;
}

static void InitAccelerometer(void)
{
    static const uint8_t kAccelAddresses[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
    i2c_master_config_t masterConfig;
    uint32_t i;

    I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = 100000U;
    I2C_MasterInit(I2C0, &masterConfig, CLOCK_GetFreq(I2C0_CLK_SRC));

    for (i = 0U; i < sizeof(kAccelAddresses); i++)
    {
        uint8_t whoAmI = 0U;
        g_accelAddr = kAccelAddresses[i];
        if (AccelReadRegs(0x0DU, &whoAmI, 1U) && (whoAmI == 0xC7U))
        {
            break;
        }
        g_accelAddr = 0U;
    }
    if (g_accelAddr != 0U)
    {
        AccelWriteReg(0x2AU, 0x00U);
        AccelWriteReg(0x0EU, 0x01U); /* +/-4g, 0.488 mg/LSB */
        AccelWriteReg(0x2AU, 0x0DU);
    }
}

static void ReadAccelMg(int16_t *xMg, int16_t *yMg, int16_t *zMg)
{
    uint8_t raw[6];
    int16_t xRaw, yRaw, zRaw;

    if ((g_accelAddr == 0U) || !AccelReadRegs(0x01U, raw, 6U))
    {
        *xMg = 0;
        *yMg = 0;
        *zMg = 0;
        return;
    }
    xRaw = (int16_t)((uint16_t)(raw[0] << 8) | raw[1]) / 4;
    yRaw = (int16_t)((uint16_t)(raw[2] << 8) | raw[3]) / 4;
    zRaw = (int16_t)((uint16_t)(raw[4] << 8) | raw[5]) / 4;
    *xMg = (int16_t)((int32_t)xRaw * 488 / 1000);
    *yMg = (int16_t)((int32_t)yRaw * 488 / 1000);
    *zMg = (int16_t)((int32_t)zRaw * 488 / 1000);
}

/* ---------------- Light sensor stand-in: ADC0_SE23, reused from P1/P3 ---------------- */

static void InitAdc(void)
{
    adc16_config_t adcConfig;
    ADC16_GetDefaultConfig(&adcConfig);
    adcConfig.referenceVoltageSource = kADC16_ReferenceVoltageSourceValt;
    ADC16_Init(ADC0, &adcConfig);
    ADC16_SetHardwareAverage(ADC0, kADC16_HardwareAverageDisabled);
}

static uint16_t ReadLightCounts(void)
{
    adc16_channel_config_t channelConfig;
    channelConfig.channelNumber = 23U; /* PTE30, ADC0_SE23 */
    channelConfig.enableInterruptOnConversionCompleted = false;
    ADC16_SetChannelConfig(ADC0, 0U, &channelConfig);
    while (0U == (kADC16_ChannelConversionDoneFlag & ADC16_GetChannelStatusFlags(ADC0, 0U)))
    {
    }
    return ADC16_GetChannelConversionValue(ADC0, 0U);
}

/* ---------------- HC-05, over UART1 ---------------- */

static void InitHc05Uart(void)
{
    uart_config_t config;
    UART_GetDefaultConfig(&config);
    config.baudRate_Bps = HC05_BAUD_BPS;
    config.enableTx = true;
    config.enableRx = false; /* this node only ever transmits sensor data */
    UART_Init(HC05_UART, &config, CLOCK_GetFreq(BUS_CLK));
}

static void SendPacket(int16_t xMg, int16_t yMg, int16_t zMg, uint16_t lightCounts)
{
    char line[48];
    int len = snprintf(line, sizeof(line), "%d,%d,%d,%u\r\n", xMg, yMg, zMg, lightCounts);
    if (len > 0)
    {
        UART_WriteBlocking(HC05_UART, (const uint8_t *)line, (size_t)len);
    }
}

int main(void)
{
    gpio_pin_config_t ledConfig;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    PRINTF("\r\n=== P4-D Session 2 reference: full wireless sensor node ===\r\n");
    PRINTF("Waking every %u seconds, transmitting X,Y,Z,light over UART1/HC-05.\r\n\r\n", WAKE_PERIOD_SECONDS);

    ledConfig.pinDirection = kGPIO_DigitalOutput;
    ledConfig.outputLogic = 1U;
    GPIO_PinInit(BOARD_LED_RED_GPIO, BOARD_LED_RED_GPIO_PIN, &ledConfig);

    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
    InitLptmrAndLlwu();
    InitAccelerometer();
    InitAdc();
    InitHc05Uart();

    for (;;)
    {
        int16_t xMg, yMg, zMg;
        uint16_t lightCounts;

        EnterLlsUntilWake();
        GPIO_TogglePinsOutput(BOARD_LED_RED_GPIO, 1U << BOARD_LED_RED_GPIO_PIN);

        ReadAccelMg(&xMg, &yMg, &zMg);
        lightCounts = ReadLightCounts();

        SendPacket(xMg, yMg, zMg, lightCounts);
        PRINTF("sent: %d,%d,%d,%u\r\n", xMg, yMg, zMg, lightCounts);
    }
}
