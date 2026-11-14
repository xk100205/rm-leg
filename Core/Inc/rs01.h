#ifndef RS01_H
#define RS01_H

#include <stdint.h>

/* Phase 1: legs are off. This does not transmit. */
void RS01_SetSpeed(uint8_t id, float vel_rad_s);

/*
 * Reserved for a later RS01 type-4 stop.
 * The manual is not in this repo, so the data fields stay empty and
 * nothing is transmitted. Safety_ApplyStop does not call this.
 */
void RS01_StopReserved(uint8_t id);

uint32_t RS01_SetSpeedCallCount(void);

#endif
