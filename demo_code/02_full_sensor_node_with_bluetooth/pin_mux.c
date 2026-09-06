/*
 * P4-D Session 2 pin mux: UART0 console (debug only), UART1 to the HC-05
 * Bluetooth module (verified pins/Alt from P1's UART1 exploration), I2C0
 * (accelerometer), ADC0_SE23 (the light-sensor stand-in, same pin P1/P3
 * used), and the red LED (wake indicator). All values reused verbatim
 * from prior projects; nothing new is guessed here.
 */
#include "fsl_common.h"
#include "fsl_port.h"
#include "pin_mux.h"

#define PIN0_IDX 0u
#define PIN1_IDX 1u
#define PIN2_IDX 2u
#define PIN24_IDX 24u
#define PIN25_IDX 25u
#define PIN29_IDX 29u
#define PIN30_IDX 30u

#define SOPT5_UART0TXSRC_UART_TX 0x00u
#define SOPT5_UART0RXSRC_UART_RX 0x00u

void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortE);

    /* UART0 debug console. */
    PORT_SetPinMux(PORTA, PIN1_IDX, kPORT_MuxAlt2); /* PTA1 = UART0_RX */
    PORT_SetPinMux(PORTA, PIN2_IDX, kPORT_MuxAlt2); /* PTA2 = UART0_TX */
    SIM->SOPT5 = ((SIM->SOPT5 & (~(SIM_SOPT5_UART0TXSRC_MASK | SIM_SOPT5_UART0RXSRC_MASK))) |
                  SIM_SOPT5_UART0TXSRC(SOPT5_UART0TXSRC_UART_TX) | SIM_SOPT5_UART0RXSRC(SOPT5_UART0RXSRC_UART_RX));

    /* UART1 to the HC-05 module. Verified pins/Alt: PTE0 = UART1_TX,
       PTE1 = UART1_RX, both Alt3 (confirmed in driver_examples/uart/interrupt,
       used again here). */
    PORT_SetPinMux(PORTE, PIN0_IDX, kPORT_MuxAlt3); /* PTE0 = UART1_TX -> HC-05 RX */
    PORT_SetPinMux(PORTE, PIN1_IDX, kPORT_MuxAlt3); /* PTE1 = UART1_RX -> HC-05 TX */

    /* Red LED, wake indicator. */
    PORT_SetPinMux(PORTE, PIN29_IDX, kPORT_MuxAsGpio);

    /* ADC0_SE23, the light-sensor stand-in (photoresistor + fixed
       resistor divider), same channel P1/P3 used. */
    PORT_SetPinMux(PORTE, PIN30_IDX, kPORT_PinDisabledOrAnalog);

    /* I2C0 to the on-board FXOS8700CQ accelerometer. */
    PORT_SetPinMux(PORTE, PIN24_IDX, kPORT_MuxAlt5); /* PTE24 = I2C0_SCL */
    PORT_SetPinMux(PORTE, PIN25_IDX, kPORT_MuxAlt5); /* PTE25 = I2C0_SDA */
}
