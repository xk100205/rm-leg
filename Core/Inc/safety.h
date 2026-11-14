#ifndef SAFETY_H
#define SAFETY_H

#include <stdint.h>

typedef enum {
    SAFETY_OK = 0,
    SAFETY_LATCH_ESTOP = 1,
    SAFETY_LATCH_LINK = 2,
    SAFETY_LATCH_FAULT = 3
} SafetyReason;

void Safety_Init(void);

/* Latches on the next Safety_Evaluate. Does not transmit. */
void Safety_RequestEstop(void);
void Safety_RaiseFault(void);

/*
 * Decides and latches. Does not transmit.
 * Latches on an ESTOP request, or when the UART link has had no valid
 * line for more than TELEOP_LINK_TIMEOUT_MS.
 * Key release is delivered by the host as ESTOP; if that line is lost,
 * the same timeout latches the stop.
 * The knee-limit check is a stub and does not trip (legs are off).
 * Returns 1 only when motion is allowed.
 */
int Safety_Evaluate(void);

int Safety_IsLatched(void);
SafetyReason Safety_Reason(void);

/* Drops a latch. Does not transmit and does not enter TELEOP. */
int Safety_Clear(void);

/*
 * Both wheels: MF9025 command 0x81.
 * RS01 type-4 stop is not sent; its data bytes are not known here.
 */
void Safety_ApplyStop(void);

#endif
