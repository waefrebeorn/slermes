/*
 * tts_tool_plugins.c - C home for upstream Hermes tools/tts_tool_plugins.py.
 *
 * PoP: w9_ttstoolplu_c91bc9_route_key @ tools/tts_tool_plugins.py:_plugin_route_key
 * PoP: w9_ttstoolplu_c91bc9_voice_kwargs @ tools/tts_tool_plugins.py:_plugin_voice_kwargs
 * PoP: w9_ttstoolplu_c91bc9_voice_compatible @ tools/tts_tool_plugins.py:_plugin_provider_is_voice_compatible
 * PoP: w9_ttstoolplu_c91bc9_is_available @ tools/tts_tool_plugins.py:_plugin_provider_is_available
 */
#include "noc_w9_ttstoolplu_c91bc9.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Python (value or "").lower().strip() */
static char *key_norm(const char *s) {
    const unsigned char *a = (const unsigned char *)(s ? s : "");
    while (*a && isspace(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && isspace(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) {
        for (size_t i = 0; i < n; ++i) p[i] = (char)tolower(a[i]);
        p[n] = '\0';
    }
    return p;
}

static int in_builtins(const struct json_t *builtins, const char *key) {
    if (!builtins || builtins->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(builtins); ++i) {
        struct json_t *v = json_get(builtins, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, key)) return 1;
    }
    return 0;
}

struct json_t *w9_ttstoolplu_c91bc9_route_key(const char *provider, const struct json_t *builtins,
                             const char *nous, int has_command) {
    char *key = key_norm(provider);
    if (!*key || in_builtins(builtins, key) || (nous && !strcmp(key, nous)) || has_command) {
        free(key);
        return json_null();
    }
    struct json_t *out = json_string(key);
    free(key);
    return out;
}

struct json_t *w9_ttstoolplu_c91bc9_voice_kwargs(const struct json_t *config) {
    struct json_t *out = json_object();
    const struct json_t *cfg = (config && config->type == JSON_OBJECT) ? config : NULL;
    const char *keys[3] = {"voice", "model", "speed"};
    for (int i = 0; i < 3; ++i) {
        struct json_t *v = cfg ? json_obj_get(cfg, keys[i]) : NULL;
        if (i < 2) {
            if (v && v->type == JSON_STRING && v->str_val && *v->str_val)
                json_set(out, keys[i], json_copy(v));
            else
                json_set(out, keys[i], json_null());
        } else {
            double speed = NAN;
            int have = 0;
            if (v && (v->type == JSON_NUMBER || v->type == JSON_BOOL)) {
                speed = (v->type == JSON_BOOL) ? (v->bool_val ? 1.0 : 0.0) : v->num_val;
                have = 1;
            }
            if (have && isfinite(speed)) json_set(out, "speed", json_number(speed));
            else json_set(out, "speed", json_null());
        }
    }
    return out;
}

int w9_ttstoolplu_c91bc9_voice_compatible(const char *provider, const struct json_t *builtins, int available) {
    char *key = key_norm(provider);
    int ok = 0;
    if (*key && !in_builtins(builtins, key)) ok = available ? 1 : 0;
    free(key);
    return ok;
}

int w9_ttstoolplu_c91bc9_is_available(const char *provider, int available) {
    (void)provider;
    return available ? 1 : 0;
}
