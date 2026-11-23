/*
 * safe_worker_policy.c — C home for upstream Hermes agent/safe_worker_policy.py.
 *
 * Faithful C11 ports of the one-shot worker policy: strict bool validation,
 * config-object validation, the bypass/ordinary distinction (a bypass always
 * binds ignore_user_config=True), and the fork guard.  Process-bound state
 * (already-bound, runtime imports, policy snapshot, pid) is injected through
 * the args object so the C side never reads live interpreter state.
 *
 * PoP: w10_safeworker_419ccd_bind_safe_worker_policy @ agent/safe_worker_policy.py:_bind_safe_worker_policy
 * PoP: w10_safeworker_419ccd_bind_worker_config @ agent/safe_worker_policy.py:_bind_worker_config
 * PoP: w10_safeworker_419ccd_current_policy @ agent/safe_worker_policy.py:_current_policy
 * PoP: w10_safeworker_419ccd_worker_config_snapshot @ agent/safe_worker_policy.py:worker_config_snapshot
 */
#include "noc_w10_safeworker_419ccd.h"

#include <string.h>

/* Shared tail of `_bind`: config must be an object; one-shot / pre-import gates. */
static const char *bind_gate(const json_t *a)
{
    if (!a || !json_is_object(json_obj_get(a, "config"))) return "type_config";
    if (json_get_bool(a, "already_bound", false)) return "runtime_already";
    if (json_get_bool(a, "runtime_imports", false)) return "runtime_imports";
    return NULL;
}

static json_t *bind_result(const char *error, int safe_mode, int ignore)
{
    json_t *out = json_object();
    json_set(out, "error", error ? json_string(error) : json_null());
    json_set(out, "ok", json_bool(error == NULL));
    json_set(out, "safe_mode", error ? json_null() : json_bool(safe_mode));
    json_set(out, "ignore_user_config", error ? json_null() : json_bool(ignore));
    return out;
}

/* PoP: w10_safeworker_419ccd_bind_safe_worker_policy @ agent/safe_worker_policy.py:_bind_safe_worker_policy */
json_t *w10_safeworker_419ccd_bind_safe_worker_policy(json_t *a)
{
    json_t *sm = a ? json_obj_get(a, "safe_mode") : NULL;
    json_t *iu = a ? json_obj_get(a, "ignore_user_config") : NULL;
    /* `type(x) is not bool` — numbers/strings are NOT coerced, unlike C truthiness. */
    if (!json_is_bool(sm) || !json_is_bool(iu))
        return bind_result("type_fields", 0, 0);
    int safe_mode = json_bool_value(sm) ? 1 : 0;
    int ignore_user_config = json_bool_value(iu) ? 1 : 0;
    if (!safe_mode && !ignore_user_config)
        return bind_result("value_ordinary", 0, 0);
    const char *err = bind_gate(a);
    if (err) return bind_result(err, 0, 0);
    /* `_bind(safe_mode, True, config)` — a bypass always latches ignore=True. */
    return bind_result(NULL, safe_mode, 1);
}

/* PoP: w10_safeworker_419ccd_bind_worker_config @ agent/safe_worker_policy.py:_bind_worker_config */
json_t *w10_safeworker_419ccd_bind_worker_config(json_t *a)
{
    const char *err = bind_gate(a);
    if (err) return bind_result(err, 0, 0);
    /* `_bind(False, False, config)` */
    return bind_result(NULL, 0, 0);
}

/* PoP: w10_safeworker_419ccd_current_policy @ agent/safe_worker_policy.py:_current_policy */
json_t *w10_safeworker_419ccd_current_policy(json_t *a)
{
    json_t *policy = a ? json_obj_get(a, "policy") : NULL;
    json_t *out = json_object();
    if (!json_is_object(policy)) {
        json_set(out, "error", json_null());
        json_set(out, "enabled", json_bool(false));
        return out;
    }
    json_t *pid = json_obj_get(policy, "pid");
    json_t *caller = a ? json_obj_get(a, "caller_pid") : NULL;
    long long p = json_is_number(pid) ? (long long)json_number_value(pid) : 0;
    long long c = json_is_number(caller) ? (long long)json_number_value(caller) : 0;
    if (p != c) {
        json_set(out, "error", json_string("fork"));
        json_set(out, "enabled", json_bool(false));
        return out;
    }
    int safe = json_get_bool(policy, "safe_mode", false) ? 1 : 0;
    json_set(out, "error", json_null());
    json_set(out, "enabled", json_bool(safe));
    return out;
}

/* PoP: w10_safeworker_419ccd_worker_config_snapshot @ agent/safe_worker_policy.py:worker_config_snapshot */
json_t *w10_safeworker_419ccd_worker_config_snapshot(json_t *a)
{
    json_t *policy = a ? json_obj_get(a, "policy") : NULL;
    if (!json_is_object(policy)) return json_null();
    json_t *cfg = json_obj_get(policy, "config");
    return cfg ? json_copy(cfg) : json_null();
}
