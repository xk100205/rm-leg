#include "fsm.h"

#include "safety.h"
#include "teleop_uart.h"
#include "wheel_leg_config.h"

static FsmState s_state;

static void hold_stopped(FsmState next) {
    s_state = next;
    Teleop_SetPermit(next == FSM_TEST || next == FSM_TELEOP);
    Teleop_AcceptMotion(0);
    Teleop_ClearMotion();
    Teleop_Disarm();
    Safety_ApplyStop();
}

void Fsm_Init(void) {
    Safety_Init();
    Teleop_Init();
    s_state = FSM_TEST;
    Teleop_SetPermit(1);
    Teleop_AcceptMotion(0);
}

void Fsm_RaiseFault(void) {
    Safety_RaiseFault();
    hold_stopped(FSM_FAULT);
}

FsmState Fsm_State(void) { return s_state; }

static void exit_to_test(void) {
    (void)Safety_Clear();
    s_state = FSM_TEST;
    Teleop_SetPermit(1);
    Teleop_AcceptMotion(0);
    Teleop_ClearMotion();
    Teleop_Disarm();
    Safety_ApplyStop();
}

static int clear_accepted(const TeleopLevels *lv) {
    return lv->evt_clear && !lv->evt_estop &&
           (s_state == FSM_ESTOP || s_state == FSM_FAULT);
}

void Fsm_Step(void) {
    TeleopLevels lv;
    float sign;
    float vel;
    uint8_t active;
    uint8_t idle;

    Teleop_Read(&lv);
    if (lv.evt_estop) {
        Safety_RequestEstop();
    }
    (void)Safety_Evaluate();

    if (Safety_Reason() == SAFETY_LATCH_FAULT) {
        hold_stopped(FSM_FAULT);
        if (clear_accepted(&lv)) {
            exit_to_test();
        }
        Teleop_ConsumeEvents(&lv);
        return;
    }

    if (Safety_IsLatched()) {
        hold_stopped(FSM_ESTOP);
        if (clear_accepted(&lv)) {
            exit_to_test();
        }
        Teleop_ConsumeEvents(&lv);
        return;
    }

    if (lv.evt_teleop && s_state == FSM_TEST && Teleop_LinkAlive()) {
        /* Motion parsed before this transition was not stored, so a DIR
         * left over from an earlier batch cannot start the wheels. The
         * host resends while a direction key is held. */
        s_state = FSM_TELEOP;
        Teleop_SetPermit(1);
        Teleop_AcceptMotion(1);
        Teleop_ClearMotion();
        Teleop_Disarm();
        Safety_ApplyStop();
        Teleop_ConsumeEvents(&lv);
        return;
    }

    if (s_state != FSM_TELEOP || !Teleop_Permit() || !Teleop_LinkAlive()) {
        Teleop_Disarm();
        if (s_state == FSM_TEST) {
            Teleop_SetPermit(1);
            Teleop_AcceptMotion(0);
        }
        Safety_ApplyStop();
        Teleop_ConsumeEvents(&lv);
        return;
    }

    if (!(lv.has_sel && lv.has_spd && lv.has_dir)) {
        Teleop_Disarm();
        Safety_ApplyStop();
        Teleop_ConsumeEvents(&lv);
        return;
    }

    if (lv.sel == TELEOP_SEL_LEFT) {
        active = (uint8_t)MF9025_NODE_ID_LEFT;
        idle = (uint8_t)MF9025_NODE_ID_RIGHT;
        sign = 1.0f;
    } else if (lv.sel == TELEOP_SEL_RIGHT) {
        active = (uint8_t)MF9025_NODE_ID_RIGHT;
        idle = (uint8_t)MF9025_NODE_ID_LEFT;
        sign = (float)MF9025_RIGHT_WHEEL_SIGN;
    } else {
        Teleop_Disarm();
        Safety_ApplyStop();
        Teleop_ConsumeEvents(&lv);
        return;
    }

    vel = (float)lv.dir * lv.spd_dps * sign;
    Teleop_ArmWheelSpeeds(active, vel, idle, 0.0f);
    Teleop_ConsumeEvents(&lv);
}
