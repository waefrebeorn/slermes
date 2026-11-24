/*
 * primary_runtime_snapshot.c — C port of upstream Hermes agent/primary_runtime_snapshot.py.
 *
 * PoP: w18_primaryrun_7e6e15_take @ agent/primary_runtime_snapshot.py:_snapshot_primary_runtime
 * PoP: w18_primaryrun_7e6e15_build @ agent/primary_runtime_snapshot.py:_build_primary_runtime_snapshot
 *
 * SNAPSHOT_OWNER_KEY == "custom_entry". The config-reading runtime_entry_owner()
 * resolver is a controlled seam: its result is supplied as `owner`. Every other
 * field (getattr defaults, dict() copies, anthropic-only keys) is ported verbatim.
 */
#include "noc_w18_primaryrun_7e6e15.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static json_t *field(const json_t *src, const char *k) {
    return (src && src->type == JSON_OBJECT) ? json_obj_get(src, k) : NULL;
}
static void set_copy(json_t *out, const char *k, const json_t *src, const char *sk) {
    json_t *v = field(src, sk);
    json_set(out, k, v ? json_copy(v) : json_null());
}
static void set_str_or(json_t *out, const char *k, const json_t *src, const char *sk, const char *def) {
    json_t *v = field(src, sk);
    if (v && (v->type == JSON_STRING || v->type == JSON_NULL)) json_set(out, k, json_copy(v));
    else json_set(out, k, json_string(def));
}
static void set_bool_or(json_t *out, const char *k, const json_t *src, const char *sk, bool def) {
    json_t *v = field(src, sk);
    json_set(out, k, (v && v->type == JSON_BOOL) ? json_copy(v) : json_bool(def));
}
/* dict(getattr(src, sk, {}) or {}) -> copy when truthy, else {} */
static void set_dict_or(json_t *out, const char *k, const json_t *src, const char *sk, bool null_when_falsy) {
    json_t *v = field(src, sk);
    if (truthy(v) && v->type == JSON_OBJECT) json_set(out, k, json_copy(v));
    else json_set(out, k, null_when_falsy ? json_null() : json_object());
}
/* getattr(src, sk, getattr(fb, fsk, ...)): a present key (even null) wins. */
static void set_inherit(json_t *out, const char *k, const json_t *src, const char *sk,
                        const json_t *fb, const char *fsk) {
    json_t *v = field(src, sk);
    if (v) json_set(out, k, json_copy(v));
    else set_copy(out, k, fb, fsk);
}
static void set_num_or(json_t *out, const char *k, const json_t *src, const char *sk, double def) {
    json_t *v = field(src, sk);
    if (v && (v->type == JSON_NUMBER || v->type == JSON_BOOL)) json_set(out, k, json_copy(v));
    else json_set(out, k, json_number(def));
}
static void common(json_t *out, const json_t *agent, const json_t *owner) {
    set_copy(out, "model", agent, "model");
    set_copy(out, "provider", agent, "provider");
    set_copy(out, "requested_provider", agent, "requested_provider");
    json_set(out, "custom_entry", owner ? json_copy(owner) : json_null());
    set_copy(out, "base_url", agent, "base_url");
    set_copy(out, "api_mode", agent, "api_mode");
    set_str_or(out, "api_key", agent, "api_key", "");
}
static void anthropic(json_t *out, const json_t *agent) {
    set_copy(out, "anthropic_api_key", agent, "_anthropic_api_key");
    set_copy(out, "anthropic_base_url", agent, "_anthropic_base_url");
    set_copy(out, "is_anthropic_oauth", agent, "_is_anthropic_oauth");
}
static bool is_anthropic(const json_t *agent) {
    const char *m = json_get_str(agent, "api_mode", "");
    return m && !strcmp(m, "anthropic_messages");
}

json_t *w18_primaryrun_7e6e15_take(const json_t *agent, const json_t *cc, const json_t *owner) {
    json_t *out = json_object();
    common(out, agent, owner);
    set_dict_or(out, "request_overrides", agent, "request_overrides", false);
    set_copy(out, "client_kwargs", agent, "_client_kwargs");
    set_copy(out, "use_prompt_caching", agent, "_use_prompt_caching");
    set_copy(out, "use_native_cache_layout", agent, "_use_native_cache_layout");
    set_bool_or(out, "reasoning_echo_flag", agent, "_reasoning_echo_flag", false);
    set_inherit(out, "compressor_model", cc, "model", agent, "model");
    set_inherit(out, "compressor_base_url", cc, "base_url", agent, "base_url");
    set_str_or(out, "compressor_api_key", cc, "api_key", "");
    set_inherit(out, "compressor_provider", cc, "provider", agent, "provider");
    set_num_or(out, "compressor_context_length", cc, "context_length", 0);
    set_num_or(out, "compressor_threshold_tokens", cc, "threshold_tokens", 0);
    if (is_anthropic(agent)) anthropic(out, agent);
    return out;
}

json_t *w18_primaryrun_7e6e15_build(const json_t *agent, const json_t *cc, const json_t *owner,
                                    const char *api_mode) {
    json_t *out = json_object();
    common(out, agent, owner);
    set_copy(out, "client_kwargs", agent, "_client_kwargs");
    set_copy(out, "use_prompt_caching", agent, "_use_prompt_caching");
    set_copy(out, "use_native_cache_layout", agent, "_use_native_cache_layout");
    set_dict_or(out, "reasoning_config", agent, "reasoning_config", true);
    set_bool_or(out, "reasoning_echo_flag", agent, "_reasoning_echo_flag", false);
    set_dict_or(out, "request_overrides", agent, "request_overrides", false);
    set_dict_or(out, "runtime_capabilities", agent, "runtime_capabilities", false);
    set_inherit(out, "compressor_model", cc, "model", agent, "model");
    set_inherit(out, "compressor_base_url", cc, "base_url", agent, "base_url");
    set_str_or(out, "compressor_api_key", cc, "api_key", "");
    set_inherit(out, "compressor_provider", cc, "provider", agent, "provider");
    set_num_or(out, "compressor_context_length", cc, "context_length", 0);
    set_inherit(out, "compressor_api_mode", cc, "api_mode", agent, "api_mode");
    set_num_or(out, "compressor_threshold_tokens", cc, "threshold_tokens", 0);
    if (api_mode && !strcmp(api_mode, "anthropic_messages")) anthropic(out, agent);
    return out;
}
