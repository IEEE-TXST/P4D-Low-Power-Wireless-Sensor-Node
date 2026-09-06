/*
 * P4-D Session 1 pin mux: UART0 console and the red LED, both reused
 * verbatim from P0/P1. LPTMR and LLWU are internal peripherals with no
 * physical pins to mux.
 */
#include "fsl_common.h"
#include "fsl_port.h"
#include "pin_mux.h"

#define PIN1_IDX 1u
#define PIN2_IDX 2u
#define PIN29_IDX 29u

#define SOPT5_UART0TXSRC_UART_TX 0x00u
#define SOPT5_UART0RXSRC_UART_RX 0x00u

void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortE);

    PORT_SetPinMux(PORTA, PIN1_IDX, kPORT_MuxAlt2); /* PTA1 = UART0_RX */
    PORT_SetPinMux(PORTA, PIN2_IDX, kPORT_MuxAlt2); /* PTA2 = UART0_TX */
    SIM->SOPT5 = ((SIM->SOPT5 & (~(SIM_SOPT5_UART0TXSRC_MASK | SIM_SOPT5_UART0RXSRC_MASK))) |
                  SIM_SOPT5_UART0TXSRC(SOPT5_UART0TXSRC_UART_TX) | SIM_SOPT5_UART0RXSRC(SOPT5_UART0RXSRC_UART_RX));

    PORT_SetPinMux(PORTE, PIN29_IDX, kPORT_MuxAsGpio); /* PTE29 = red LED */
}
