/*
 * usage.c -- real C11 port of upstream Hermes
 *   persistence/sqlite/conversation/usage.py (module-level pure helper(s)).
 *
 * The repository class is a DB wrapper (refusals + store holder + SQL bodies) and is
 * not ported; ``_delta_kwargs`` is the pure mapping every queued delta goes through
 * and is ported verbatim, key order included.
 *
 * PoP: w19_usage_8ec762_delta_kwargs @ persistence/sqlite/conversation/usage.py:_delta_kwargs
 */
#include "hermes_json.h"
#include "noc_w19_usage_8ec762.h"

/* Object member lookup that tolerates a missing/non-object parent. */
static const json_t *oget(const json_t *obj, const char *key) {
    if (!obj || !key || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

/* Python dict-literal passthrough: the value itself when present and not None,
 * ``None`` (JSON null) otherwise.  The copy is owned by the caller. */
static json_t *cp(const json_t *v) {
    if (!v) return json_null();
    return json_copy(v);
}

json_t *w19_usage_8ec762_delta_kwargs(const json_t *delta) {
    json_t *out = json_object();
    const json_t *route = oget(delta, "route");
    const json_t *tokens = oget(delta, "tokens");
    if (route && route->type == JSON_NULL) route = NULL;   /* `route is not None` */
    if (tokens && tokens->type == JSON_NULL) tokens = NULL;

    /* tokens = delta.tokens; route = delta.route */
    json_set(out, "source", cp(oget(delta, "source")));
    json_set(out, "input_tokens", tokens ? cp(oget(tokens, "input_tokens")) : json_null());
    json_set(out, "output_tokens", tokens ? cp(oget(tokens, "output_tokens")) : json_null());
    json_set(out, "cache_read_tokens", tokens ? cp(oget(tokens, "cache_read_tokens")) : json_null());
    json_set(out, "cache_write_tokens", tokens ? cp(oget(tokens, "cache_write_tokens")) : json_null());
    json_set(out, "reasoning_tokens", tokens ? cp(oget(tokens, "reasoning_tokens")) : json_null());
    json_set(out, "estimated_cost_usd", cp(oget(delta, "estimated_cost_usd")));
    json_set(out, "actual_cost_usd", cp(oget(delta, "actual_cost_usd")));
    json_set(out, "cost_status", cp(oget(delta, "cost_status")));
    json_set(out, "cost_source", cp(oget(delta, "cost_source")));
    json_set(out, "pricing_version", cp(oget(delta, "pricing_version")));
    /* route.billing_* / route.model guarded by `if route is not None`, not by getattr. */
    json_set(out, "billing_provider", route ? cp(oget(route, "billing_provider")) : json_null());
    json_set(out, "billing_base_url", route ? cp(oget(route, "billing_base_url")) : json_null());
    json_set(out, "billing_mode", route ? cp(oget(route, "billing_mode")) : json_null());
    json_set(out, "model", route ? cp(oget(route, "model")) : json_null());
    json_set(out, "api_call_count", cp(oget(delta, "api_call_count")));
    json_set(out, "task", cp(oget(delta, "task")));
    json_set(out, "prompt_cache_clock", cp(oget(delta, "prompt_cache_clock")));
    return out;
}
