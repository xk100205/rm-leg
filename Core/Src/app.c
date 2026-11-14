#include "app.h"

#include "fsm.h"
#include "teleop_uart.h"

void WheelLeg_Init(void) { Fsm_Init(); }

void App_Tick1kHz(void) {
    Teleop_Age1ms();
    Fsm_Step();
    Teleop_Tick1kHz();
}
