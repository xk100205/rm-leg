#include "board.h"

#include "wheel_leg_config.h"

#ifndef WHEEL_LEG_HOST
#include "teleop_uart.h"
#endif

#ifdef WHEEL_LEG_HOST

#include <string.h>

static BoardCanFrame s_log[BOARD_CAN_LOG_CAP];
static uint32_t s_log_n;
static uint32_t s_cmd[256];
static uint32_t s_tx_fail;

void Board_IrqLock(void) {}
void Board_IrqUnlock(void) {}

void Board_CanLogClear(void) {
    memset(s_log, 0, sizeof s_log);
    memset(s_cmd, 0, sizeof s_cmd);
    s_log_n = 0;
}

uint32_t Board_CanLogCount(void) { return s_log_n; }

const BoardCanFrame *Board_CanLogAt(uint32_t index) {
    if (index >= s_log_n) {
        return 0;
    }
    return &s_log[index];
}

uint32_t Board_CanCmdCount(uint8_t cmd) { return s_cmd[cmd]; }

uint32_t Board_CanTxFailCount(void) { return s_tx_fail; }

void Board_CanTxStd(uint16_t id, const uint8_t data[8]) {
    uint32_t i;
    if (data == 0 || id > 0x7FFu) {
        return;
    }
    s_cmd[data[0]]++;
    if (s_log_n >= BOARD_CAN_LOG_CAP) {
        return;
    }
    s_log[s_log_n].id = id;
    for (i = 0; i < 8u; i++) {
        s_log[s_log_n].data[i] = data[i];
    }
    s_log_n++;
}

#else

#include "stm32h7xx_hal.h"

#include <string.h>

extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef huart5;

static uint8_t s_uart5_rx_byte;
static uint32_t s_tx_fail;

void Board_IrqLock(void) { __disable_irq(); }
void Board_IrqUnlock(void) { __enable_irq(); }

void Board_CanTxStd(uint16_t id, const uint8_t data[8]) {
    FDCAN_TxHeaderTypeDef tx;
    if (data == 0 || id > 0x7FFu) {
        return;
    }
    tx.Identifier = id;
    tx.IdType = FDCAN_STANDARD_ID;
    tx.TxFrameType = FDCAN_DATA_FRAME;
    tx.DataLength = FDCAN_DLC_BYTES_8;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch = FDCAN_BRS_OFF;
    tx.FDFormat = FDCAN_CLASSIC_CAN;
    tx.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx.MessageMarker = 0;
    /* Non-blocking. The queue rejects a frame when the TX FIFO is full
     * or the peripheral is not ready; count that and let the next tick
     * retry. Classic 11-bit frame on FDCAN1 only. */
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx, (uint8_t *)data) != HAL_OK) {
        s_tx_fail++;
    }
}

uint32_t Board_CanTxFailCount(void) { return s_tx_fail; }

void Board_DebugUartInit(void) {
    GPIO_InitTypeDef gpio;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_UART5_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(GPIOC, &gpio); /* PC12 UART5_TX */

    gpio.Pin = GPIO_PIN_12;
    gpio.Alternate = GPIO_AF14_UART5;
    HAL_GPIO_Init(GPIOB, &gpio); /* PB12 UART5_RX */

    memset(&huart5, 0, sizeof huart5);
    huart5.Instance = UART5;
    huart5.Init.BaudRate = TELEOP_UART_BAUD;
    huart5.Init.WordLength = UART_WORDLENGTH_8B;
    huart5.Init.StopBits = UART_STOPBITS_1;
    huart5.Init.Parity = UART_PARITY_NONE;
    huart5.Init.Mode = UART_MODE_TX_RX;
    huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart5.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart5) != HAL_OK) {
        return;
    }
    (void)HAL_UART_Receive_IT(&huart5, &s_uart5_rx_byte, 1u);
}

void Board_DebugUartOnRxCplt(void) {
    Teleop_OnRxByte(s_uart5_rx_byte);
    (void)HAL_UART_Receive_IT(&huart5, &s_uart5_rx_byte, 1u);
}

void Board_DebugUartOnError(void) {
    (void)HAL_UART_Receive_IT(&huart5, &s_uart5_rx_byte, 1u);
}

#endif
