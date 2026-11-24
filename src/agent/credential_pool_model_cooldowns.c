/*
 * credential_pool_model_cooldowns.c -- real C11 port of upstream Hermes
 * agent/credential_pool_model_cooldowns.py: the model-scoped rate-limit
 * cooldown policy of a pooled credential.
 *
 * PoP: w20_credential_86c0f9_pool_failure_reason @ agent/credential_pool_model_cooldowns.py:pool_failure_reason
 * PoP: w20_credential_86c0f9_model_cooldown_until @ agent/credential_pool_model_cooldowns.py:model_cooldown_until
 * PoP: w20_credential_86c0f9_merge_model_cooldowns @ agent/credential_pool_model_cooldowns.py:merge_model_cooldowns
 * PoP: w20_credential_86c0f9_is_model_scoped_failure @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin._is_model_scoped_failure
 * PoP: w20_credential_86c0f9_token_is_blocked @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin.token_is_blocked
 * PoP: w20_credential_86c0f9_cool_down_model @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin._cool_down_model
 * PoP: w20_credential_86c0f9_limit_state @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin.limit_state
 *
 * The pool (lock, entries tuple, auth.json persistence) stays upstream. The
 * clock, _exhausted_ttl, _normalize_error_context's reset_at and
 * _exhausted_until are injected: they are the only impure readers the policy
 * has.
 */
#include "hermes_json.h"
#include "noc_w20_credential_86c0f9.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CP86C0_MODEL_ENTITLEMENT_BENCH_SECONDS (365.0 * 24.0 * 60.0 * 60.0)
#define CP86C0_FAILURE_REASON_USAGE_WINDOW "usage_window"
#define CP86C0_FAILURE_REASON_BILLING "billing"
#define CP86C0_FAILURE_REASON_BILLING_UNVERIFIED "billing_unverified"
#define CP86C0_STATUS_DEAD "dead"

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int cp86c0_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* str(value) for the string fields this module compares. */
static const char *cp86c0_str(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

/* isinstance(value, (int, float)) -- a JSON bool IS an int in Python, so it
 * qualifies; a string does not. */
static int cp86c0_is_number_like(const json_t *v) {
    if (!v) return 0;
    return v->type == JSON_NUMBER || v->type == JSON_BOOL;
}

static double cp86c0_number_value(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return v->num_val;
}

double w20_credential_86c0f9_model_entitlement_bench_seconds(void) {
    return CP86C0_MODEL_ENTITLEMENT_BENCH_SECONDS;
}

const char *w20_credential_86c0f9_usage_window_reason(void) {
    return CP86C0_FAILURE_REASON_USAGE_WINDOW;
}

/* --- pool_failure_reason -------------------------------------------------- */

json_t *w20_credential_86c0f9_pool_failure_reason(const char *reason,
                                                  const json_t *error_context,
                                                  int billing_unverified) {
    const char *name = reason ? reason : "";
    if (!strcmp(name, CP86C0_FAILURE_REASON_BILLING)) {
        const json_t *flag = (error_context && error_context->type == JSON_OBJECT)
                                 ? json_obj_get((json_t *)error_context, "usage_window")
                                 : NULL;
        if (cp86c0_truthy(flag)) return json_string(CP86C0_FAILURE_REASON_USAGE_WINDOW);
        if (billing_unverified) return json_string(CP86C0_FAILURE_REASON_BILLING_UNVERIFIED);
    }
    return json_string(name);                    /* reason.value */
}

/* --- model_cooldown_until ------------------------------------------------- */

json_t *w20_credential_86c0f9_model_cooldown_until(const json_t *cooldowns,
                                                   const json_t *model, double now) {
    double best = 0.0;
    int found = 0;
    size_t i;

    if (!cooldowns || cooldowns->type != JSON_OBJECT) return json_null();  /* {} */
    if (cp86c0_truthy(model)) {
        json_t *value = NULL;
        const char *key = cp86c0_str(model);
        if (key) value = json_obj_get((json_t *)cooldowns, key);
        if (cp86c0_is_number_like(value) && cp86c0_number_value(value) > now) {
            return json_number(cp86c0_number_value(value));
        }
        return json_null();
    }
    for (i = 0; i < cooldowns->c.count; i++) {
        const json_t *value = cooldowns->c.items[i];
        if (!cp86c0_is_number_like(value)) continue;
        if (cp86c0_number_value(value) > now) {
            if (!found || cp86c0_number_value(value) > best) best = cp86c0_number_value(value);
            found = 1;
        }
    }
    return found ? json_number(best) : json_null();
}

/* --- merge_model_cooldowns ------------------------------------------------ */

json_t *w20_credential_86c0f9_merge_model_cooldowns(const json_t *maps) {
    json_t *merged = json_object();
    size_t m, i;
    if (!maps || maps->type != JSON_ARRAY) return merged;
    for (m = 0; m < maps->c.count; m++) {
        const json_t *snapshot = maps->c.items[m];
        if (!snapshot || snapshot->type != JSON_OBJECT) continue;   /* not a dict */
        for (i = 0; i < snapshot->c.count; i++) {
            const char *model = snapshot->c.keys[i];
            const json_t *until = snapshot->c.items[i];
            double value, seen;
            if (!model) continue;
            if (!cp86c0_is_number_like(until)) continue;
            value = cp86c0_number_value(until);
            seen = json_get_num(merged, model, 0.0);
            json_set(merged, model, json_number(value > seen ? value : seen));
        }
    }
    return merged;
}

/* --- _is_model_scoped_failure --------------------------------------------- */

int w20_credential_86c0f9_is_model_scoped_failure(const char *provider,
                                                  const json_t *status_code,
                                                  const json_t *model,
                                                  const json_t *failure_reason) {
    const char *reason = cp86c0_str(failure_reason);
    if (!cp86c0_truthy(model)) return 0;
    if (reason && (!strcmp(reason, "model_entitlement") ||
                   !strcmp(reason, CP86C0_FAILURE_REASON_USAGE_WINDOW))) {
        return 1;
    }
    if (!provider || strcmp(provider, "anthropic") != 0) return 0;
    if (!status_code || status_code->type != JSON_NUMBER || status_code->num_val != 429.0) return 0;
    if (reason && (!strcmp(reason, CP86C0_FAILURE_REASON_BILLING) ||
                   !strcmp(reason, CP86C0_FAILURE_REASON_BILLING_UNVERIFIED))) {
        return 0;
    }
    return 1;
}

/* --- token_is_blocked ----------------------------------------------------- */

int w20_credential_86c0f9_token_is_blocked(const json_t *entries, const char *token,
                                           const json_t *model, double now) {
    size_t i;
    if (!entries || entries->type != JSON_ARRAY) return 0;
    for (i = 0; i < entries->c.count; i++) {
        const json_t *entry = entries->c.items[i];
        const char *key;
        json_t *cooldowns;
        json_t *until;
        if (!entry || entry->type != JSON_OBJECT) continue;
        key = json_get_str((json_t *)entry, "runtime_api_key", NULL);
        if (!key || !token || strcmp(key, token) != 0) continue;
        cooldowns = json_obj_get((json_t *)entry, "model_cooldowns");
        until = w20_credential_86c0f9_model_cooldown_until(cooldowns, model, now);
        if (until && until->type != JSON_NULL) {
            json_free(until);
            return 1;
        }
        json_free(until);
    }
    return 0;
}

/* --- _cool_down_model ----------------------------------------------------- */

json_t *w20_credential_86c0f9_cool_down_model(const json_t *entries, int entry_index,
                                              const char *model, const json_t *reset_at,
                                              const char *failure_reason, double now,
                                              int sole_credential, double exhausted_ttl) {
    json_t *adopts = json_array();
    json_t *out = json_object();
    double until;
    const json_t *entry;
    const char *failed_key;

    if (!entries || entries->type != JSON_ARRAY || entry_index < 0 ||
        (size_t)entry_index >= entries->c.count) {
        json_set(out, "adopts", adopts);
        json_set(out, "persisted", json_bool(1));
        return out;
    }
    entry = entries->c.items[entry_index];

    if (failure_reason && !strcmp(failure_reason, "model_entitlement")) {
        until = now + CP86C0_MODEL_ENTITLEMENT_BENCH_SECONDS;
    } else if (reset_at && cp86c0_is_number_like(reset_at) && cp86c0_number_value(reset_at) != 0.0) {
        until = cp86c0_number_value(reset_at);
    } else {
        /* _normalize_error_context(...).get("reset_at") or now + _exhausted_ttl(429, sole) */
        until = now + exhausted_ttl;
    }
    (void)sole_credential;   /* already folded into the scripted exhausted_ttl */

    failed_key = json_get_str((json_t *)entry, "runtime_api_key", NULL);
    {
        size_t i;
        for (i = 0; i < entries->c.count; i++) {
            const json_t *scoped = entries->c.items[i];
            const char *scoped_key, *scoped_id, *entry_id;
            int same_id;
            json_t *snapshots, *merged, *adopt;
            if (!scoped || scoped->type != JSON_OBJECT) continue;
            scoped_id = json_get_str((json_t *)scoped, "id", NULL);
            entry_id = json_get_str((json_t *)entry, "id", NULL);
            scoped_key = json_get_str((json_t *)scoped, "runtime_api_key", NULL);
            same_id = (scoped_id && entry_id) ? (strcmp(scoped_id, entry_id) == 0)
                                              : (scoped_id == entry_id);
            /* `scoped.id != entry.id and not (failed_key and scoped.key == failed_key)` */
            if (!same_id && !(failed_key && failed_key[0] && scoped_key &&
                              strcmp(failed_key, scoped_key) == 0)) {
                continue;
            }
            snapshots = json_array();
            json_append(snapshots, json_copy(json_obj_get((json_t *)scoped, "model_cooldowns")));
            {
                json_t *fresh = json_object();
                json_set(fresh, model ? model : "", json_number(until));
                json_append(snapshots, fresh);
            }
            merged = w20_credential_86c0f9_merge_model_cooldowns(snapshots);
            json_free(snapshots);
            adopt = json_object();
            json_set(adopt, "index", json_number((double)i));
            json_set(adopt, "persist", json_bool(0));
            json_set(adopt, "model_cooldowns", merged);
            json_append(adopts, adopt);
        }
    }
    json_set(out, "adopts", adopts);
    json_set(out, "persisted", json_bool(1));
    return out;
}

/* --- limit_state ---------------------------------------------------------- */

json_t *w20_credential_86c0f9_limit_state(const json_t *entries, const json_t *models,
                                          double now) {
    json_t *live = json_array();
    json_t *benched = json_object();
    json_t *usable = json_array();
    json_t *cooled = json_object();
    json_t *out;
    size_t i;
    int sole;
    double account_reset = 0.0;

    if (!entries || entries->type != JSON_ARRAY) {
        /* No live row: nothing to report. */
        json_free(live);
        json_free(benched);
        json_free(usable);
        json_free(cooled);
        return json_null();
    }
    for (i = 0; i < entries->c.count; i++) {
        const json_t *entry = entries->c.items[i];
        const char *status;
        json_t *until;
        if (!entry || entry->type != JSON_OBJECT) continue;
        status = json_get_str((json_t *)entry, "last_status", NULL);
        if (status && !strcmp(status, CP86C0_STATUS_DEAD)) continue;
        json_append(live, json_copy((json_t *)entry));
        until = json_obj_get((json_t *)entry, "until");
        if (until && cp86c0_truthy(until) && cp86c0_is_number_like(until) &&
            cp86c0_number_value(until) > now) {
            const char *id = json_get_str((json_t *)entry, "id", "");
            json_set(benched, id ? id : "", json_number(cp86c0_number_value(until)));
        }
    }
    sole = live->c.count <= 1;
    (void)sole;   /* folded into the scripted "until" by the caller */

    if (live->c.count > 0 && benched->c.count == live->c.count) {
        size_t b;
        int first = 1;
        for (b = 0; b < benched->c.count; b++) {
            double v = benched->c.items[b]->num_val;
            if (first || v < account_reset) account_reset = v;
            first = 0;
        }
        out = json_object();
        json_set(out, "scope", json_string("account"));
        json_set(out, "resets_at", json_number(account_reset));
        json_free(live);
        json_free(benched);
        json_free(usable);
        json_free(cooled);
        return out;
    }

    for (i = 0; i < live->c.count; i++) {
        const json_t *entry = live->c.items[i];
        const char *id = json_get_str((json_t *)entry, "id", "");
        if (id && json_obj_get(benched, id)) continue;   /* entry.id in benched */
        json_append(usable, json_copy((json_t *)entry));
    }

    if (models && models->type == JSON_ARRAY) {
        size_t m;
        for (m = 0; m < models->c.count; m++) {
            const json_t *model = models->c.items[m];
            double best = 0.0;
            int all = 1, any = 0;
            size_t u;
            for (u = 0; u < usable->c.count; u++) {
                const json_t *entry = usable->c.items[u];
                json_t *wait = w20_credential_86c0f9_model_cooldown_until(
                    json_obj_get((json_t *)entry, "model_cooldowns"), model, now);
                if (wait && wait->type != JSON_NULL) {
                    double v = wait->num_val;
                    if (!any || v < best) best = v;
                    any = 1;
                } else {
                    all = 0;
                }
                json_free(wait);
            }
            if (any && all && best - now < CP86C0_MODEL_ENTITLEMENT_BENCH_SECONDS / 2.0) {
                const char *key = cp86c0_str(model);
                if (key) json_set(cooled, key, json_number(best));
            }
        }
    }

    if (cooled->c.count == 0) {
        json_free(live);
        json_free(benched);
        json_free(usable);
        json_free(cooled);
        return json_null();
    }
    out = json_object();
    json_set(out, "scope", json_string("models"));
    json_set(out, "models", cooled);
    json_free(live);
    json_free(benched);
    json_free(usable);
    return out;
}
