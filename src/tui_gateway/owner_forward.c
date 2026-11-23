/*
 * owner_forward.c — C home for upstream Hermes tui_gateway/owner_forward.py.
 *
 * PoP: ownerforw_forwardable @ tui_gateway/owner_forward.py:forwardable
 * PoP: ownerforw_discover_owner @ tui_gateway/owner_forward.py:discover_owner
 * PoP: ownerforw_payload_for @ tui_gateway/owner_forward.py:_payload_for
 *
 * Deterministic C11 ports; external dependencies are explicit fixture seams.
 */
#include "noc_w13_ownerforwa_a9e844.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}
static int is_ws(char c) { return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v'; }
static int blank_str(const char *s) {
    if (!s) return 1;
    for (; *s; ++s) if (!is_ws(*s)) return 0;
    return 1;
}

struct json_t *ownerforw_forwardable(const struct json_t *refusal, const struct json_t *session,
                                     const struct json_t *text, const struct json_t *params) {
    int ok = 1;
    const char *ls = (refusal && refusal->type == JSON_OBJECT)
                     ? json_get_str(refusal, "ledger_status", NULL) : NULL;
    static const char *STATES[] = {"started","queued","registered","running",NULL};
    int in_states = 0;
    if (ls) for (int i = 0; STATES[i]; ++i) if (!strcmp(ls, STATES[i])) { in_states = 1; break; }
    if (!in_states) ok = 0;
    if (ok && (!text || text->type != JSON_STRING || blank_str(text->str_val))) ok = 0;
    if (ok) {
        json_t *img = session ? json_obj_get(session, "attached_images") : NULL;
        if (py_truthy(img)) ok = 0;
    }
    if (ok && params && params->type == JSON_OBJECT) {
        static const char *KEYS[] = {"truncate_before_user_ordinal","truncate_before_row_id",
                                     "truncate_before_message_id",NULL};
        for (int i = 0; KEYS[i]; ++i) {
            json_t *v = json_obj_get(params, KEYS[i]);
            if (v && v->type != JSON_NULL) { ok = 0; break; }
        }
    }
    return json_bool(ok ? true : false);
}

struct json_t *ownerforw_discover_owner(const struct json_t *state, const struct json_t *endpoint) {
    const char *s = (state && state->type == JSON_STRING) ? state->str_val : "";
    int ready = !strcmp(s, "ready");
    if (!ready || !endpoint || endpoint->type == JSON_NULL) {
        json_t *out = json_object();
        char buf[256];
        snprintf(buf, sizeof(buf), "the gateway that owns it is %s", s);
        json_set_hdr(out, "error", buf);
        return out;
    }
    json_t *out = json_object();
    json_set(out, "endpoint", json_copy(endpoint));
    return out;
}

static int field_allowed(const struct json_t *fields, const char *key) {
    if (!fields || fields->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(fields); ++i) {
        json_t *v = json_get(fields, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, key)) return 1;
    }
    return 0;
}

struct json_t *ownerforw_payload_for(const char *event_type, const struct json_t *payload,
                                     const struct json_t *contract_fields, const struct json_t *valid) {
    if (!event_type || !payload || payload->type != JSON_OBJECT) return json_null();
    const struct json_t *fields = (contract_fields && contract_fields->type == JSON_OBJECT)
                                  ? json_obj_get(contract_fields, event_type) : NULL;
    if (!fields || fields->type != JSON_ARRAY) return json_null(); /* no contract / no payload model */
    json_t *shaped = json_object();
    for (size_t i = 0; i < json_object_size(payload); ++i) {
        const char *k = json_object_get_key_at(payload, i);
        if (k && field_allowed(fields, k))
            json_set(shaped, k, json_copy(json_object_get_at(payload, i)));
    }
    int is_valid = py_truthy(valid);
    if (!is_valid) {
        if (strcmp(event_type, "message.complete")) { json_free(shaped); return json_null(); }
        json_free(shaped);
        shaped = json_object();
        json_t *t = json_obj_get(payload, "text");
        if (t && t->type == JSON_STRING) json_set(shaped, "text", json_copy(t));
        else json_set_hdr(shaped, "text", "");
    }
    return shaped;
}
