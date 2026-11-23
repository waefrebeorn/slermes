/*
 * _stdin_recovery.c — faithful C11 port of tui_gateway/_stdin_recovery.py.
 * PoP: w15_stdinreco_6979ca_diagnose_stdin_state @ tui_gateway/_stdin_recovery.py:diagnose_stdin_state
 * PoP: w15_stdinreco_6979ca_handle_spurious_eof  @ tui_gateway/_stdin_recovery.py:handle_spurious_eof
 *
 * The live module reads POSIX fcntl/socket state from fd 0. The C port takes
 * those reads as injected facts (see header); the 60s window filter, the
 * per-minute cap, the exact log strings and the True/False decision are real.
 */
#include "noc_w15_stdinreco_6979ca.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char buf[4096];
    size_t len;
} strbuf;

static void sb_add(strbuf *sb, const char *fmt, ...) {
    if (sb->len >= sizeof(sb->buf)) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(sb->buf + sb->len, sizeof(sb->buf) - sb->len, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(sb->buf) - sb->len) sb->len = sizeof(sb->buf);
    else sb->len += (size_t)n;
}

static char *sb_take(strbuf *sb) {
    char *p = (char *)malloc(sb->len + 1);
    if (p) { memcpy(p, sb->buf, sb->len); p[sb->len] = '\0'; }
    return p;
}

char *w15_stdinreco_6979ca_diagnose_stdin_state(int fcntl_available,
        long long flags, long long nonblock_flag,
        const char *fgetfl_error, const char *rcvtimeout_repr) {
    strbuf sb = {{0}, 0};
    if (!fcntl_available) {
        sb_add(&sb, "O_NONBLOCK=n/a (no fcntl)");
    } else if (fgetfl_error) {
        sb_add(&sb, "F_GETFL error: %s", fgetfl_error);
    } else {
        sb_add(&sb, "O_NONBLOCK=%s", (flags & nonblock_flag) ? "1" : "0");
    }
    if (rcvtimeout_repr) {
        if (sb.len) sb_add(&sb, ", ");
        sb_add(&sb, "SO_RCVTIMEO=%s", rcvtimeout_repr);
    }
    if (!sb.len) sb_add(&sb, "unknown");
    return sb_take(&sb);
}

static const char *jstr_or_null(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : "";
}

json_t *w15_stdinreco_6979ca_handle_spurious_eof(int nonblock,
        const json_t *recovery_times, double now, int max_recoveries,
        const char *diag_state) {
    json_t *times = json_array();
    if (recovery_times && recovery_times->type == JSON_ARRAY) {
        for (size_t i = 0; i < recovery_times->c.count; i++) {
            json_t *v = recovery_times->c.items[i];
            if (v && v->type == JSON_NUMBER) json_append(times, json_number(v->num_val));
        }
    }
    json_t *logs = json_array();
    int recovered = 0;
    if (!nonblock) {
        json_append(logs, json_string("stdin EOF (peer closed)"));
    } else {
        json_append(times, json_number(now));
        /* recovery_times[:] = [t for t in recovery_times if t > now - 60].
         * Build a fresh array so no slot is freed twice (avoids use-after-free). */
        json_t *kept = json_array();
        for (size_t i = 0; i < times->c.count; i++) {
            if (times->c.items[i]->num_val > now - 60.0)
                json_append(kept, json_copy(times->c.items[i]));
        }
        json_free(times);
        times = kept;
        if ((int)times->c.count > max_recoveries) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "stdin spurious-EOF recovery rate exceeded (%d/min, cap %d)",
                     (int)times->c.count, max_recoveries);
            json_append(logs, json_string(msg));
        } else {
            strbuf sb = {{0}, 0};
            sb_add(&sb, "stdin spurious EOF (subprocess O_NONBLOCK flip), recovering: %s",
                   diag_state ? diag_state : "");
            json_append(logs, json_string(sb.buf));
            recovered = 1;
        }
    }
    json_t *out = json_object();
    json_set(out, "recovered", json_bool(recovered != 0));
    json_set(out, "logs", logs);
    json_set(out, "recovery_times", times);
    return out;
}
