#ifndef TELEOP_UART_H
#define TELEOP_UART_H

#include <stdint.h>

void Teleop_Init(void);
void Teleop_OnRxByte(uint8_t b);

/* Advance the link clock by 1 ms. Does not change SEL, SPD, or DIR. */
void Teleop_Age1ms(void);

/*
 * Retransmit the speed command armed by the FSM.
 * Does not recompute direction, sign, or the setpoint.
 */
void Teleop_Tick1kHz(void);

/* 1 when a valid line was stamped less than 100 ms ago. */
int Teleop_LinkAlive(void);

/* 1 when the 100 ms deadman has elapsed. This is the latch condition. */
int Teleop_LinkTimedOut(void);

/* 1 only while the FSM is in TEST or TELEOP. Gates actuation, not parsing. */
void Teleop_SetPermit(int on);
int Teleop_Permit(void);

/*
 * SEL/SPD/DIR are stored only while accept is on (FSM is in TELEOP).
 * ESTOP, CLEAR, and TELEOP are always recorded.
 */
void Teleop_AcceptMotion(int on);
void Teleop_ClearMotion(void);

typedef struct {
    int has_sel;
    int sel;
    int has_spd;
    float spd_dps;
    int has_dir;
    int dir;
    int evt_estop;
    int evt_clear;
    int evt_teleop;
} TeleopLevels;

void Teleop_Read(TeleopLevels *out);
void Teleop_ConsumeEvents(const TeleopLevels *seen);

/*
 * `node_a` is the selected wheel at `vel_a_dps`.
 * `node_b` is the other wheel, held at `vel_b_dps` (zero in phase 1).
 */
void Teleop_ArmWheelSpeeds(uint8_t node_a, float vel_a_dps,
                           uint8_t node_b, float vel_b_dps);
void Teleop_Disarm(void);

uint32_t Teleop_LegRejectCount(void);

#endif
