/*
 * credential_auth_recovery.c — C port of upstream Hermes agent/credential_auth_recovery.py.
 *
 * PoP: w18_credential_ed2097_nous_refreshed @ agent/credential_auth_recovery.py:nous_pool_refreshed_this_turn
 * PoP: w18_credential_ed2097_entry_for @ agent/credential_auth_recovery.py:_entry_for
 * PoP: w18_credential_ed2097_episode @ agent/credential_auth_recovery.py:_episode
 * PoP: w18_credential_ed2097_constants @ agent/credential_auth_recovery.py:TRANSIENT_AUTH_BACKOFF_S/_MAX_AUTH_REFRESH_ATTEMPTS/_ACTIVITY_TOUCH_S/_EPISODE_IDLE_S
 *
 * The time.sleep/network/agent-mutation paths (_retry_after_backoff,
 * recover_auth_failure, _interruptible_wait) are intentionally out of scope.
 * time.monotonic() is injected as `now` so episode state is deterministic.
 */
#include "noc_w18_credential_ed2097.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPISODE_IDLE_S 120.0

static bool truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default:          return false;
    }
}

static bool scalar_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    if (a->type == JSON_NULL) return true;
    if (a->type == JSON_STRING) return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
    if (a->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_NUMBER) return a->num_val == b->num_val;
    return false;
}

json_t *w18_credential_ed2097_nous_refreshed(const json_t *counts) {
    if (!counts || counts->type != JSON_ARRAY) return json_bool(false);
    for (size_t i = 0; i < json_len(counts); i++) {
        json_t *row = json_get(counts, i);
        if (!row || row->type != JSON_ARRAY || json_len(row) < 3) continue;
        json_t *prov = json_get(row, 0);
        if (prov && prov->type == JSON_STRING && prov->str_val && !strcmp(prov->str_val, "nous")) {
            /* state.get("refreshes") — NOT the truthiness of the state dict. */
            json_t *state = json_get(row, 2);
            json_t *refreshes = (state && state->type == JSON_OBJECT) ? json_obj_get(state, "refreshes") : NULL;
            if (truthy(refreshes)) return json_bool(true);
        }
    }
    return json_bool(false);
}

json_t *w18_credential_ed2097_entry_for(const json_t *entries, const char *credential_id,
                                        const char *api_key_hint) {
    if (!entries || entries->type != JSON_ARRAY) return json_null();
    bool have_cid = credential_id && credential_id[0];
    bool have_hint = api_key_hint && api_key_hint[0];
    if (have_cid) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const char *id = json_get_str(e, "id", NULL);
            if (id && !strcmp(id, credential_id)) return json_copy(e);
        }
    }
    if (have_hint) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            if (!e || e->type != JSON_OBJECT) continue;
            const char *key = json_get_str(e, "runtime_api_key", NULL);
            if (key && !strcmp(key, api_key_hint)) return json_copy(e);
        }
    }
    return json_null();
}

json_t *w18_credential_ed2097_episode(const json_t *counts, const char *provider,
                                      const json_t *entry_id, double now) {
    json_t *rows = (counts && counts->type == JSON_ARRAY) ? (json_t *)counts : json_array();
    json_t *prov = json_string(provider ? provider : "");
    json_t *eid = entry_id ? json_copy(entry_id) : json_null();

    long long found = -1;
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_ARRAY || json_len(row) < 3) continue;
        if (scalar_eq(json_get(row, 0), prov) && scalar_eq(json_get(row, 1), eid)) { found = (long long)i; break; }
    }
    bool reset = found < 0;
    if (found >= 0) {
        json_t *row = json_get(rows, (size_t)found);
        json_t *state = json_get(row, 2);
        double last = (state && state->type == JSON_OBJECT) ? json_get_num(state, "last", 0) : 0.0;
        if (now - last > EPISODE_IDLE_S) reset = true;
    }

    json_t *state;
    if (reset) {
        state = json_object();
        json_set(state, "refreshes", json_number(0));
        json_set(state, "waits", json_number(0));
    } else {
        json_t *row = json_get(rows, (size_t)found);
        state = json_copy(json_get(row, 2));
    }
    json_set(state, "last", json_number(now));

    json_t *out_rows = json_array();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        if ((long long)i == found) {
            json_t *nr = json_array();
            json_append(nr, json_copy(json_get(row, 0)));
            json_append(nr, json_copy(json_get(row, 1)));
            json_append(nr, json_copy(state));
            json_append(out_rows, nr);
        } else {
            json_append(out_rows, json_copy(row));
        }
    }
    if (reset && found < 0) {
        json_t *nr = json_array();
        json_append(nr, json_copy(prov));
        json_append(nr, json_copy(eid));
        json_append(nr, json_copy(state));
        json_append(out_rows, nr);
    }
    json_free(prov); json_free(eid);

    json_t *out = json_object();
    json_set(out, "counts", out_rows);
    json_set(out, "episode", state);
    return out;
}

json_t *w18_credential_ed2097_constants(void) {
    json_t *out = json_object();
    json_t *waits = json_array();
    json_append(waits, json_number(5.0));
    json_append(waits, json_number(15.0));
    json_append(waits, json_number(30.0));
    json_set(out, "TRANSIENT_AUTH_BACKOFF_S", waits);
    json_set(out, "_MAX_AUTH_REFRESH_ATTEMPTS", json_number(2));
    json_set(out, "_ACTIVITY_TOUCH_S", json_number(30.0));
    json_set(out, "_EPISODE_IDLE_S", json_number(120.0));
    return out;
}
