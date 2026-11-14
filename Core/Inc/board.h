#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

/*
 * Transmit one classic CAN data frame on FDCAN1.
 * `id` is an 11-bit standard identifier. FDCAN2 is not used.
 * A null data pointer or an id above 0x7FF is dropped.
 */
void Board_CanTxStd(uint16_t id, const uint8_t data[8]);

/*
 * Short critical section so a UART RX interrupt cannot tear a command
 * the 1 kHz tick is copying. Not nestable. Host builds no-op it.
 */
void Board_IrqLock(void);
void Board_IrqUnlock(void);

#ifndef WHEEL_LEG_HOST
/* UART5 on PC12/PB12, 115200 8N1. FDCAN pin mux is not done here.
 * Cube must define `huart5` and `hfdcan1`. */
void Board_DebugUartInit(void);
void Board_DebugUartOnRxCplt(void);
void Board_DebugUartOnError(void);
#endif

#ifdef WHEEL_LEG_HOST
#define BOARD_CAN_LOG_CAP 64u

typedef struct {
    uint16_t id;
    uint8_t data[8];
} BoardCanFrame;

void Board_CanLogClear(void);
uint32_t Board_CanLogCount(void);
const BoardCanFrame *Board_CanLogAt(uint32_t index);
uint32_t Board_CanCmdCount(uint8_t cmd);
#endif

#endif
