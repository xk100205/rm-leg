#include "mf9025.h"

#include "board.h"
#include "wheel_leg_config.h"

static int node_ok(uint8_t node_id) {
    return node_id >= MF9025_NODE_ID_MIN && node_id <= MF9025_NODE_ID_MAX;
}

static void send(uint8_t node_id, const uint8_t data[8]) {
    uint16_t id;
    if (!node_ok(node_id)) {
        return;
    }
    id = (uint16_t)(MF9025_CAN_CMD_BASE + node_id);
    Board_CanTxStd(id, data);
}

static int32_t dps_to_lsb(float vel_dps) {
    float scaled = vel_dps * (1.0f / MF9025_SPEED_LSB_DPS);
    if (scaled >= 2147483647.0f) {
        return 2147483647;
    }
    if (scaled <= -2147483648.0f) {
        return (-2147483647 - 1);
    }
    if (scaled >= 0.0f) {
        return (int32_t)(scaled + 0.5f);
    }
    return (int32_t)(scaled - 0.5f);
}

void MF9025_SetSpeed(uint8_t node_id, float vel_dps) {
    uint8_t data[8];
    int32_t lsb;
    uint32_t bits;
    /*
     * Speed closed-loop only. DATA[1..3] stay 0, matching the LK CAN
     * frame. The Max Torque Current placeholder is a drive setting, not
     * a byte in this frame, and it is not scaled into the speed word.
     */
    (void)MF9025_A2_MAX_TORQUE_CURRENT_A_PLACEHOLDER;
    lsb = dps_to_lsb(vel_dps);
    bits = (uint32_t)lsb;
    data[0] = MF9025_CMD_SPEED;
    data[1] = 0u;
    data[2] = 0u;
    data[3] = 0u;
    data[4] = (uint8_t)bits;
    data[5] = (uint8_t)(bits >> 8);
    data[6] = (uint8_t)(bits >> 16);
    data[7] = (uint8_t)(bits >> 24);
    send(node_id, data);
}

void MF9025_SendStop(uint8_t node_id) {
    uint8_t data[8];
    uint32_t i;
    data[0] = MF9025_CMD_STOP;
    for (i = 1u; i < 8u; i++) {
        data[i] = 0u;
    }
    send(node_id, data);
}
