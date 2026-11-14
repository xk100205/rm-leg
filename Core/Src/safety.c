#include "safety.h"

#include "mf9025.h"
#include "teleop_uart.h"
#include "wheel_leg_config.h"

static SafetyReason s_reason;
static int s_latched;
static int s_estop_req;

/* Legs are disabled. This never trips and never opens a leg command. */
static int knee_limit_tripped(void) { return 0; }

void Safety_Init(void) {
    s_reason = SAFETY_OK;
    s_latched = 0;
    s_estop_req = 0;
}

void Safety_RequestEstop(void) { s_estop_req = 1; }

void Safety_RaiseFault(void) {
    s_reason = SAFETY_LATCH_FAULT;
    s_latched = 1;
}

int Safety_Evaluate(void) {
    if (s_reason == SAFETY_LATCH_FAULT) {
        s_latched = 1;
        return 0;
    }
    if (s_estop_req) {
        s_reason = SAFETY_LATCH_ESTOP;
        s_estop_req = 0;
        s_latched = 1;
        return 0;
    }
    if (s_reason == SAFETY_LATCH_ESTOP || s_reason == SAFETY_LATCH_LINK) {
        s_latched = 1;
        return 0;
    }
    if (Teleop_LinkTimedOut()) {
        s_reason = SAFETY_LATCH_LINK;
        s_latched = 1;
        return 0;
    }
    if (knee_limit_tripped()) {
        s_reason = SAFETY_LATCH_FAULT;
        s_latched = 1;
        return 0;
    }
    if (!Teleop_LinkAlive() || !Teleop_Permit()) {
        return 0;
    }
    return 1;
}

int Safety_IsLatched(void) { return s_latched; }

SafetyReason Safety_Reason(void) { return s_reason; }

int Safety_Clear(void) {
    if (!s_latched && s_reason == SAFETY_OK && !s_estop_req) {
        return 0;
    }
    s_latched = 0;
    s_reason = SAFETY_OK;
    s_estop_req = 0;
    return 1;
}

void Safety_ApplyStop(void) {
    /*
     * 0x81 stops each wheel. It does not clear a command the drive has
     * already stored, and this phase never sends the run/resume byte,
     * so a stop cannot be undone by replaying that stored command.
     * Repeat the stop while the latch is held. RS01 is not addressed.
     */
    MF9025_SendStop(MF9025_NODE_ID_LEFT);
    MF9025_SendStop(MF9025_NODE_ID_RIGHT);
}
