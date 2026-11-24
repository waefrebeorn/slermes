/*
 * hosted_room_prepare_stall.c -- real C11 port of the stall bookkeeping of
 * upstream Hermes tui_gateway/hosted_room_prepare_stall.py.
 *
 * PoP: w21_hostedroom_0f248d_describe @ tui_gateway/hosted_room_prepare_stall.py:_describe
 * PoP: w21_hostedroom_0f248d_feed @ tui_gateway/hosted_room_prepare_stall.py:PrepareStallTracker.failed
 * PoP: w21_hostedroom_0f248d_retry_action @ tui_gateway/hosted_room_prepare_stall.py:retry_action
 * PoP: w21_hostedroom_0f248d_report_plan @ tui_gateway/hosted_room_prepare_stall.py:report_stall
 *
 * `PrepareStallTracker` is a threading.Lock around `{room_id: _Streak}`.  The
 * streak transitions are the whole state machine: `failed()` records the error
 * text on every call (even one it does not count), counts only when the
 * exception is neither transient nor a missing table and the room is not
 * already stalled, and returns True exactly once -- on the call that reaches
 * STALL_ATTEMPTS.  `stall()` (a failed explicit Retry) re-arms the room as
 * stalled at once without emitting a second room event.
 *
 * Left upstream: the store calls in `report_stall` (`room_state`, `read_events`,
 * `append_event`), the logger, and the module's exception imports
 * (`PermanentStoreError`, `TransientStoreError`, `AuthorityConflictError`, ...).
 * The driver's classification of an exception is the feed's `kind` input.
 */
#include "hermes_json.h"
#include "noc_w21_hostedroom_0f248d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* U+2026 HORIZONTAL ELLIPSIS, the module's truncation marker. */
#define HRPS_ELLIPSIS "\xE2\x80\xA6"

static char *hrps_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int hrps_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

char *w21_hostedroom_0f248d_describe(const char *message, const char *exc_type) {
    const char *s = message ? message : "";
    size_t start = 0, end = strlen(s);
    char *out;
    while (start < end && hrps_space((unsigned char)s[start])) start++;
    while (end > start && hrps_space((unsigned char)s[end - 1])) end--;
    if (end == start)
        return hrps_dup(exc_type && exc_type[0] ? exc_type : "");
    if (end - start <= W21_HOSTEDROOM_0F248D_MAX_ERROR_CHARS) {
        out = (char *)malloc(end - start + 1);
        if (!out) return NULL;
        memcpy(out, s + start, end - start);
        out[end - start] = '\0';
        return out;
    }
    /* text[:299] + the ellipsis: 300 characters in Python's count. */
    out = (char *)malloc((W21_HOSTEDROOM_0F248D_MAX_ERROR_CHARS - 1) + sizeof(HRPS_ELLIPSIS));
    if (!out) return NULL;
    memcpy(out, s + start, W21_HOSTEDROOM_0F248D_MAX_ERROR_CHARS - 1);
    memcpy(out + (W21_HOSTEDROOM_0F248D_MAX_ERROR_CHARS - 1), HRPS_ELLIPSIS, sizeof(HRPS_ELLIPSIS));
    return out;
}

typedef struct {
    int failures;
    char *error;
    int stalled;
} hrps_streak;

/* PrepareStallTracker.failed for one room. */
static int hrps_failed(hrps_streak *st, int transient, int missing_table,
                       const char *message, const char *exc_type) {
    char *text = w21_hostedroom_0f248d_describe(message, exc_type);
    free(st->error);
    st->error = text;
    if (transient || missing_table || st->stalled) return 0;
    st->failures += 1;
    st->stalled = st->failures >= W21_HOSTEDROOM_0F248D_STALL_ATTEMPTS;
    return st->stalled;
}

json_t *w21_hostedroom_0f248d_feed(const json_t *events) {
    hrps_streak st;
    json_t *out, *emitted;
    size_t i;
    st.failures = 0;
    st.error = NULL;
    st.stalled = 0;
    out = json_object();
    emitted = json_array();
    if (!out || !emitted) {
        json_free(out);
        json_free(emitted);
        return NULL;
    }
    if (events && events->type == JSON_ARRAY) {
        for (i = 0; i < events->c.count; i++) {
            json_t *ev = events->c.items[i];
            const char *op, *kind, *message, *type;
            if (!ev || ev->type != JSON_OBJECT) continue;
            op = json_get_str(ev, "op", "failed");
            kind = json_get_str(ev, "kind", "permanent");
            message = json_get_str(ev, "message", "");
            type = json_get_str(ev, "type", "");
            if (!strcmp(op, "clear")) {
                free(st.error);
                st.error = NULL;
                st.failures = 0;
                st.stalled = 0;
            } else if (!strcmp(op, "stall")) {
                free(st.error);
                st.error = w21_hostedroom_0f248d_describe(message, type);
                st.failures = W21_HOSTEDROOM_0F248D_STALL_ATTEMPTS;
                st.stalled = 1;
            } else {
                int transient = !strcmp(kind, "transient");
                int missing = !strcmp(kind, "missing_table");
                json_append(emitted, json_bool(hrps_failed(&st, transient, missing, message, type)));
            }
        }
    }
    json_set(out, "stalled", json_bool(st.stalled));
    json_set(out, "error", st.error ? json_string(st.error) : json_null());
    json_set(out, "failures", json_number((double)st.failures));
    json_set(out, "emitted", emitted);
    free(st.error);
    return out;
}

/* `m.get("handle") or m.get("member_id") or ""`, then str(). */
static int hrps_falsy(const json_t *v) {
    if (!v) return 1;
    switch (v->type) {
    case JSON_NULL:   return 1;
    case JSON_BOOL:   return v->bool_val ? 0 : 1;
    case JSON_NUMBER: return v->num_val == 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 0 : 1;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count == 0 ? 1 : 0;
    }
    return 1;
}

static char *hrps_member_name(const json_t *m) {
    json_t *v;
    if (!m || m->type != JSON_OBJECT) return hrps_dup("");
    v = json_obj_get(m, "handle");
    if (hrps_falsy(v)) v = json_obj_get(m, "member_id");
    if (hrps_falsy(v)) return hrps_dup("");
    if (v->type == JSON_STRING) return hrps_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return hrps_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return hrps_dup(buf);
    }
    return hrps_dup("");
}

json_t *w21_hostedroom_0f248d_retry_action(const char *room_id, const json_t *room) {
    json_t *out = json_object();
    json_t *members;
    char *joined;
    size_t i, len = 0;
    const char *rid = room_id ? room_id : "";
    if (!out) return NULL;
    members = (room && room->type == JSON_OBJECT) ? json_obj_get(room, "members") : NULL;
    joined = (char *)malloc(1);
    if (!joined) { json_free(out); return NULL; }
    joined[0] = '\0';
    if (members && members->type == JSON_ARRAY) {
        for (i = 0; i < members->c.count; i++) {
            char *name = hrps_member_name(members->c.items[i]);
            size_t nn = strlen(name ? name : "");
            if (nn) {
                size_t add = (len ? 2 : 0) + nn;
                char *grown = (char *)malloc(len + add + 1);
                if (grown) {
                    if (len) { memcpy(grown, joined, len); grown[len++] = ','; grown[len++] = ' '; }
                    memcpy(grown + len, name, nn);
                    len += nn;
                    grown[len] = '\0';
                    free(joined);
                    joined = grown;
                }
            }
            free(name);
        }
    }
    json_set(out, "kind", json_string("retry"));
    json_set(out, "member_id", json_string(joined[0] ? joined : rid));
    {
        size_t need = strlen(W21_HOSTEDROOM_0F248D_RETRY_TASK_PREFIX) + strlen(rid) + 1;
        char *task = (char *)malloc(need);
        if (task) {
            snprintf(task, need, "%s%s", W21_HOSTEDROOM_0F248D_RETRY_TASK_PREFIX, rid);
            json_set(out, "task_id", json_string(task));
            free(task);
        }
    }
    json_set(out, "execution_generation", json_number(1));
    free(joined);
    return out;
}

json_t *w21_hostedroom_0f248d_report_plan(const char *latest_seq, const char *tail_event_id,
                                          const char *message, const char *exc_type) {
    json_t *out = json_object();
    json_t *payload;
    char *descr, *event_id;
    size_t need, pneed;
    int already;
    const char *seq = latest_seq ? latest_seq : "0";
    if (!out) return NULL;
    already = tail_event_id &&
              !strncmp(tail_event_id, W21_HOSTEDROOM_0F248D_EVENT_PREFIX,
                       strlen(W21_HOSTEDROOM_0F248D_EVENT_PREFIX));
    need = strlen(W21_HOSTEDROOM_0F248D_EVENT_PREFIX) + strlen(seq) + 1;
    event_id = (char *)malloc(need);
    if (!event_id) { json_free(out); return NULL; }
    snprintf(event_id, need, "%s%s", W21_HOSTEDROOM_0F248D_EVENT_PREFIX, seq);
    descr = w21_hostedroom_0f248d_describe(message, exc_type);
    payload = json_object();
    pneed = strlen("Members could not be woken: ") + strlen(descr ? descr : "") + 1;
    {
        char *perr = (char *)malloc(pneed);
        if (perr) {
            snprintf(perr, pneed, "Members could not be woken: %s", descr ? descr : "");
            json_set(payload, "error", json_string(perr));
            free(perr);
        }
    }
    json_set(payload, "reason_code", json_string("prepare_failed"));
    json_set(out, "append", json_bool(already ? 0 : 1));
    json_set(out, "event_id", json_string(event_id));
    json_set(out, "kind", json_string("member.unavailable"));
    json_set(out, "payload", payload);
    free(descr);
    free(event_id);
    return out;
}
