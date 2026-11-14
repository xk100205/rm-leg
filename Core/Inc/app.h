#ifndef APP_H
#define APP_H

/* 1 kHz control tick: age the link, step the FSM, resend the armed speed. */
void App_Tick1kHz(void);

/* FSM and teleop state only. Does not program the UART. */
void WheelLeg_Init(void);

/*
 * Target entry: WheelLeg_Init plus UART5 bring-up.
 * Defined in Core/Src/wheel_leg_hooks.c for the Cube build, not the host test.
 */
void WheelLeg_BringUp(void);

#endif
