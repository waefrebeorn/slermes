/*
 * source_completion.c — real C11 port of selected pieces of upstream Hermes
 * hermes_cli/source_completion.py.
 *
 * PoP: w16_sourcecomp_caf511_acquire_waiting @ hermes_cli/source_completion.py:_acquire_waiting
 * PoP: w16_sourcecomp_caf511_prepared_flag @ hermes_cli/source_completion.py:_PREPARED
 * PoP: w16_sourcecomp_caf511_lock_wait_seconds @ hermes_cli/source_completion.py:INSTALLER_LOCK_WAIT_SECONDS
 */
#include "noc_w16_sourcecomp_caf511.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

char *w16_sourcecomp_caf511_prepared_flag(void) { return dupstr("--prepared"); }

int w16_sourcecomp_caf511_lock_wait_seconds(void) { return 30 * 60; }

static int arr_truth(json_t *node) {
    if (!node) return 1; /* lock.acquire() default: True */
    if (node->type == JSON_BOOL) return node->bool_val ? 1 : 0;
    if (node->type == JSON_NUMBER) return node->num_val != 0.0;
    return 0;
}

json_t *w16_sourcecomp_caf511_acquire_waiting(double wait, json_t *clocks,
                                              json_t *acquire, int holder_pid,
                                              const char *holder_reason) {
    size_t ci = 0, ai = 0;
    size_t nclock = (clocks && clocks->type == JSON_ARRAY) ? clocks->c.count : 0;
    size_t nacq = (acquire && acquire->type == JSON_ARRAY) ? acquire->c.count : 0;
    double deadline, next_notice = 0.0, now, sl;
    int ok = 0;
    json_t *notices = json_array();
    json_t *sleeps = json_array();
    json_t *out;

#define MONO() ((nclock == 0) ? 0.0 : \
    ((ci < nclock - 1) ? clocks->c.items[ci++]->num_val : clocks->c.items[nclock - 1]->num_val))

    deadline = MONO() + wait;
    for (;;) {
        int acquired = (ai < nacq) ? arr_truth(acquire->c.items[ai++]) : 1;
        if (acquired) { ok = 1; break; }

        now = MONO();
        /* `holder` is a live holder object in this contract; a reason never
         * clears by waiting. */
        if (now >= deadline || (holder_reason && holder_reason[0])) break;

        if (now >= next_notice) {
            char buf[256];
            if (holder_pid)
                snprintf(buf, sizeof(buf),
                         "\xE2\x86\x92 Waiting for another Hermes update to finish (process %d)...",
                         holder_pid);
            else
                snprintf(buf, sizeof(buf),
                         "\xE2\x86\x92 Waiting for another Hermes update to finish (its processes)...");
            json_append(notices, json_string(buf));
            next_notice = now + 60.0;
        }
        sl = deadline - now;
        if (sl > 2.0) sl = 2.0;
        json_append(sleeps, json_number(sl));
    }
#undef MONO

    out = json_object();
    json_set(out, "ok", json_bool(ok ? 1 : 0));
    json_set(out, "notices", notices);
    json_set(out, "sleeps", sleeps);
    return out;
}
