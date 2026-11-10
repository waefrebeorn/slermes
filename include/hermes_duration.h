/*
 * hermes_duration.h — shared duration-formatting helpers.
 *
 * format_duration_age() was copy-pasted byte-identical into chat_view.c and
 * sidebar.c; this is the single source of truth. Minimal includes (<time.h>,
 * <stdio.h>) only; callers that already pull them pay nothing.
 */
#ifndef HERMES_DURATION_H
#define HERMES_DURATION_H

#include <time.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Render a wall-clock age ("now", "5m", "2h", "3d", "1w", "4mo") into buf.
 * Delta is clamped to >= 0 and bucketed by the same thresholds live uses. */
static inline void format_duration_age(long started_at, char *buf, size_t sz) {
    time_t now = time(NULL);
    double delta = difftime(now, (time_t)started_at);
    if (delta < 0) delta = 0;
    if (delta < 60) snprintf(buf, sz, "now");
    else if (delta < 3600) snprintf(buf, sz, "%dm", (int)(delta / 60));
    else if (delta < 86400) snprintf(buf, sz, "%dh", (int)(delta / 3600));
    else if (delta < 604800) snprintf(buf, sz, "%dd", (int)(delta / 86400));
    else if (delta < 2592000) snprintf(buf, sz, "%dw", (int)(delta / 604800));
    else snprintf(buf, sz, "%dmo", (int)(delta / 2592000));
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DURATION_H */
