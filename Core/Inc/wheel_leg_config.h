#ifndef WHEEL_LEG_CONFIG_H
#define WHEEL_LEG_CONFIG_H

/*
 * Phase-1 placeholders. Confirm every value in this file before the
 * wheels are powered. None of these are calibrated bring-up numbers.
 */

/* Debug UART: UART5, PC12 TX (AF8), PB12 RX (AF14), 115200 8N1. STM32H7. */
#define TELEOP_UART_BAUD 115200u

/* Deadman. A valid UART line must arrive at least this often. */
#define TELEOP_LINK_TIMEOUT_MS 100u

/* Logical selectors on the ASCII link. 1..4 are legs and are refused. */
#define TELEOP_SEL_LEG_MIN 1
#define TELEOP_SEL_LEG_MAX 4
#define TELEOP_SEL_LEFT 5
#define TELEOP_SEL_RIGHT 6

/*
 * PLACEHOLDER CAN node ids (LK range 1..32).
 * SEL 5 drives LEFT, SEL 6 drives RIGHT. The selector and the node id
 * are the same number only until the bench wiring says otherwise.
 */
#define MF9025_NODE_ID_LEFT 5u
#define MF9025_NODE_ID_RIGHT 6u

/* LK single-motor command id: standard 11-bit frame, 0x140 + node. */
#define MF9025_CAN_CMD_BASE 0x140u

#define MF9025_CMD_STOP 0x81u
#define MF9025_CMD_SPEED 0xA2u

/* LK speed closed-loop unit: 0.01 dps per LSB. Not a torque scale. */
#define MF9025_SPEED_LSB_DPS 0.01f

/*
 * PLACEHOLDER. Command 0xA2 has no current field (DATA[1..3] are 0).
 * The drive applies its stored Max Torque Current. Set that, in amperes,
 * from the LK host tool before bring-up. This firmware does not transmit
 * the value and does not scale it.
 */
#define MF9025_A2_MAX_TORQUE_CURRENT_A_PLACEHOLDER 0.0f

/*
 * PLACEHOLDER. +1 or -1, applied only to SEL 6 so DIR +1 is the same
 * vehicle direction on both wheels. Flip this on the bench if the right
 * wheel rolls backward.
 */
#define MF9025_RIGHT_WHEEL_SIGN (-1)

/* PLACEHOLDER. Firmware rejects an SPD line above this magnitude. */
#define TELEOP_SPD_MAX_DPS_PLACEHOLDER 360.0f

#define MF9025_NODE_ID_MIN 1u
#define MF9025_NODE_ID_MAX 32u

#if MF9025_NODE_ID_LEFT < MF9025_NODE_ID_MIN || MF9025_NODE_ID_LEFT > MF9025_NODE_ID_MAX
#error MF9025_NODE_ID_LEFT is outside 1..32
#endif
#if MF9025_NODE_ID_RIGHT < MF9025_NODE_ID_MIN || MF9025_NODE_ID_RIGHT > MF9025_NODE_ID_MAX
#error MF9025_NODE_ID_RIGHT is outside 1..32
#endif
#if MF9025_NODE_ID_LEFT == MF9025_NODE_ID_RIGHT
#error wheel node ids must be different
#endif
#if MF9025_RIGHT_WHEEL_SIGN != 1 && MF9025_RIGHT_WHEEL_SIGN != -1
#error MF9025_RIGHT_WHEEL_SIGN must be +1 or -1
#endif

#endif
