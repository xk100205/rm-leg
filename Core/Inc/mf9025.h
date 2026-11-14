#ifndef MF9025_H
#define MF9025_H

#include <stdint.h>

/*
 * LingKong MF9025 speed closed-loop, command 0xA2.
 * `vel_dps` is degrees per second. The frame uses 0.01 dps/LSB.
 * Does not call a torque command and does not apply a torque scale.
 */
void MF9025_SetSpeed(uint8_t node_id, float vel_dps);

/* Motor-stop command 0x81 for one node. DATA[1..7] are 0. */
void MF9025_SendStop(uint8_t node_id);

/*
 * Motor ON, command 0x88. DATA[1..7] are 0.
 * Does nothing while the FSM is in ESTOP or FAULT, and does nothing
 * when MF9025_SEND_ENABLE is 0.
 */
void MF9025_SendEnable(uint8_t node_id);

#endif
