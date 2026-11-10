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
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif


/* Seconds since CLOCK_MONOTONIC (double, seconds+fraction). Distinct from
 * clock_now_ms (ms): used by timing that needs sub-ms monotonic granularity.
 * (4 identical copies of this lived in port_agent_context_compressor /
 * gw_thread / codex_app_server_session / llm_client.) */
static inline double clock_mono_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

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
