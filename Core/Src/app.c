#include "app.h"

#include "fsm.h"
#include "mf9025.h"
#include "teleop_uart.h"
#include "wheel_leg_config.h"

static uint16_t s_enable_div;
static FsmState s_enable_watch;

static void send_wheel_enable(void) {
    MF9025_SendEnable((uint8_t)MF9025_NODE_ID_LEFT);
    MF9025_SendEnable((uint8_t)MF9025_NODE_ID_RIGHT);
}

void WheelLeg_Init(void) {
    Fsm_Init();
    s_enable_div = 0;
    s_enable_watch = Fsm_State();
    /* Once at power-on. The 1 kHz poll repeats this; not a CAN ISR. */
    send_wheel_enable();
}

void App_Tick1kHz(void) {
    FsmState now;

    Teleop_Age1ms();
    Fsm_Step();
    Teleop_Tick1kHz();

    now = Fsm_State();
    if (now == FSM_ESTOP || now == FSM_FAULT) {
        s_enable_watch = now;
        return;
    }
    /* The tick that leaves ESTOP or FAULT (CLEAR) re-enables immediately. */
    if (s_enable_watch == FSM_ESTOP || s_enable_watch == FSM_FAULT) {
        s_enable_watch = now;
        s_enable_div = 0;
        send_wheel_enable();
        return;
    }
    s_enable_watch = now;
    s_enable_div++;
    if (s_enable_div >= 1000u) {
        s_enable_div = 0;
        send_wheel_enable();
    }
}
