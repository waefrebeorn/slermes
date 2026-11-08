/*
 * hermes_clock.h — shared monotonic/wall clock helpers.
 *
 * now_ms() was copy-pasted byte-identical into gateway_client.c and
 * gateway_probe.c (GetTickCount64 on Windows, gettimeofday elsewhere).
 * Single source of truth, minimal includes.
 */
#ifndef HERMES_CLOCK_H
#define HERMES_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Milliseconds since some fixed reference (monotonic where available).
 * Same shape both callers used; callers pass it to other timing code. */
static inline int64_t clock_now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CLOCK_H */
