/*
 * config_providers.c — C port of upstream Hermes hermes_cli/config_providers.py.
 * Gate: w15_configprov_34dd3f
 *
 * PoP: w15_configprov_34dd3f_canonical_api_mode @ hermes_cli/config_providers.py:_canonical_api_mode
 * PoP: w15_configprov_34dd3f_coerce_provider_id @ hermes_cli/config_providers.py:coerce_provider_id
 * PoP: w15_configprov_34dd3f_stringify_provider_map @ hermes_cli/config_providers.py:stringify_provider_map
 * PoP: w15_configprov_34dd3f_find_provider_entry @ hermes_cli/config_providers.py:find_provider_entry
 * PoP: w15_configprov_34dd3f_normalize_provider_models @ hermes_cli/config_providers.py:_normalize_provider_models
 * PoP: w15_configprov_34dd3f_coerce_ssl_verify @ hermes_cli/config_providers.py:_coerce_ssl_verify
 * PoP: w15_configprov_34dd3f_normalize_extra_headers @ hermes_cli/config_providers.py:normalize_extra_headers
 * PoP: w15_configprov_34dd3f_is_provider_enabled @ hermes_cli/config_providers.py:is_provider_enabled
 * PoP: w15_configprov_34dd3f_route_model_cfg @ hermes_cli/config_providers.py:_route_model_cfg
 *
 * All ported functions are pure; the module-load route_identity import is
 * stubbed in the oracle and unused by these helpers.
 */
#include "noc_w15_configprov_34dd3f.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *lower_dup(const char *s) {
    char *p = dupstr(s);
    if (p) for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

static int is_blank_str(const json_t *v) {
    if (!v || v->type != JSON_STRING) return 0;
    char *s = strip_dup(v->str_val ? v->str_val : "");
    int blank = !s || !s[0];
    free(s);
    return !blank;
}

/* Python str(value) for scalar config values. */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    if (v->type == JSON_STRING) { snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        long long i = (long long)v->num_val;
        if ((double)i == v->num_val && i >= -9007199254740992LL && i <= 9007199254740992LL)
            snprintf(buf, cap, "%lld", i);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return;
    }
    char *s = json_dumps(v, 0);
    snprintf(buf, cap, "%s", s ? s : "");
    free(s);
}

/* coerce_provider_id -> freshly allocated string (never NULL). */
static char *coerce_pid(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    char tmp[512];
    py_str(v, tmp, sizeof(tmp));
    return strip_dup(tmp);
}

json_t *w15_configprov_34dd3f_canonical_api_mode(const char *api_mode) {
    char *cleaned = strip_dup(api_mode ? api_mode : "");
    char *low = lower_dup(cleaned ? cleaned : "");
    static const char *const from[] = {
        "openai", "openai_chat", "openai-chat", "chat-completions", "chatcompletions",
        "responses", "openai_responses", "openai-responses", "anthropic",
        "anthropic-messages", "messages", "bedrock", "bedrock-converse", NULL
    };
    static const char *const to[] = {
        "chat_completions", "chat_completions", "chat_completions", "chat_completions", "chat_completions",
        "codex_responses", "codex_responses", "codex_responses", "anthropic_messages",
        "anthropic_messages", "anthropic_messages", "bedrock_converse", "bedrock_converse", NULL
    };
    const char *res = cleaned ? cleaned : "";
    for (int i = 0; from[i]; i++)
        if (low && !strcmp(low, from[i])) { res = to[i]; break; }
    json_t *out = json_string(res);
    free(cleaned); free(low);
    return out;
}

json_t *w15_configprov_34dd3f_coerce_provider_id(const struct json_t *value) {
    char *s = coerce_pid(value);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

json_t *w15_configprov_34dd3f_stringify_provider_map(const struct json_t *providers) {
    json_t *out = json_object();
    if (!json_is_object(providers)) return out;
    for (size_t i = 0; i < json_object_size(providers); i++) {
        char *key = coerce_pid(json_object_get_key_at(providers, i) ?
                               json_string(json_object_get_key_at(providers, i)) : NULL);
        if (key && key[0])
            json_set(out, key, json_copy(json_object_get_at(providers, i)));
        free(key);
    }
    return out;
}

json_t *w15_configprov_34dd3f_find_provider_entry(const struct json_t *providers,
                                                  const struct json_t *key) {
    json_t *out = json_array();
    if (!json_is_object(providers)) { json_append(out, json_null()); json_append(out, json_null()); return out; }
    char *want = coerce_pid(key);
    if (!want || !want[0]) { free(want); json_append(out, json_null()); json_append(out, json_null()); return out; }
    json_t *exact = json_obj_get(providers, want);
    if (json_is_object(exact)) {
        json_append(out, json_string(want));
        json_append(out, json_copy(exact));
        free(want);
        return out;
    }
    for (size_t i = 0; i < json_object_size(providers); i++) {
        const char *stored = json_object_get_key_at(providers, i);
        json_t *entry = json_object_get_at(providers, i);
        char *sk = coerce_pid(stored ? json_string(stored) : NULL);
        if (sk && !strcmp(sk, want) && json_is_object(entry)) {
            json_append(out, stored ? json_string(stored) : json_null());
            json_append(out, json_copy(entry));
            free(sk); free(want);
            return out;
        }
        free(sk);
    }
    free(want);
    json_append(out, json_null());
    json_append(out, json_null());
    return out;
}

json_t *w15_configprov_34dd3f_normalize_provider_models(const struct json_t *models) {
    json_t *out = json_array();
    int discovered = 0;
    json_t *result = json_object();
    if (json_is_object(models) && json_object_size(models) > 0) {
        result = json_copy(models);
        json_t *flag = json_obj_get(result, "__discovered_model_catalog__");
        if (flag && flag->type == JSON_BOOL && flag->bool_val) discovered = 1;
        json_obj_del(result, "__discovered_model_catalog__");
        json_obj_del(result, "__explicit_model_allowlist__");
    } else if (json_is_array(models) && json_len(models) > 0) {
        result = json_object();
        for (size_t i = 0; i < json_len(models); i++) {
            json_t *item = json_get(models, i);
            if (item && item->type == JSON_STRING) {
                char *s = strip_dup(item->str_val ? item->str_val : "");
                if (s && s[0]) json_set(result, s, json_object());
                free(s);
                continue;
            }
            if (!json_is_object(item)) continue;
            const json_t *id = json_obj_get(item, "id");
            if (!is_blank_str(id)) id = json_obj_get(item, "name");
            if (!is_blank_str(id)) continue;
            char *key = strip_dup(id->str_val ? id->str_val : "");
            json_t *cfg = json_object();
            for (size_t j = 0; j < json_object_size(item); j++) {
                const char *k = json_object_get_key_at(item, j);
                if (!k || !strcmp(k, "id") || !strcmp(k, "name")) continue;
                json_set(cfg, k, json_copy(json_object_get_at(item, j)));
            }
            json_set(result, key, cfg);
            free(key);
        }
    }
    json_append(out, result);
    json_append(out, json_bool(discovered));
    return out;
}

json_t *w15_configprov_34dd3f_coerce_ssl_verify(const struct json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_BOOL) return json_bool(value->bool_val);
    if (value->type == JSON_STRING) {
        char *low = lower_dup(strip_dup(value->str_val ? value->str_val : ""));
        static const char *const falses[] = {"false", "0", "no", "off", NULL};
        static const char *const trues[] = {"true", "1", "yes", "on", NULL};
        int res = -1;
        for (int i = 0; falses[i]; i++) if (low && !strcmp(low, falses[i])) { res = 0; break; }
        if (res < 0) for (int i = 0; trues[i]; i++) if (low && !strcmp(low, trues[i])) { res = 1; break; }
        free(low);
        if (res >= 0) return json_bool(res ? 1 : 0);
        return json_null();
    }
    return json_null();
}

json_t *w15_configprov_34dd3f_normalize_extra_headers(const struct json_t *extra_headers) {
    json_t *out = json_object();
    if (!json_is_object(extra_headers) || json_object_size(extra_headers) == 0) return out;
    for (size_t i = 0; i < json_object_size(extra_headers); i++) {
        json_t *v = json_object_get_at(extra_headers, i);
        if (!v || v->type == JSON_NULL) continue;
        char buf[512];
        py_str(v, buf, sizeof(buf));
        json_set(out, json_object_get_key_at(extra_headers, i), json_string(buf));
    }
    return out;
}

json_t *w15_configprov_34dd3f_is_provider_enabled(const struct json_t *provider_cfg) {
    if (!json_is_object(provider_cfg)) return json_bool(1);
    json_t *flag = json_obj_get(provider_cfg, "enabled");
    if (!flag) return json_bool(1);
    if (flag->type == JSON_BOOL) return json_bool(flag->bool_val);
    if (flag->type == JSON_STRING) {
        char *low = lower_dup(strip_dup(flag->str_val ? flag->str_val : ""));
        static const char *const falses[] = {"false", "0", "no", "off", NULL};
        int disabled = 0;
        for (int i = 0; falses[i]; i++) if (low && !strcmp(low, falses[i])) { disabled = 1; break; }
        free(low);
        return json_bool(disabled ? 0 : 1);
    }
    return json_bool(json_truthy(flag));
}

json_t *w15_configprov_34dd3f_route_model_cfg(const struct json_t *entry,
                                              const char *model) {
    json_t *models = json_obj_get(entry, "models");
    if (!json_is_object(models)) return json_null();
    json_t *cfg = json_obj_get(models, model);
    return json_is_object(cfg) ? json_copy(cfg) : json_null();
}

