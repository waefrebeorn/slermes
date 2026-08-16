/*
 * port_session_stall.c — C port of gateway/session_stall.py
 *
 * Gateway session stall notification policy (#72016 item 2).
 * Consumes the shared activity observation contract as the single
 * progress source; owns only the notify-once policy for
 * "pending inbound + stale progress".
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

/* PoP: gwstall_should_emit @ gateway/session_stall.py:should_emit_session_stall_notification */
bool gwstall_should_emit(double timeout_seconds, double idle_seconds,
                         int has_pending_inbound, int already_notified) {
    /* Python: True when a stall warning should be sent for this session. */
    if (timeout_seconds <= 0) return false;
    if (!has_pending_inbound) return false;
    if (already_notified) return false;
    if (isnan(idle_seconds)) return false;
    return idle_seconds >= timeout_seconds;
}

/* PoP: gwstall_should_clear @ gateway/session_stall.py:should_clear_session_stall_notification */
bool gwstall_should_clear(double timeout_seconds, double idle_seconds,
                          int has_pending_inbound) {
    /* Python: True when a prior stall notice may be cleared (episode ended). */
    if (!has_pending_inbound) return true;
    if (timeout_seconds <= 0) return true;
    /* Unknown progress: hold the latch. */
    if (isnan(idle_seconds)) return false;
    return idle_seconds < timeout_seconds;
}

/* PoP: gwstall_format_notification @ gateway/session_stall.py:format_session_stall_notification */
char *gwstall_format_notification(double idle_seconds) {
    /* Python: user-facing stall warning (ASCII minutes). */
    long mins = (long)(idle_seconds / 60.0);
    if (mins < 1) mins = 1;
    char *out = NULL;
    asprintf(&out,
             "⚠️ Agent session appears stalled (last activity %ld min ago). "
             "Try /new to reset.", mins);
    return out;
}

/* PoP: gwstall_resolve_idle_seconds @ gateway/session_stall.py:resolve_session_idle_seconds_from_activity */
double gwstall_resolve_idle_seconds(const char *seconds_since_activity,
                                    const char *last_activity_ts,
                                    double now) {
    /* Python: idle seconds from a shared activity snapshot only.
     * Prefers seconds_since_activity when present and finite; otherwise
     * derives from last_activity_at / last_activity_ts. Returns NaN when
     * there is no usable progress timestamp. */
    if (seconds_since_activity && seconds_since_activity[0]) {
        char *end = NULL;
        double idle = strtod(seconds_since_activity, &end);
        if (end && *end == '\0' && isfinite(idle)) {
            if (idle < 0) return 0.0;
            return idle;
        }
    }
    if (!last_activity_ts || !last_activity_ts[0]) return NAN;
    char *end = NULL;
    double when = strtod(last_activity_ts, &end);
    if (!end || *end != '\0' || !isfinite(when)) return NAN;
    double clock = (now == now) ? now : (double)time(NULL);
    double idle = clock - when;
    if (idle < 0) return 0.0;
    return idle;
}
