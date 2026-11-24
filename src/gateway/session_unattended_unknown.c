/*
 * session_unattended_unknown.c - C11 port of upstream
 * gateway/session_unattended_unknown.py (gate w23_sessionuna_84b661).
 *
 * PoP: w23_sessionuna_84b661_is_unattended @ gateway/session_unattended_unknown.py:is_unattended
 * PoP: w23_sessionuna_84b661_unattended_unknown @ gateway/session_unattended_unknown.py:_unattended_unknown
 * PoP: w23_sessionuna_84b661_settle_recovered @ gateway/session_unattended_unknown.py:settle_recovered_unattended
 * PoP: w23_sessionuna_84b661_settle_drain @ gateway/session_unattended_unknown.py:settle_unattended_unknown
 *
 * A turn fenced ``unknown`` across an owner restart pauses its session's FIFO
 * because only its sender knows whether to resend it.  An automation delivery
 * has no sender, so the boot recovery (and the drain that meets an unknown head)
 * settles it ``interrupted`` through the same fenced resolution a person's
 * Discard uses, and releases its media.  The store, the notices and the media
 * release are seams: the caller supplies the rows and which resolutions the
 * store refuses; this file owns the filter, the refusal ladder and the settle
 * order.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_sessionuna_84b661.h"

const char *w23_sessionuna_84b661_cron_principal(void) {
    return W23_SESSIONUNA_84B661_CRON_PRINCIPAL;
}

const char *w23_sessionuna_84b661_notice(void) {
    return W23_SESSIONUNA_84B661_NOTICE;
}

/* row['status'] == 'unknown' (a bare equality: no coercion). */
static int su_status_unknown(const json_t *row) {
    const json_t *v = json_obj_get(row, "status");
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, "unknown") == 0;
}

/* '<key>' in payload: key membership, whatever the value is. */
static int su_payload_key(const json_t *payload, const char *key) {
    if (!payload || payload->type != JSON_OBJECT) return 0;
    return json_obj_get(payload, key) != NULL;
}

/* '<key>' in payload['native_text_v1'] or {}: key membership of an object,
 * substring of a TEXT cell, element of a list -- Python's ``in``. */
static int su_native_has(const json_t *payload, const char *key) {
    const json_t *native;
    size_t i, n;

    if (!payload || payload->type != JSON_OBJECT) return 0;
    native = json_obj_get(payload, "native_text_v1");
    if (!native || native->type == JSON_NULL) return 0;
    if (native->type == JSON_OBJECT) return json_obj_get(native, key) != NULL;
    if (native->type == JSON_STRING)
        return native->str_val && strstr(native->str_val, key) != NULL;
    if (native->type == JSON_ARRAY) {
        n = json_len(native);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(native, i);
            if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, key) == 0)
                return 1;
        }
    }
    return 0;
}

static int su_is_unattended_row(const json_t *row) {
    const json_t *principal_node, *payload;
    const char *principal = "";

    if (!row || row->type != JSON_OBJECT) return 0;
    principal_node = json_obj_get(row, "principal_id");
    if (principal_node && principal_node->type == JSON_STRING && principal_node->str_val)
        principal = principal_node->str_val;
    if (strcmp(principal, W23_SESSIONUNA_84B661_CRON_PRINCIPAL) == 0) return 1;
    if (strncmp(principal, "automation:", sizeof("automation:") - 1) == 0) return 1;

    payload = json_obj_get(row, "payload");
    if (!payload || payload->type != JSON_OBJECT) return 0;
    return su_payload_key(payload, "local_automation_v1") ||
           su_payload_key(payload, "kanban_card_v1") ||
           su_native_has(payload, "automation") ||
           su_native_has(payload, "webhook_delivery");
}

int w23_sessionuna_84b661_is_unattended(const json_t *row) {
    return su_is_unattended_row(row);
}

/* The rows the module maps admission_row over and keeps: unknown and unattended. */
static int su_keeps(const json_t *row) {
    return row && row->type == JSON_OBJECT && su_status_unknown(row) &&
           su_is_unattended_row(row);
}

static void su_put_copy(json_t *obj, const char *key, const json_t *v) {
    json_set(obj, key, v ? json_copy(v) : json_null());
}

json_t *w23_sessionuna_84b661_unattended_unknown(const json_t *records) {
    json_t *out = json_array();
    size_t i, n;

    if (!out) return NULL;
    n = records && records->type == JSON_ARRAY ? json_len(records) : 0;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(records, i);
        const json_t *aid;
        if (!su_keeps(row)) continue;
        aid = json_obj_get(row, "admission_id");
        json_append(out, aid ? json_copy(aid) : json_null());
    }
    return out;
}

/* One resolved call: the kwargs the store really receives. */
static json_t *su_call(const json_t *row, const json_t *epoch, int with_epoch) {
    json_t *call = json_object();
    if (!call) return NULL;
    su_put_copy(call, "admission_id", json_obj_get(row, "admission_id"));
    su_put_copy(call, "generation", json_obj_get(row, "generation"));
    if (with_epoch) su_put_copy(call, "epoch", epoch);
    json_set(call, "notice", json_string(W23_SESSIONUNA_84B661_NOTICE));
    return call;
}

static const json_t *su_rows(const json_t *req) {
    return req && req->type == JSON_OBJECT ? json_obj_get(req, "rows") : NULL;
}

json_t *w23_sessionuna_84b661_settle_recovered(const json_t *req) {
    const json_t *rows = su_rows(req);
    const json_t *epoch = req && req->type == JSON_OBJECT ? json_obj_get(req, "epoch") : NULL;
    json_t *out = json_object();
    json_t *resolved = json_array(), *released = json_array(), *refused = json_array();
    size_t i, n;
    long settled = 0;

    if (!out || !resolved || !released || !refused) {
        json_free(out);
        return NULL;
    }
    n = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *aid;
        if (!su_keeps(row)) continue;
        aid = json_obj_get(row, "admission_id");
        if (json_get_bool(row, "refuse", 0)) {
            json_append(refused, aid ? json_copy(aid) : json_null());
            continue;
        }
        json_append(resolved, su_call(row, epoch, 1));
        json_append(released, aid ? json_copy(aid) : json_null());
        settled++;
    }
    json_set(out, "settled", json_number((double)settled));
    json_set(out, "resolved", resolved);
    json_set(out, "released", released);
    json_set(out, "refused", refused);
    return out;
}

json_t *w23_sessionuna_84b661_settle_drain(const json_t *req) {
    const json_t *rows = su_rows(req);
    const json_t *session_id = req && req->type == JSON_OBJECT ? json_obj_get(req, "session_id") : NULL;
    json_t *out = json_object();
    json_t *resolved = json_array(), *refused = json_array(), *read = json_array();
    size_t i, n;
    int settled = 0;

    if (!out || !resolved || !refused || !read) {
        json_free(out);
        return NULL;
    }
    /* the FIFO read is scoped to the head's session (asyncio.to_thread(...list)). */
    json_append(read, session_id ? json_copy(session_id) : json_null());
    n = rows && rows->type == JSON_ARRAY ? json_len(rows) : 0;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *aid;
        if (!su_keeps(row)) continue;
        aid = json_obj_get(row, "admission_id");
        if (json_get_bool(row, "refuse", 0)) {
            json_append(refused, aid ? json_copy(aid) : json_null());
            continue;
        }
        json_append(resolved, su_call(row, NULL, 0));
        settled = 1;
    }
    json_set(out, "settled", json_bool(settled));
    json_set(out, "resolved", resolved);
    json_set(out, "refused", refused);
    json_set(out, "session_ids", read);
    return out;
}
