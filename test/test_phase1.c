#include "app.h"
#include "board.h"
#include "fsm.h"
#include "mf9025.h"
#include "rs01.h"
#include "safety.h"
#include "teleop_uart.h"
#include "wheel_leg_config.h"

#include <stdio.h>

static int g_fails;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_fails++;                                                       \
        }                                                                    \
    } while (0)

static void boot(void) {
    Board_CanLogClear();
    Fsm_Init();
}

static void feed(const char *text) {
    const char *p;
    for (p = text; *p != '\0'; p++) {
        Teleop_OnRxByte((uint8_t)*p);
    }
}

static void ticks(int n) {
    int i;
    for (i = 0; i < n; i++) {
        App_Tick1kHz();
    }
}

static int32_t frame_lsb(const BoardCanFrame *frame) {
    uint32_t bits = (uint32_t)frame->data[4] | ((uint32_t)frame->data[5] << 8) |
                    ((uint32_t)frame->data[6] << 16) | ((uint32_t)frame->data[7] << 24);
    return (int32_t)bits;
}

static void expect_speed(uint32_t index, uint16_t id, int32_t lsb) {
    const BoardCanFrame *frame = Board_CanLogAt(index);
    if (frame == 0) {
        CHECK(frame != 0);
        return;
    }
    CHECK(frame->id == id);
    CHECK(frame->data[0] == MF9025_CMD_SPEED);
    CHECK(frame->data[1] == 0u);
    CHECK(frame->data[2] == 0u);
    CHECK(frame->data[3] == 0u);
    CHECK(frame_lsb(frame) == lsb);
}

static void expect_stop(uint32_t index, uint16_t id) {
    const BoardCanFrame *frame = Board_CanLogAt(index);
    uint32_t i;
    if (frame == 0) {
        CHECK(frame != 0);
        return;
    }
    CHECK(frame->id == id);
    CHECK(frame->data[0] == MF9025_CMD_STOP);
    for (i = 1u; i < 8u; i++) {
        CHECK(frame->data[i] == 0u);
    }
}

static void test_driver_frame(void) {
    uint8_t data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    boot();
    Board_CanLogClear();
    MF9025_SetSpeed(0u, 10.0f);
    MF9025_SetSpeed(33u, 10.0f);
    Board_CanTxStd(0x800u, data);
    Board_CanTxStd(0x7FFu, 0);
    CHECK(Board_CanLogCount() == 0u);

    MF9025_SetSpeed(MF9025_NODE_ID_LEFT, 12.5f);
    expect_speed(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT), 1250);
    CHECK(Board_CanCmdCount(0xA1u) == 0u);

    Board_CanLogClear();
    MF9025_SetSpeed(MF9025_NODE_ID_LEFT, 1.0e10f);
    expect_speed(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT), 2147483647);

    Board_CanLogClear();
    MF9025_SetSpeed(MF9025_NODE_ID_LEFT, -1.0e10f);
    expect_speed(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT),
                 (-2147483647 - 1));

    Board_CanLogClear();
    Safety_ApplyStop();
    CHECK(Board_CanLogCount() == 2u);
    expect_stop(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT));
    expect_stop(1u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_RIGHT));
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
}

static void test_rs01_is_silent(void) {
    uint32_t before;
    boot();
    before = RS01_SetSpeedCallCount();
    Board_CanLogClear();
    RS01_SetSpeed(1u, 1.0f);
    RS01_StopReserved(1u);
    CHECK(RS01_SetSpeedCallCount() == before + 1u);
    CHECK(Board_CanLogCount() == 0u);
}

static void test_legs_rejected(void) {
    uint32_t legs;
    uint32_t rs;
    boot();
    legs = Teleop_LegRejectCount();
    rs = RS01_SetSpeedCallCount();
    feed("SEL 1\nSEL 2\r\nSEL 3\nSEL 4\n");
    ticks(1);
    CHECK(Teleop_LegRejectCount() == legs + 4u);
    CHECK(RS01_SetSpeedCallCount() == rs);
    CHECK(Teleop_LinkAlive());
    CHECK(Fsm_State() == FSM_TEST);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
}

static void test_garbage_does_not_refresh(void) {
    boot();
    feed("NOPE\nSEL 7\nSPD 9999\nSPD -4\nDIR 2\n");
    ticks(1);
    CHECK(!Teleop_LinkAlive());
    CHECK(Fsm_State() == FSM_TEST);
}

static void test_boot_deadman(void) {
    boot();
    ticks(99);
    CHECK(Fsm_State() == FSM_TEST);
    CHECK(!Safety_IsLatched());
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
    ticks(1);
    CHECK(Fsm_State() == FSM_ESTOP);
    CHECK(Safety_Reason() == SAFETY_LATCH_LINK);
    CHECK(Teleop_Permit() == 0);
}

static void enter_teleop(void) {
    feed("TELEOP\n");
    ticks(1);
}

static void test_pre_entry_command_does_not_move(void) {
    boot();
    feed("SEL 5\nSPD 100\nDIR 1\nTELEOP\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_TELEOP);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
    Board_CanLogClear();
    ticks(1);
    CHECK(Fsm_State() == FSM_TELEOP);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
    CHECK(Board_CanCmdCount(MF9025_CMD_STOP) == 2u);
}

static void command_left(void) {
    feed("SEL 5\nSPD 100\r\nDIR 1\n");
    Board_CanLogClear();
    ticks(1);
}

static void test_left_wheel_speed(void) {
    uint16_t left = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT);
    uint16_t right = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_RIGHT);
    int i;
    boot();
    enter_teleop();
    CHECK(Fsm_State() == FSM_TELEOP);
    CHECK(Teleop_Permit() == 1);
    command_left();
    CHECK(Board_CanLogCount() == 2u);
    CHECK(Board_CanCmdCount(MF9025_CMD_STOP) == 0u);
    CHECK(Board_CanCmdCount(0xA1u) == 0u);
    expect_speed(0u, left, 10000);
    expect_speed(1u, right, 0);

    Board_CanLogClear();
    ticks(4);
    CHECK(Board_CanLogCount() == 8u);
    for (i = 0; i < 4; i++) {
        expect_speed((uint32_t)(i * 2), left, 10000);
        expect_speed((uint32_t)(i * 2 + 1), right, 0);
    }
}

static void test_right_wheel_sign_once(void) {
    uint16_t left = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT);
    uint16_t right = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_RIGHT);
    int32_t positive = (int32_t)(100.0f * (float)MF9025_RIGHT_WHEEL_SIGN * 100.0f);
    int i;
    boot();
    enter_teleop();
    feed("SEL 6\nSPD 100\nDIR 1\n");
    Board_CanLogClear();
    ticks(3);
    CHECK(Board_CanLogCount() == 6u);
    for (i = 0; i < 3; i++) {
        expect_speed((uint32_t)(i * 2), right, positive);
        expect_speed((uint32_t)(i * 2 + 1), left, 0);
    }
    feed("DIR -1\n");
    Board_CanLogClear();
    ticks(1);
    expect_speed(0u, right, -positive);
    expect_speed(1u, left, 0);
}

static void test_dir_zero_is_speed_not_stop_command(void) {
    uint16_t left = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT);
    uint16_t right = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_RIGHT);
    boot();
    enter_teleop();
    command_left();
    feed("DIR 0\n");
    Board_CanLogClear();
    ticks(1);
    CHECK(Fsm_State() == FSM_TELEOP);
    CHECK(!Safety_IsLatched());
    CHECK(Board_CanCmdCount(MF9025_CMD_STOP) == 0u);
    expect_speed(0u, left, 0);
    expect_speed(1u, right, 0);
}

static void test_bad_spd_keeps_previous(void) {
    uint16_t left = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT);
    boot();
    enter_teleop();
    command_left();
    feed("SPD 361\nSPD -1\nSPD no\n");
    Board_CanLogClear();
    ticks(1);
    expect_speed(0u, left, 10000);
}

static void test_leg_select_does_not_retarget(void) {
    uint16_t left = (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT);
    uint32_t legs;
    uint32_t rs;
    boot();
    enter_teleop();
    command_left();
    legs = Teleop_LegRejectCount();
    rs = RS01_SetSpeedCallCount();
    feed("SEL 2\n");
    Board_CanLogClear();
    ticks(1);
    CHECK(Teleop_LegRejectCount() == legs + 1u);
    CHECK(RS01_SetSpeedCallCount() == rs);
    expect_speed(0u, left, 10000);
}

static void test_estop_latch_needs_clear(void) {
    boot();
    enter_teleop();
    command_left();
    feed("ESTOP\nCLEAR\n");
    Board_CanLogClear();
    ticks(1);
    CHECK(Fsm_State() == FSM_ESTOP);
    CHECK(Safety_Reason() == SAFETY_LATCH_ESTOP);
    CHECK(Teleop_Permit() == 0);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
    expect_stop(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT));
    expect_stop(1u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_RIGHT));

    feed("TELEOP\nSEL 5\nSPD 100\nDIR 1\n");
    Board_CanLogClear();
    ticks(1);
    CHECK(Fsm_State() == FSM_ESTOP);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);

    feed("CLEAR\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_TEST);
    CHECK(!Safety_IsLatched());
    CHECK(Teleop_Permit() == 1);
    feed("TELEOP\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_TELEOP);
    Board_CanLogClear();
    ticks(1);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
    feed("SEL 5\nSPD 80\nDIR 1\n");
    Board_CanLogClear();
    ticks(1);
    expect_speed(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT), 8000);
}

static void test_link_loss_latches(void) {
    int i;
    boot();
    enter_teleop();
    command_left();
    CHECK(Teleop_LinkAlive());
    for (i = 0; i < 99; i++) {
        App_Tick1kHz();
    }
    CHECK(Teleop_LinkAlive());
    CHECK(Fsm_State() == FSM_TELEOP);
    Board_CanLogClear();
    ticks(1);
    CHECK(!Teleop_LinkAlive());
    CHECK(Fsm_State() == FSM_ESTOP);
    CHECK(Safety_Reason() == SAFETY_LATCH_LINK);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
    CHECK(Board_CanCmdCount(MF9025_CMD_STOP) == 2u);

    feed("SEL 5\nSPD 100\nDIR 1\nTELEOP\n");
    Board_CanLogClear();
    ticks(1);
    CHECK(Fsm_State() == FSM_ESTOP);
    CHECK(Board_CanCmdCount(MF9025_CMD_SPEED) == 0u);
}

static void test_fault_from_teleop_and_test(void) {
    boot();
    Fsm_RaiseFault();
    CHECK(Fsm_State() == FSM_FAULT);
    CHECK(Safety_Reason() == SAFETY_LATCH_FAULT);
    feed("TELEOP\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_FAULT);
    feed("CLEAR\nTELEOP\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_TEST);
    feed("TELEOP\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_TELEOP);
    Fsm_RaiseFault();
    CHECK(Fsm_State() == FSM_FAULT);
    feed("ESTOP\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_FAULT);
    feed("CLEAR\n");
    ticks(1);
    CHECK(Fsm_State() == FSM_TEST);
    CHECK(Teleop_Permit() == 1);
}

static void test_current_placeholder_not_on_wire(void) {
    /* The drive-tool current limit must not appear in DATA[1..3]. */
    boot();
    enter_teleop();
    feed("SEL 5\nSPD 10.5\nDIR -1\n");
    Board_CanLogClear();
    ticks(1);
    expect_speed(0u, (uint16_t)(MF9025_CAN_CMD_BASE + MF9025_NODE_ID_LEFT), -1050);
}

int main(void) {
    test_driver_frame();
    test_rs01_is_silent();
    test_legs_rejected();
    test_garbage_does_not_refresh();
    test_boot_deadman();
    test_pre_entry_command_does_not_move();
    test_left_wheel_speed();
    test_right_wheel_sign_once();
    test_dir_zero_is_speed_not_stop_command();
    test_bad_spd_keeps_previous();
    test_leg_select_does_not_retarget();
    test_estop_latch_needs_clear();
    test_link_loss_latches();
    test_fault_from_teleop_and_test();
    test_current_placeholder_not_on_wire();
    if (g_fails != 0) {
        fprintf(stderr, "%d check(s) failed\n", g_fails);
        return 1;
    }
    printf("phase1 teleop checks passed\n");
    return 0;
}
