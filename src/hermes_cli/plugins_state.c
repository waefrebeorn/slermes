/*
 * plugins_state.c — faithful C11 port of the pure helpers in upstream Hermes
 * hermes_cli/plugins_state.py (gate w10_pluginssta_a8131d).
 *
 * PoP: w10_pluginssta_a8131d_relative_segments @ hermes_cli/plugins_state.py:_plugin_relative_segments
 * PoP: w10_pluginssta_a8131d_nested_value      @ hermes_cli/plugins_state.py:_nested_plugin_value
 * PoP: w10_pluginssta_a8131d_nested_mapping    @ hermes_cli/plugins_state.py:_nested_plugin_mapping
 * PoP: w10_pluginssta_a8131d_state_key_valid   @ hermes_cli/plugins_state.py:PluginState._validate_key
 * PoP: w10_pluginssta_a8131d_data_namespace    @ hermes_cli/plugins_state.py:_plugin_data_namespace
 *
 * Filesystem/locking writers are effectful and out of scope; the injected
 * `portable_ns` seam replaces _portable_data_namespace (as the live module's
 * own tests do).
 */
#include "noc_w10_pluginssta_a8131d.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static json_t *err_result(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "error", json_string("ValueError"));
    return o;
}
static json_t *ok_segments(void) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    return o;
}

/* ^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$ */
static int setting_seg_ok(const char *s, size_t n) {
    if (n < 1 || n > 128) return 0;
    if (!(isalpha((unsigned char)s[0]) || isdigit((unsigned char)s[0]))) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) return 0;
    }
    return 1;
}
static int reserved_root(const char *s, size_t n) {
    static const char *const roots[] = {"model", "plugins", "security", "settings"};
    for (int i = 0; i < 4; i++) {
        size_t m = strlen(roots[i]);
        if (n == m) {
            size_t j = 0;
            for (; j < m; j++) if (tolower((unsigned char)s[j]) != roots[i][j]) break;
            if (j == m) return 1;
        }
    }
    return 0;
}

json_t *w10_pluginssta_a8131d_relative_segments(const char *key) {
    if (!key || !key[0] || strchr(key, '/') || strchr(key, '\\')) return err_result();
    size_t n = strlen(key);
    /* split on '.' */
    json_t *segs = json_array();
    size_t start = 0;
    int first = 1;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || key[i] == '.') {
            size_t len = i - start;
            if (first && reserved_root(key + start, len)) { json_free(segs); return err_result(); }
            if (!setting_seg_ok(key + start, len)) { json_free(segs); return err_result(); }
            {
                char segbuf[256];
                if (len >= sizeof(segbuf)) len = sizeof(segbuf) - 1;
                memcpy(segbuf, key + start, len);
                segbuf[len] = '\0';
                json_append(segs, json_string(segbuf));
            }
            first = 0;
            start = i + 1;
        }
    }
    json_t *o = ok_segments();
    json_set(o, "segments", segs);
    return o;
}

json_t *w10_pluginssta_a8131d_nested_value(const json_t *root, const json_t *segments, const json_t *def) {
    const json_t *cur = root;
    if (segments && segments->type == JSON_ARRAY) {
        for (size_t i = 0; i < segments->c.count; i++) {
            json_t *s = segments->c.items[i];
            if (!cur || cur->type != JSON_OBJECT || !s || s->type != JSON_STRING) return json_copy(def);
            json_t *nxt = json_obj_get(cur, s->str_val);
            if (!nxt) return json_copy(def);
            cur = nxt;
        }
    }
    return json_copy(cur ? cur : def);
}

json_t *w10_pluginssta_a8131d_nested_mapping(const json_t *segments, const json_t *value) {
    json_t *nested = json_copy(value);
    if (segments && segments->type == JSON_ARRAY) {
        for (size_t i = segments->c.count; i > 0; i--) {
            json_t *s = segments->c.items[i - 1];
            json_t *wrap = json_object();
            json_set(wrap, (s && s->type == JSON_STRING) ? s->str_val : "", nested);
            nested = wrap;
        }
    }
    return nested;
}

/* ^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$ and no ".." */
static int state_key_ok(const char *key) {
    if (!key) return 0;
    size_t n = strlen(key);
    if (n < 1 || n > 128) return 0;
    if (!(isalnum((unsigned char)key[0]))) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = key[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == ':' || c == '-')) return 0;
    }
    if (strstr(key, "..")) return 0;
    return 1;
}
json_t *w10_pluginssta_a8131d_state_key_valid(const char *key) {
    return json_bool(state_key_ok(key));
}

static int portable_ok(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 1 || n > 192) return 0;
    if (!(isalnum((unsigned char)s[0]))) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) return 0;
    }
    return 1;
}
json_t *w10_pluginssta_a8131d_data_namespace(const char *plugin_id,
                                             const char *data_namespace,
                                             const char *portable_ns) {
    const char *candidate = (data_namespace && data_namespace[0]) ? data_namespace : plugin_id;
    if (!candidate) candidate = "";
    int portable = (data_namespace && data_namespace[0]) && !strncmp(candidate, "agent-plugin-", 13);
    if (portable && portable_ok(candidate)) return json_string(candidate);
    return json_string(portable_ns ? portable_ns : "");
}
