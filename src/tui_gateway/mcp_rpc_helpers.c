/*
 * mcp_rpc_helpers.c — faithful C11 port of tui_gateway/mcp_rpc_helpers.py.
 * PoP: w15_mcprpchel_bbb1c7_summarize_server             @ tui_gateway/mcp_rpc_helpers.py:summarize_server
 * PoP: w15_mcprpchel_bbb1c7_server_configs_with_sources  @ tui_gateway/mcp_rpc_helpers.py:server_configs_with_sources
 * PoP: w15_mcprpchel_bbb1c7_record_mcp_add               @ tui_gateway/mcp_rpc_helpers.py:record_mcp_add
 *
 * The live helpers read plugin discovery, OAuth state and the install catalog.
 * Those reads are injected as resolved facts (see header); the projections,
 * transport/auth detection, sorted env keys and telemetry fields are real.
 */
#include "noc_w15_mcprpchel_bbb1c7.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:  return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && *v->str_val;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

static json_t *value_or_null(const json_t *v) {
    return v && v->type != JSON_NULL ? json_copy(v) : json_null();
}

static const char *jstr_or_null(const json_t *v) {
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

json_t *w15_mcprpchel_bbb1c7_summarize_server(const char *name, const json_t *cfg,
        const char *plugin, int oauth_tokens_present, int server_enabled) {
    const json_t *c = (cfg && cfg->type == JSON_OBJECT) ? cfg : NULL;
    const char *transport = "unknown";
    if (c && jtruthy(json_obj_get(c, "url"))) transport = "http";
    else if (c && jtruthy(json_obj_get(c, "command"))) transport = "stdio";

    json_t *auth_v = c ? json_obj_get(c, "auth") : NULL;
    json_t *auth = value_or_null(auth_v);
    json_t *headers = c ? json_obj_get(c, "headers") : NULL;
    if (!jtruthy(auth_v) && headers && headers->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(headers); i++) {
            const char *k = json_object_get_key_at(headers, i);
            if (!k) continue;
            char low[256];
            size_t n = strlen(k);
            if (n >= sizeof(low)) n = sizeof(low) - 1;
            for (size_t j = 0; j < n; j++) low[j] = (char)tolower((unsigned char)k[j]);
            low[n] = '\0';
            if (!strcmp(low, "authorization")) {
                json_free(auth);
                auth = json_string("header");
                break;
            }
        }
    }

    json_t *args = json_array();
    if (c) {
        json_t *a = json_obj_get(c, "args");
        if (a && a->type == JSON_ARRAY) {
            for (size_t i = 0; i < a->c.count; i++) json_append(args, json_copy(a->c.items[i]));
        }
    }

    /* env: sorted(str(k) for k in (cfg.get("env") or {})) */
    json_t *env = json_array();
    if (c) {
        json_t *e = json_obj_get(c, "env");
        if (e && (e->type == JSON_OBJECT || e->type == JSON_ARRAY) && e->c.count) {
            const char **keys = (const char **)malloc(e->c.count * sizeof(const char *));
            size_t nk = 0;
            if (keys) {
                if (e->type == JSON_OBJECT) {
                    for (size_t i = 0; i < json_object_size(e); i++) keys[nk++] = json_object_get_key_at(e, i);
                } else {
                    for (size_t i = 0; i < e->c.count; i++) {
                        json_t *v = e->c.items[i];
                        if (v && v->type == JSON_STRING) keys[nk++] = v->str_val;
                    }
                }
                for (size_t i = 1; i < nk; i++) {
                    const char *tmp = keys[i];
                    size_t j = i;
                    while (j > 0 && strcmp(keys[j - 1], tmp) > 0) { keys[j] = keys[j - 1]; j--; }
                    keys[j] = tmp;
                }
                for (size_t i = 0; i < nk; i++) json_append(env, json_string(keys[i]));
                free(keys);
            }
        }
    }

    int is_oauth = auth && auth->type == JSON_STRING && !strcmp(auth->str_val, "oauth");
    json_t *out = json_object();
    json_set(out, "name", json_string(name ? name : ""));
    json_set(out, "transport", json_string(transport));
    json_set(out, "url", c ? value_or_null(json_obj_get(c, "url")) : json_null());
    json_set(out, "command", c ? value_or_null(json_obj_get(c, "command")) : json_null());
    json_set(out, "args", args);
    json_set(out, "env", env);
    json_set(out, "auth", auth);
    json_set(out, "oauth_tokens_present",
             is_oauth ? json_bool(oauth_tokens_present != 0) : json_null());
    json_set(out, "enabled", json_bool(server_enabled != 0));
    json_set(out, "tools", c ? value_or_null(json_obj_get(c, "tools")) : json_null());
    json_set(out, "source", json_string(plugin ? "plugin" : "config"));
    json_set(out, "plugin", plugin ? json_string(plugin) : json_null());
    return out;
}

json_t *w15_mcprpchel_bbb1c7_server_configs_with_sources(const json_t *config_servers,
        const json_t *portable, const json_t *owners) {
    json_t *servers = json_object();
    json_t *plugins = json_object();
    if (config_servers && config_servers->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(config_servers); i++) {
            const char *k = json_object_get_key_at(config_servers, i);
            json_t *v = json_object_get_at(config_servers, i);
            if (!k || !v || v->type != JSON_OBJECT) continue;
            json_set(servers, k, json_copy(v));
            json_set(plugins, k, json_null());
        }
    }
    if (portable && portable->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(portable); i++) {
            const char *k = json_object_get_key_at(portable, i);
            json_t *v = json_object_get_at(portable, i);
            if (!k || !v || json_obj_get(servers, k)) continue;
            json_set(servers, k, json_copy(v));
            json_t *owner = owners && owners->type == JSON_OBJECT ? json_obj_get(owners, k) : NULL;
            json_set(plugins, k, owner && owner->type == JSON_STRING
                     ? json_copy(owner) : json_null());
        }
    }
    json_t *out = json_object();
    json_set(out, "servers", servers);
    json_set(out, "plugins", plugins);
    return out;
}

json_t *w15_mcprpchel_bbb1c7_record_mcp_add(int entry_present, const char *entry_name,
        const json_t *server_config, int saved) {
    const char *source = "local";
    if (entry_present) source = "catalog";
    else if (server_config && server_config->type == JSON_OBJECT &&
             jtruthy(json_obj_get(server_config, "url"))) source = "url";
    json_t *out = json_object();
    json_set(out, "source", json_string(source));
    json_set(out, "name", entry_present ? json_string(entry_name ? entry_name : "") : json_null());
    json_set(out, "status", json_string(saved ? "success" : "failed"));
    json_set(out, "failure_class", saved ? json_null() : json_string("config_rejected"));
    return out;
}
