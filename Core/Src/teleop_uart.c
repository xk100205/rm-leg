#include "teleop_uart.h"

#include "board.h"
#include "mf9025.h"
#include "wheel_leg_config.h"

#include <stdlib.h>
#include <string.h>

#define LINE_CAP 64

static char s_line[LINE_CAP];
static uint8_t s_line_len;
static uint8_t s_drop_until_nl;

static volatile uint32_t s_uptime_ms;
static volatile uint32_t s_last_valid_ms;
static volatile uint8_t s_seen_valid;
static volatile uint8_t s_rx_pulse;

static int s_permit;
static int s_accept;

static TeleopLevels s_pub;
static uint32_t s_leg_reject;

static int s_armed;
static uint8_t s_node_a;
static float s_vel_a;
static uint8_t s_node_b;
static float s_vel_b;

static int token_int(const char *s, int *out) {
    char *end;
    long v;
    if (s == 0 || s[0] == '\0') {
        return 0;
    }
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0;
    }
    if (v < -1000000L || v > 1000000L) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int token_spd(const char *s, float *out) {
    char *end;
    float v;
    if (s == 0 || s[0] == '\0') {
        return 0;
    }
    v = strtof(s, &end);
    if (end == s || *end != '\0') {
        return 0;
    }
    /* A negative or non-finite value fails the comparison. */
    if (!(v >= 0.0f && v <= TELEOP_SPD_MAX_DPS_PLACEHOLDER)) {
        return 0;
    }
    *out = v;
    return 1;
}

static void mark_valid(void) { s_rx_pulse = 1u; }

static void commit_line(const char *line) {
    int value;
    float spd;

    Board_IrqLock();
    if (strcmp(line, "ESTOP") == 0) {
        s_pub.evt_estop = 1;
        mark_valid();
    } else if (strcmp(line, "CLEAR") == 0) {
        s_pub.evt_clear = 1;
        mark_valid();
    } else if (strcmp(line, "TELEOP") == 0) {
        s_pub.evt_teleop = 1;
        mark_valid();
    } else if (strncmp(line, "SEL ", 4) == 0) {
        if (!token_int(line + 4, &value)) {
            Board_IrqUnlock();
            return;
        }
        if (value >= TELEOP_SEL_LEG_MIN && value <= TELEOP_SEL_LEG_MAX) {
            s_leg_reject++;
            mark_valid();
            Board_IrqUnlock();
            return;
        }
        if (value != TELEOP_SEL_LEFT && value != TELEOP_SEL_RIGHT) {
            Board_IrqUnlock();
            return;
        }
        mark_valid();
        if (s_accept) {
            s_pub.has_sel = 1;
            s_pub.sel = value;
        }
    } else if (strncmp(line, "SPD ", 4) == 0) {
        if (!token_spd(line + 4, &spd)) {
            Board_IrqUnlock();
            return;
        }
        mark_valid();
        if (s_accept) {
            s_pub.has_spd = 1;
            s_pub.spd_dps = spd;
        }
    } else if (strncmp(line, "DIR ", 4) == 0) {
        if (!token_int(line + 4, &value) || (value != -1 && value != 0 && value != 1)) {
            Board_IrqUnlock();
            return;
        }
        mark_valid();
        if (s_accept) {
            s_pub.has_dir = 1;
            s_pub.dir = value;
        }
    }
    Board_IrqUnlock();
}

void Teleop_Init(void) {
    memset(s_line, 0, sizeof s_line);
    s_line_len = 0;
    s_drop_until_nl = 0;
    s_uptime_ms = 0;
    s_last_valid_ms = 0;
    s_seen_valid = 0;
    s_rx_pulse = 0;
    s_permit = 0;
    s_accept = 0;
    memset(&s_pub, 0, sizeof s_pub);
    s_leg_reject = 0;
    s_armed = 0;
    s_node_a = 0;
    s_vel_a = 0.0f;
    s_node_b = 0;
    s_vel_b = 0.0f;
}

void Teleop_OnRxByte(uint8_t b) {
    if (b == '\r') {
        return;
    }
    if (b == '\n') {
        if (!s_drop_until_nl && s_line_len > 0) {
            s_line[s_line_len] = '\0';
            commit_line(s_line);
        }
        s_line_len = 0;
        s_drop_until_nl = 0;
        return;
    }
    if (s_drop_until_nl) {
        return;
    }
    if (s_line_len + 1u >= LINE_CAP) {
        s_drop_until_nl = 1;
        s_line_len = 0;
        return;
    }
    s_line[s_line_len++] = (char)b;
}

void Teleop_Age1ms(void) {
    Board_IrqLock();
    s_uptime_ms++;
    if (s_rx_pulse) {
        s_rx_pulse = 0u;
        s_seen_valid = 1u;
        s_last_valid_ms = s_uptime_ms;
    }
    Board_IrqUnlock();
}

int Teleop_LinkAlive(void) {
    uint32_t up;
    uint32_t last;
    uint8_t seen;
    Board_IrqLock();
    up = s_uptime_ms;
    last = s_last_valid_ms;
    seen = s_seen_valid;
    Board_IrqUnlock();
    if (!seen) {
        return 0;
    }
    /* Fresh while the newest valid line is strictly younger than 100 ms. */
    return (up - last) < TELEOP_LINK_TIMEOUT_MS;
}

int Teleop_LinkTimedOut(void) {
    uint32_t up;
    uint32_t last;
    uint8_t seen;
    Board_IrqLock();
    up = s_uptime_ms;
    last = s_last_valid_ms;
    seen = s_seen_valid;
    Board_IrqUnlock();
    if (!seen) {
        return up >= TELEOP_LINK_TIMEOUT_MS;
    }
    return (up - last) >= TELEOP_LINK_TIMEOUT_MS;
}

void Teleop_SetPermit(int on) {
    Board_IrqLock();
    s_permit = on ? 1 : 0;
    Board_IrqUnlock();
}

int Teleop_Permit(void) {
    int on;
    Board_IrqLock();
    on = s_permit;
    Board_IrqUnlock();
    return on;
}

void Teleop_AcceptMotion(int on) {
    Board_IrqLock();
    s_accept = on ? 1 : 0;
    Board_IrqUnlock();
}

void Teleop_ClearMotion(void) {
    Board_IrqLock();
    s_pub.has_sel = 0;
    s_pub.sel = 0;
    s_pub.has_spd = 0;
    s_pub.spd_dps = 0.0f;
    s_pub.has_dir = 0;
    s_pub.dir = 0;
    Board_IrqUnlock();
}

void Teleop_Read(TeleopLevels *out) {
    if (out == 0) {
        return;
    }
    Board_IrqLock();
    *out = s_pub;
    Board_IrqUnlock();
}

void Teleop_ConsumeEvents(const TeleopLevels *seen) {
    if (seen == 0) {
        return;
    }
    Board_IrqLock();
    if (seen->evt_estop) {
        s_pub.evt_estop = 0;
    }
    if (seen->evt_clear) {
        s_pub.evt_clear = 0;
    }
    if (seen->evt_teleop) {
        s_pub.evt_teleop = 0;
    }
    Board_IrqUnlock();
}

void Teleop_ArmWheelSpeeds(uint8_t node_a, float vel_a_dps, uint8_t node_b,
                           float vel_b_dps) {
    s_node_a = node_a;
    s_vel_a = vel_a_dps;
    s_node_b = node_b;
    s_vel_b = vel_b_dps;
    s_armed = 1;
}

void Teleop_Disarm(void) { s_armed = 0; }

void Teleop_Tick1kHz(void) {
    float vel_a;
    float vel_b;
    uint8_t node_a;
    uint8_t node_b;
    if (!s_armed || !s_permit || !Teleop_LinkAlive()) {
        return;
    }
    /* Copy once so a later arm cannot change this transmit. */
    node_a = s_node_a;
    node_b = s_node_b;
    vel_a = s_vel_a;
    vel_b = s_vel_b;
    MF9025_SetSpeed(node_a, vel_a);
    MF9025_SetSpeed(node_b, vel_b);
}

uint32_t Teleop_LegRejectCount(void) {
    uint32_t n;
    Board_IrqLock();
    n = s_leg_reject;
    Board_IrqUnlock();
    return n;
}
