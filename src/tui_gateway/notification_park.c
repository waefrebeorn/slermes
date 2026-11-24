/*
 * notification_park.c — C home for upstream Hermes tui_gateway/notification_park.py.
 *
 * PoP: w18_notificatio_e552e1_parkable @ tui_gateway/notification_park.py:_parkable
 * PoP: w18_notificatio_e552e1_is_progress @ tui_gateway/notification_park.py:_is_progress
 * PoP: w18_notificatio_e552e1_key @ tui_gateway/notification_park.py:_key
 * PoP: w18_notificatio_e552e1_admit_locked @ tui_gateway/notification_park.py:_admit_locked
 *
 * MAX_PARKED=16, MAX_PARKED_PER_KEY=4, RETENTION_SECONDS=600.0 mirrored from
 * the Python module; admission state is passed in so the gate is deterministic.
 */
#include "noc_w18_notificatio_e552e1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARKED 16
#define MAX_PARKED_PER_KEY 4

static char *dupstr(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* Python truthiness for the JSON values the harness can carry. */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static const char *coerce_str(const json_t *v, char *buf, size_t cap) {
    if (!v || !py_truthy(v)) return "";
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER && cap >= 32 && v->num_val == (double)(long long)v->num_val) {
        snprintf(buf, cap, "%lld", (long long)v->num_val);
        return buf;
    }
    return "";
}

json_t *w18_notificatio_e552e1_parkable(const json_t *evt) {
    const json_t *tv = evt ? json_obj_get(evt, "type") : NULL;
    int delegation = tv && tv->type == JSON_STRING && !strcmp(tv->str_val, "async_delegation");
    int restored = py_truthy(evt ? json_obj_get(evt, "restored") : NULL);
    return json_bool(!(delegation && restored) ? true : false);
}

json_t *w18_notificatio_e552e1_is_progress(const json_t *evt) {
    char buf[64];
    const char *t = coerce_str(evt ? json_obj_get(evt, "type") : NULL, buf, sizeof(buf));
    if (!strcmp(t, "heartbeat")) return json_bool(true);
    return json_bool(!strncmp(t, "watch_", 6) ? true : false);
}

json_t *w18_notificatio_e552e1_key(const json_t *evt) {
    char buf[64];
    return json_string(coerce_str(evt ? json_obj_get(evt, "session_key") : NULL, buf, sizeof(buf)));
}

/* held event from a [deadline, evt] row */
static const json_t *held_of(const json_t *row) {
    if (!row || row->type != JSON_ARRAY || json_len(row) < 2) return NULL;
    return json_get(row, 1);
}

static const char *evt_key(const json_t *evt, char *buf, size_t cap) {
    return coerce_str(evt ? json_obj_get(evt, "session_key") : NULL, buf, sizeof(buf) > cap ? cap : sizeof(buf));
}

static int evt_progress(const json_t *evt) {
    char b1[64], b2[64];
    const char *t = coerce_str(evt ? json_obj_get(evt, "type") : NULL, b1, sizeof(b1));
    (void)b2;
    return !strcmp(t, "heartbeat") || !strncmp(t, "watch_", 6) ? 1 : 0;
}

json_t *w18_notificatio_e552e1_admit_locked(const json_t *state, const json_t *evt, const char *key) {
    size_t total = state ? json_len(state) : 0, i, j, keycount = 0;
    size_t progress_idx[MAX_PARKED];
    size_t np = 0, victim = (size_t)-1;
    json_t *out = json_object();
    json_t *displaced = json_array();
    int key_full, admitted = 0;
    const char *k = key ? key : "";

    for (i = 0; i < total && i < MAX_PARKED; ++i) {
        char kb[64];
        const json_t *held = held_of(json_get(state, i));
        if (held && !strcmp(evt_key(held, kb, sizeof(kb)), k)) keycount++;
    }
    key_full = (*k != '\0') && keycount >= MAX_PARKED_PER_KEY;

    if (!key_full && total < MAX_PARKED) {
        admitted = 1;
    } else if (!evt_progress(evt)) {
        for (i = 0; i < total && i < MAX_PARKED; ++i) {
            const json_t *held = held_of(json_get(state, i));
            if (held && evt_progress(held)) {
                char kb[64];
                progress_idx[np++] = i;
                if (!strcmp(evt_key(held, kb, sizeof(kb)), k)) break;
            }
        }
        /* Python: [i for i in progress if key matches] or ([] if key_full else progress).
         * The loop above stops at the first same-key progress row; that is the victim. */
        if (np > 0) {
            char kb[64];
            const json_t *held = held_of(json_get(state, progress_idx[np - 1]));
            if (held && !strcmp(evt_key(held, kb, sizeof(kb)), k)) {
                victim = progress_idx[np - 1];
            } else if (!key_full) {
                victim = progress_idx[0];
            }
        }
        if (victim != (size_t)-1) {
            const json_t *held = held_of(json_get(state, victim));
            admitted = 1;
            if (held) json_append(displaced, json_copy(held));
        }
    }

    json_set(out, "admitted", json_bool(admitted ? true : false));
    json_set(out, "displaced", displaced);
    return out;
}

