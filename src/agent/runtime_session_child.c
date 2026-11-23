/*
 * runtime_session_child.c — faithful C11 port of agent/runtime_session_child.py.
 * Gate w14_runtimeses_cd33f5.
 *
 * PoP: w14_runtimeses_cd33f5_refusal @ agent/runtime_session_child.py:_REFUSALS
 * PoP: w14_runtimeses_cd33f5_constructor_fields @ agent/runtime_session_child.py:ChildSessionStore.create_session
 * PoP: w14_runtimeses_cd33f5_outbox_name @ agent/runtime_session_child.py:reserve_child_store
 *
 * Random secret minting and RPC/event-loop transport are not ported; the three
 * deterministic transformations around them are.
 */
#include "noc_w14_runtimeses_cd33f5.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

const char *w14_runtimeses_cd33f5_refusal(const char *code) {
    if (!code) code = "";
    if (!strcmp(code, "delegation_capacity"))
        return "the gateway owner refused another subagent: this execution already runs "
               "delegation.max_concurrent_children of them";
    if (!strcmp(code, "delegation_depth_exceeded"))
        return "the gateway owner refused a subagent this deep (delegation.max_spawn_depth)";
    static char out[1024];
    snprintf(out, sizeof(out), "the gateway owner refused a subagent: %s", code);
    return out;
}

static int allowed(const struct json_t *keys, const char *k) {
    if (!keys || keys->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(keys); i++) {
        json_t *v = json_get(keys, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, k)) return 1;
    }
    return 0;
}

struct json_t *w14_runtimeses_cd33f5_constructor_fields(const struct json_t *fields,
        const struct json_t *allowed_keys) {
    json_t *out = json_object();
    const json_t *model = (fields && fields->type == JSON_OBJECT)
        ? json_obj_get(fields, "model") : NULL;
    const json_t *sp = (fields && fields->type == JSON_OBJECT)
        ? json_obj_get(fields, "system_prompt") : NULL;
    json_set(out, "model", model ? json_copy((json_t *)model) : json_null());
    json_set(out, "system_prompt", sp ? json_copy((json_t *)sp) : json_null());

    const json_t *cfg_in = (fields && fields->type == JSON_OBJECT)
        ? json_obj_get(fields, "model_config") : NULL;
    json_t *cfg = json_object();
    if (cfg_in && cfg_in->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(cfg_in); i++) {
            const char *k = json_object_get_key_at(cfg_in, i);
            json_t *v = json_object_get_at(cfg_in, i);
            if (k && allowed(allowed_keys, k)) json_set(cfg, k, json_copy(v));
        }
    }
    if (cfg->c.count == 0) { json_free(cfg); json_set(out, "model_config", json_null()); }
    else json_set(out, "model_config", cfg);
    return out;
}

const char *w14_runtimeses_cd33f5_outbox_name(const char *execution_id) {
    const char *s = execution_id ? execution_id : "";
    const char *last = strrchr(s, ':');
    const char *suffix = last ? last + 1 : s;
    static char out[2048];
    snprintf(out, sizeof(out), "child-%s", suffix);
    return out;
}
