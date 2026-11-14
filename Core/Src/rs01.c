#include "rs01.h"

static uint32_t s_set_speed_calls;

void RS01_SetSpeed(uint8_t id, float vel_rad_s) {
    (void)id;
    (void)vel_rad_s;
    s_set_speed_calls++;
}

void RS01_StopReserved(uint8_t id) {
    (void)id;
    /*
     * Type-4 stop stays empty. Filling the data bytes would require the
     * RS01 manual, which is not in this repository.
     */
}

uint32_t RS01_SetSpeedCallCount(void) { return s_set_speed_calls; }
