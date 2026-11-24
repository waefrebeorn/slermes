/*
 * free_tier_bootstrap.c - C home for upstream Hermes hermes_cli/free_tier_bootstrap.py.
 *
 * PoP: w18_freetierb_22d217_free_tier_route @ hermes_cli/free_tier_bootstrap.py:SetupRecord.free_tier_route
 * PoP: w18_freetierb_22d217_as_payload @ hermes_cli/free_tier_bootstrap.py:SetupRecord.as_payload
 * PoP: w18_freetierb_22d217_build_record @ hermes_cli/free_tier_bootstrap.py:_build_record
 *
 * The injected collaborators in _build_record (anon_auth state/failure,
 * free_inference_active, is_guest_state) and _resolve_inference are fixture
 * seams; Python truthiness of a JSON value is reproduced by ftb_truthy so an
 * empty state dict is correctly falsy. as_payload copies the record before
 * flattening failure so no borrowed node is ever freed.
 */
#include "noc_w18_freetierb_22d217.h"

#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int w18_freetierb_22d217_free_tier_route(int free_tier_account, const char *inference_provider) {
    return free_tier_account && inference_provider && !strcmp(inference_provider, "nous") ? 1 : 0;
}

struct json_t *w18_freetierb_22d217_as_payload(const struct json_t *record) {
    json_t *payload = (record && record->type == JSON_OBJECT) ? json_copy(record) : json_object();

    /* failure_copy is owned BEFORE json_obj_del frees the original in payload. */
    const json_t *failure = json_obj_get(payload, "failure");
    json_t *failure_copy = failure ? json_copy(failure) : json_object();
    json_obj_del(payload, "failure");
    if (failure_copy && failure_copy->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(failure_copy); ++i) {
            const char *k = json_object_get_key_at(failure_copy, i);
            json_t *v = json_object_get_at(failure_copy, i);
            if (k) json_set(payload, k, json_copy(v));
        }
    }
    json_free(failure_copy);

    const json_t *fta = json_obj_get(payload, "free_tier_account");
    const json_t *ip = json_obj_get(payload, "inference_provider");
    int route = (fta && fta->type == JSON_BOOL && fta->bool_val &&
                 ip && ip->type == JSON_STRING && ip->str_val && !strcmp(ip->str_val, "nous"));
    json_set(payload, "free_tier_route", json_bool(route));
    return payload;
}

static int ftb_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0;
    return 1;
}

/* str(failure.get("error") or "") for the scalar error values used here. */
static void ftb_error_str(const struct json_t *failure, char *out, size_t cap) {
    out[0] = '\0';
    if (!failure || failure->type != JSON_OBJECT) return;
    const json_t *e = json_obj_get(failure, "error");
    if (!ftb_truthy(e)) return;
    if (e->type == JSON_STRING) {
        snprintf(out, cap, "%s", e->str_val ? e->str_val : "");
    } else if (e->type == JSON_BOOL) {
        snprintf(out, cap, "%s", e->bool_val ? "True" : "False");
    } else if (e->type == JSON_NUMBER) {
        double d = e->num_val;
        if (d == floor(d) && fabs(d) < 9.0e18) snprintf(out, cap, "%lld", (long long)d);
        else snprintf(out, cap, "%g", d);
    }
}

struct json_t *w18_freetierb_22d217_build_record(int other,
    const struct json_t *state, const struct json_t *failure,
    int free_active, int is_guest, const char *inference_provider, double finished_at) {
    int st = ftb_truthy(state);
    int free_tier_account = st ? (free_active ? 1 : 0) : 0;
    int has_identity = st;
    int provider_configured = other || free_tier_account || (st && !is_guest);

    /* Python: failure = {} if state else (last_mint_failure() or {})
     * -- with an identity the mint failure is never consulted, so the injected
     * seam is ignored and the record carries an empty dict. */
    const json_t *failure_used = NULL;
    json_t *empty_failure = NULL;
    if (st) {
        empty_failure = json_object();
        failure_used = empty_failure;
    } else if (failure && failure->type == JSON_OBJECT && ftb_truthy(failure)) {
        failure_used = failure;
    } else {
        empty_failure = json_object();
        failure_used = empty_failure;
    }

    char err[4096];
    ftb_error_str(failure_used, err, sizeof(err));

    json_t *rec = json_object();
    json_set(rec, "provider_configured", json_bool(provider_configured ? 1 : 0));
    json_set(rec, "inference_provider", json_string(inference_provider ? inference_provider : ""));
    json_set(rec, "free_tier_account", json_bool(free_tier_account ? 1 : 0));
    json_set(rec, "has_identity", json_bool(has_identity ? 1 : 0));
    json_set(rec, "other_providers", json_bool(other ? 1 : 0));
    json_set(rec, "error", json_string(err));
    json_set(rec, "failure", json_copy(failure_used));
    json_set(rec, "finished_at", json_number(finished_at));
    json_free(empty_failure);
    return rec;
}
