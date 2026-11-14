#include "app.h"
#include "board.h"

#include "stm32h7xx_hal.h"

/*
 * Cube integration. This is not a second main().
 *
 * Cube must define `huart5` and `hfdcan1` (FDCAN1 classic, 1 Mbps, 11-bit).
 * The HAL time base must be 1 ms. UART5's IRQ must preempt that tick.
 * If Cube already defines these callbacks, call the same functions from
 * there and do not compile this file.
 *
 * FDCAN1 pins are not programmed here. There is no .ioc yet.
 */

void WheelLeg_BringUp(void) {
    WheelLeg_Init();
    Board_DebugUartInit();
}

void HAL_SYSTICK_Callback(void) { App_Tick1kHz(); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART5) {
        Board_DebugUartOnRxCplt();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART5) {
        Board_DebugUartOnError();
    }
}
