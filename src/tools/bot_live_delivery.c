/*
 * bot_live_delivery.c — C home for upstream Hermes tools/bot_live_delivery.py.
 *
 * PoP: w9_botlivedel_dad0de_delivery_id @ tools/bot_live_delivery.py:_delivery_id
 * PoP: w9_botlivedel_dad0de_ticket_shape_error @ tools/bot_live_delivery.py:_ticket_shape_error
 *
 * Both exports are the pure validation decisions of the module; no gateway,
 * filesystem or lock machinery is involved.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w9_botlivedel_dad0de.h"

static const char *const OWNER_KEYS[] = {
    "profile_home", "session_id", "lease_id", "live_session_id"
};

static json_t *err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}

struct json_t *w9_botlivedel_dad0de_delivery_id(const char *value) {
    const char *s = value ? value : "";
    size_t n = strlen(s);
    if (n < 32 || n > 64) goto bad;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) goto bad;
    return json_string(s);
bad:
    return err("delivery id must be 32 to 64 lowercase hex characters");
}

/* Python isinstance(v, int) and not isinstance(v, bool).  The fixture passes
 * 64-bit integers as JSON numbers; bool and string are distinct JSON types. */
static int is_py_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && v->num_val == (double)(long long)v->num_val;
}

/* Limited Python repr() for the status values used by the ticket scans. */
static void py_repr(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:
            snprintf(out, cap, "%s", v->bool_val ? "True" : "False");
            return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(out, cap, "%lld", (long long)v->num_val);
            else
                snprintf(out, cap, "%g", v->num_val);
            return;
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            int dq = strchr(s, '\'') && !strchr(s, '"');
            snprintf(out, cap, "%c%s%c", dq ? '"' : '\'', s, dq ? '"' : '\'');
            return;
        }
        default:
            snprintf(out, cap, "?");
            return;
    }
}

static int str_is(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && !strcmp(v->str_val ? v->str_val : "", s);
}

static int known_status(const json_t *v) {
    static const char *const ok[] = {
        "queued", "claimed", "settled", "failed", "cancelled", "ambiguous"
    };
    if (!v || v->type != JSON_STRING) return 0;
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++)
        if (!strcmp(v->str_val ? v->str_val : "", ok[i])) return 1;
    return 0;
}

struct json_t *w9_botlivedel_dad0de_ticket_shape_error(const char *stem,
                                                      const struct json_t *record) {
    if (!record || record->type != JSON_OBJECT) return json_string("id does not match filename");
    json_t *owner = json_obj_get(record, "owner");
    json_t *created = json_obj_get(record, "created_at");
    json_t *seq = json_obj_get(record, "sequence");
    if (!seq) seq = created;  /* record.get("sequence", record.get("created_at")) */

    json_t *did = json_obj_get(record, "delivery_id");
    json_t *rid = json_obj_get(record, "id");
    if (!str_is(did, stem ? stem : "") || !str_is(rid, stem ? stem : ""))
        return json_string("id does not match filename");

    json_t *status = json_obj_get(record, "status");
    if (!known_status(status)) {
        char repr[1024];
        py_repr(status, repr, sizeof(repr));
        char msg[1200];
        snprintf(msg, sizeof(msg), "unknown status %s", repr);
        return json_string(msg);
    }
    if (!is_py_int(created) || !is_py_int(seq))
        return json_string("created_at/sequence are not integers");

    if (!owner || owner->type != JSON_OBJECT) return json_string("owner pin is incomplete");
    for (size_t i = 0; i < sizeof(OWNER_KEYS) / sizeof(OWNER_KEYS[0]); i++) {
        json_t *v = json_obj_get(owner, OWNER_KEYS[i]);
        if (!v || v->type != JSON_STRING || !(v->str_val && *v->str_val))
            return json_string("owner pin is incomplete");
    }
    return json_null();
}

void tools_bot_live_delivery_port_pending(void) { (void)0; }
