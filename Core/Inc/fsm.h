#ifndef FSM_H
#define FSM_H

/*
 * Phase 1 states. Wheel test and teleop only.
 * Only TEST may enter TELEOP.
 * ESTOP and FAULT can be entered from any state.
 * Leaving ESTOP or FAULT requires an explicit CLEAR.
 */

typedef enum {
    FSM_TEST = 0,
    FSM_TELEOP = 1,
    FSM_ESTOP = 2,
    FSM_FAULT = 3
} FsmState;

void Fsm_Init(void);
void Fsm_Step(void);
void Fsm_RaiseFault(void);
FsmState Fsm_State(void);

#endif
