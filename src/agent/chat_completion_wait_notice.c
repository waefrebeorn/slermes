/*
 * chat_completion_wait_notice.c - C11 port of upstream
 * agent/chat_completion_wait_notice.py (gate w23_chatcomple_c9e7a5).
 *
 * PoP: w23_chatcomple_c9e7a5_wait_notice_text @ agent/chat_completion_wait_notice.py:wait_notice_text
 * PoP: w23_chatcomple_c9e7a5_codex_watchdog_deadline @ agent/chat_completion_wait_notice.py:codex_watchdog_deadline
 * PoP: w23_chatcomple_c9e7a5_should_emit @ agent/chat_completion_wait_notice.py:WaitNoticeState.should_emit
 * PoP: w23_chatcomple_c9e7a5_near_deadline @ agent/chat_completion_wait_notice.py:_near_deadline
 * PoP: w23_chatcomple_c9e7a5_phases @ agent/chat_completion_wait_notice.py:_PHASE_TEXT
 * PoP: w23_chatcomple_c9e7a5_near_secs @ agent/chat_completion_wait_notice.py:NEAR_DEADLINE_SECS
 *
 * Presentation only: no clock is read here.  The caller hands over the phase,
 * the silence, the watchdog tuple and the timestamps the watchdogs already own.
 * "infinite" timeouts arrive as a *_finite flag because JSON cannot carry inf.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_chatcomple_c9e7a5.h"

#define WN_NEAR W23_CHATCOMPLE_C9E7A5_NEAR_DEADLINE_SECS

static const char *const k_phase_key[] = {
    "first_event", "reconnect", "pre_progress", "post_event", "first_chunk", "post_chunk"
};
/* The exact _PHASE_TEXT templates; {n} is replaced by int(silence_secs). */
static const char *const k_phase_tmpl[] = {
    "{n}s waiting for the first provider event",
    "{n}s waiting for the first provider event after reconnect",
    "provider stream open; {n}s without substantive model progress",
    "provider stream active; {n}s without stream events",
    "{n}s waiting for the first stream chunk",
    "stream open; {n}s without stream output"
};
#define WN_PHASE_N (sizeof(k_phase_key) / sizeof(k_phase_key[0]))

static char *wn_dup(const char *text) {
    size_t n;
    char *p;
    if (!text) text = "";
    n = strlen(text);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, text, n + 1);
    return p;
}

static void wn_copy(char *dst, size_t cap, const char *src) {
    size_t n;
    if (!dst || cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

double w23_chatcomple_c9e7a5_near_secs(void) { return WN_NEAR; }

json_t *w23_chatcomple_c9e7a5_phases(void) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < WN_PHASE_N; i++)
        json_append(arr, json_string(k_phase_key[i]));
    return arr;
}

/* Python's `watchdog is not None and watchdog[1] <= NEAR_DEADLINE_SECS`. */
static int wn_watchdog_near(const json_t *wd, double *remaining) {
    const json_t *sv;
    if (!wd || wd->type != JSON_ARRAY || json_len(wd) != 2) return 0;
    sv = json_get(wd, 1);
    if (!sv || sv->type != JSON_NUMBER) return 0;
    if (remaining) *remaining = sv->num_val;
    return sv->num_val <= WN_NEAR;
}

int w23_chatcomple_c9e7a5_near_deadline(const json_t *world) {
    const json_t *wd = world ? json_obj_get(world, "watchdog") : NULL;
    return wn_watchdog_near(wd, NULL) ? 1 : 0;
}

char *w23_chatcomple_c9e7a5_wait_notice_text(const json_t *world) {
    const json_t *wd = world ? json_obj_get(world, "watchdog") : NULL;
    const char *model = json_get_str(world, "model", NULL);
    const char *phase = json_get_str(world, "phase", "");
    const char *label = NULL;
    const char *tmpl = NULL, *at, *lead;
    double silence = json_get_num(world, "silence_secs", 0.0);
    double remaining = 0.0;
    long long n_shown, rest;
    int has_wd = 0, used;
    size_t cap, i;
    char *out;

    for (i = 0; i < WN_PHASE_N; i++)
        if (strcmp(phase, k_phase_key[i]) == 0) tmpl = k_phase_tmpl[i];
    if (!tmpl) return wn_dup("");

    if (wd && wd->type == JSON_ARRAY && json_len(wd) == 2) {
        const json_t *lv = json_get(wd, 0);
        has_wd = 1;
        if (lv && lv->type == JSON_STRING) label = lv->str_val;
        (void)wn_watchdog_near(wd, &remaining);
    }

    /* lead = "still waiting on" if _near_deadline(watchdog) else "waiting on" */
    lead = (has_wd && remaining <= WN_NEAR) ? "still waiting on" : "waiting on";
    if (!model) model = "None";     /* f-string of a None model */
    at = strstr(tmpl, "{n}");
    if (!at) return wn_dup("");
    n_shown = (long long)silence;   /* int(): truncation toward zero */

    cap = strlen(model) + strlen(tmpl) + strlen(lead) + 96;
    if (has_wd && label) cap += strlen(label);
    out = (char *)malloc(cap);
    if (!out) return NULL;
    used = snprintf(out, cap, "\xE2\x8F\xB3 %s %s \xE2\x80\x94 %.*s%lld%s",
                    lead, model, (int)(at - tmpl), tmpl, n_shown, at + 3);
    if (has_wd) {
        rest = (long long)remaining;      /* int(), clamped at zero */
        if (rest < 0) rest = 0;
        if (used > 0 && (size_t)used < cap)
            snprintf(out + used, cap - (size_t)used,
                     " (auto-reconnect: %s watchdog in %llds)",
                     label ? label : "None", rest);
    }
    return out;
}

/* A JSON number under `key`; absent/null/other types count as "not given". */
static int wn_num(const json_t *obj, const char *key, double *out) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v || v->type != JSON_NUMBER) return 0;
    *out = v->num_val;
    return 1;
}

/* earliest deadline wins; a tie keeps the one considered first */
static void wn_consider(const char **best_label, double *best, int *have,
                        const char *label, double value) {
    if (!*have || value < *best) {
        *best = value;
        *best_label = label;
        *have = 1;
    }
}

json_t *w23_chatcomple_c9e7a5_codex_watchdog_deadline(const json_t *world) {
    double stale, ttfb_to, idle_to, call_start, elapsed, prog_to;
    double retry_ts = 0.0, event_ts = 0.0, prog_ts = 0.0;
    double offset, base, best = 0.0;
    const char *best_label = NULL;
    const char *label;
    int has_retry, has_event, has_prog, have = 0;
    json_t *arr;

    stale = json_get_num(world, "stale_timeout", 0.0);
    ttfb_to = json_get_num(world, "ttfb_timeout", 0.0);
    idle_to = json_get_num(world, "idle_timeout", 0.0);
    call_start = json_get_num(world, "call_start", 0.0);
    elapsed = json_get_num(world, "elapsed", 0.0);
    prog_to = json_get_num(world, "progress_timeout", 0.0);
    has_retry = wn_num(world, "retry_started_ts", &retry_ts);
    has_event = wn_num(world, "last_event_ts", &event_ts);
    has_prog = wn_num(world, "last_progress_ts", &prog_ts);

    if (json_get_bool(world, "stale_finite", 1))
        wn_consider(&best_label, &best, &have, "wall-clock stale", stale);

    offset = has_retry ? (retry_ts - call_start) : 0.0;
    if (offset < 0.0) offset = 0.0;   /* the retry offset is clamped at zero */

    if (!has_event) {
        if (json_get_bool(world, "ttfb_enabled", 0) && json_get_bool(world, "ttfb_finite", 1))
            wn_consider(&best_label, &best, &have, "TTFB", offset + ttfb_to);
    } else if (prog_to > 0.0 && !has_prog) {
        wn_consider(&best_label, &best, &have, "first progress", offset + prog_to);
    } else if ((!json_get_bool(world, "idle_requires_progress", 0) || has_prog)
               && json_get_bool(world, "idle_enabled", 0)
               && json_get_bool(world, "idle_finite", 1)) {
        base = event_ts - call_start;
        if (base < 0.0) base = 0.0;   /* the idle base is clamped at zero */
        wn_consider(&best_label, &best, &have, "stream idle", base + idle_to);
    }

    if (!have) return json_null();
    if (best <= elapsed) return json_null();
    label = best_label;
    arr = json_array();
    json_append(arr, json_string(label));
    json_append(arr, json_number(best - elapsed));
    return arr;
}

json_t *w23_chatcomple_c9e7a5_should_emit(const json_t *world) {
    const json_t *steps = world ? json_obj_get(world, "steps") : NULL;
    char phase[256], label[256];
    json_t *out = json_array();
    int has_phase = 0, has_label = 0, near_shown = 0;
    size_t i, n;

    phase[0] = '\0';
    label[0] = '\0';
    n = (steps && steps->type == JSON_ARRAY) ? json_len(steps) : 0;
    for (i = 0; i < n; i++) {
        const json_t *step = json_get(steps, i);
        const json_t *ph, *wd;
        const char *new_label = NULL;
        double remaining = 0.0;
        int new_near = 0, label_now, emit;

        if (!step || step->type != JSON_OBJECT) continue;
        if (json_get_bool(step, "reset", 0)) {
            has_phase = 0;
            has_label = 0;
            near_shown = 0;
            phase[0] = '\0';
            label[0] = '\0';
            continue;
        }
        ph = json_obj_get(step, "phase");
        if (!ph || ph->type != JSON_STRING) continue;
        wd = json_obj_get(step, "watchdog");
        if (wd && wd->type == JSON_ARRAY && json_len(wd) == 2) {
            const json_t *lv = json_get(wd, 0);
            if (lv && lv->type == JSON_STRING) new_label = lv->str_val;
        }
        new_near = wn_watchdog_near(wd, &remaining);
        label_now = (new_label != NULL);

        emit = (!has_phase || strcmp(phase, ph->str_val ? ph->str_val : "") != 0
                || has_label != label_now
                || (has_label && label_now && strcmp(label, new_label) != 0)
                || (new_near && !near_shown));

        wn_copy(phase, sizeof phase, ph->str_val);
        has_phase = 1;
        wn_copy(label, sizeof label, new_label);
        has_label = label_now;
        if (new_near) near_shown = 1;
        json_append(out, json_bool(emit ? 1 : 0));
    }
    return out;
}

/* Kept from the C-home placeholder; nothing links against it. */
void agent_chat_completion_wait_notice_port_pending(void) { (void)0; }
