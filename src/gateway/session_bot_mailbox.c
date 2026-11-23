/*
 * session_bot_mailbox.c - C port of upstream Hermes gateway/session_bot_mailbox.py
 *
 * PoP: w14_sessionbot_ecd66b_write_receipt @ gateway/session_bot_mailbox.py:_write_receipt
 * PoP: w14_sessionbot_ecd66b_withdraw_pin @ gateway/session_bot_mailbox.py:_withdraw_pin
 * PoP: w14_sessionbot_ecd66b_requeue_pin @ gateway/session_bot_mailbox.py:_requeue_pin
 * PoP: w14_sessionbot_ecd66b_refuse_pin @ gateway/session_bot_mailbox.py:_refuse_pin
 * PoP: w14_sessionbot_ecd66b_remember_receipt @ gateway/session_bot_mailbox.py:remember_receipt
 *
 * Filesystem locking/IO are replaced by JSON seams: the live receipt is supplied
 * as `current` and the decisions (conflict / merge / pin transitions) are pure.
 */
#include "noc_w14_sessionbot_ecd66b.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const json_t *og(const json_t *o, const char *k) {
    return json_is_object(o) ? json_obj_get(o, k) : NULL;
}
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    return v->c.count > 0;
}
static int streq(const char *a, const char *b) {
    return a && b && !strcmp(a, b);
}
/* Deep JSON equality; a NULL pointer represents Python None. */
static int jeq(const json_t *a, const json_t *b) {
    if (!a && !b) return 1;
    if (!a) return b->type == JSON_NULL;
    if (!b) return a->type == JSON_NULL;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return streq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) if (!jeq(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                const char *k = a->c.keys[i];
                if (!jeq(json_obj_get(a, k), og(b, k))) return 0;
            }
            return 1;
        }
    }
    return 0;
}
static json_t *merged_record(const json_t *current, const json_t *record) {
    json_t *m = json_is_object(current) ? json_copy(current) : json_object();
    if (json_is_object(record)) {
        for (size_t i = 0; i < json_object_size(record); i++) {
            const char *k = json_object_get_key_at(record, i);
            if (k) json_set(m, k, json_copy(json_object_get_at(record, i)));
        }
    }
    return m;
}

json_t *w14_sessionbot_ecd66b_write_receipt(const json_t *current, const json_t *record,
                                            const json_t *expect_present, const json_t *expect) {
    json_t *out = json_object();
    if (truthy(expect_present) && !jeq(og(current, "status"), expect)) {
        json_set(out, "result", json_bool(0));
        json_set(out, "error", json_null());
        json_set(out, "wrote", json_null());
        return out;
    }
    const json_t *pid = og(current, "principal_id");
    if (truthy(pid)) {
        static const char *fields[] = {"profile_home", "session_id", "principal_id", "message", "author"};
        int bad = 0;
        for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++)
            if (!jeq(og(current, fields[i]), og(record, fields[i]))) { bad = 1; break; }
        const json_t *cv = og(current, "notification_category");
        const json_t *rv = og(record, "notification_category");
        const char *cs = cv ? (cv->type == JSON_STRING ? cv->str_val : NULL) : "result";
        const char *rs = rv ? (rv->type == JSON_STRING ? rv->str_val : NULL) : "result";
        if (!streq(cs, rs)) bad = 1;
        const json_t *ca = og(current, "admission_id");
        if (truthy(ca) && !jeq(ca, og(record, "admission_id"))) bad = 1;
        if (bad) {
            json_set(out, "result", json_null());
            json_set(out, "error", json_string("admission_conflict"));
            json_set(out, "wrote", json_null());
            return out;
        }
    }
    json_t *merged = merged_record(current, record);
    json_set(out, "result", json_bool(1));
    json_set(out, "error", json_null());
    if (!jeq(merged, current)) json_set(out, "wrote", json_copy(merged));
    else json_set(out, "wrote", json_null());
    json_free(merged);
    return out;
}

static int withdrawable(const json_t *current) {
    const json_t *st = og(current, "status");
    return current && current->type == JSON_OBJECT && st && st->type == JSON_STRING
        && !strcmp(st->str_val, "ambiguous") && !truthy(og(current, "admission_id"));
}

json_t *w14_sessionbot_ecd66b_withdraw_pin(const json_t *current) {
    return json_bool(withdrawable(current));
}

json_t *w14_sessionbot_ecd66b_requeue_pin(const json_t *current) {
    if (!withdrawable(current)) return json_null();
    if (!json_has(current, "owner")) return json_null();
    static const char *pin[] = {"profile_home", "session_id", "principal_id", "operator"};
    json_t *ticket = json_object();
    for (size_t i = 0; i < json_object_size(current); i++) {
        const char *k = json_object_get_key_at(current, i);
        int skip = 0;
        for (size_t j = 0; j < sizeof pin / sizeof pin[0]; j++) if (k && !strcmp(k, pin[j])) { skip = 1; break; }
        if (k && !skip) json_set(ticket, k, json_copy(json_object_get_at(current, i)));
    }
    json_set(ticket, "status", json_string("queued"));
    return ticket;
}

json_t *w14_sessionbot_ecd66b_refuse_pin(const json_t *current) {
    if (!withdrawable(current)) return json_null();
    json_t *rec = json_copy(current);
    json_set(rec, "status", json_string("failed"));
    json_set(rec, "refused", json_bool(1));
    json_set(rec, "reply", json_string(""));
    json_set(rec, "error", json_string("Bot Chat delivery was refused"));
    json_set(rec, "reason", json_string("agent_blocked"));
    return rec;
}

json_t *w14_sessionbot_ecd66b_remember_receipt(const json_t *index, const json_t *home,
                                               const json_t *record) {
    json_t *out = json_is_object(index) ? json_copy(index) : json_object();
    const char *did = NULL;
    const json_t *dv = og(record, "delivery_id");
    if (dv && dv->type == JSON_STRING) did = dv->str_val;
    if (!did) return out;
    const json_t *st = og(record, "status");
    const char *s = st && st->type == JSON_STRING ? st->str_val : "";
    if (!strcmp(s, "settled") || !strcmp(s, "failed") || !strcmp(s, "cancelled")) {
        json_obj_del(out, did);
    } else {
        json_t *pair = json_array();
        json_append(pair, json_is_string(home) ? json_copy(home) : json_string(""));
        const json_t *sid = og(record, "session_id");
        json_append(pair, sid ? json_copy(sid) : json_null());
        json_set(out, did, pair);
    }
    return out;
}
