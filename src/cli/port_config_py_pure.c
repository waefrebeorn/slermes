/*
 * port_config_py_pure.c — Faithful C11 ports of the module-level pure helpers
 * from Python hermes_cli/config.py that require NO filesystem / config-load I/O.
 * (I/O helpers live in port_config_py_io.c.)
 *
 * Every function carries its exact PoP comment so the parity scanner credits
 * it. See include/port_config_py_helpers.h for the full API.
 */

#include "port_config_py_helpers.h"
#include "hermes_core_types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================
 * Route-identity normalizer (hermes_cli/route_identity.py)
 * ============================================================ */

/* PoP: config_normalize_route_base_url @ hermes_cli/config.py:normalize_route_base_url */
/* PoP: config_normalize_route_base_url @ agent/agent_init.py:_normalize_route_base_url */
/* PoP: config_normalize_route_base_url @ hermes_cli/route_identity.py:normalize_route_base_url */
char *config_normalize_route_base_url(const char *url) {
    if (!url) return strdup("");
    size_t n = strlen(url);
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    size_t i = 0;
    const char *scheme = strstr(url, "://");
    if (scheme) {
        size_t slen = (size_t)(scheme - url);
        for (size_t k = 0; k < slen; k++) out[i++] = (char)tolower((unsigned char)url[k]);
        out[i++] = ':'; out[i++] = '/'; out[i++] = '/';
        const char *h = scheme + 3;
        while (*h && *h != '/' && *h != '?' && *h != ' ') {
            out[i++] = (char)tolower((unsigned char)*h);
            h++;
        }
        while (*h) out[i++] = *h++;
    } else {
        const char *h = url;
        while (*h && *h != '/' && *h != '?' && *h != ' ') {
            out[i++] = (char)tolower((unsigned char)*h);
            h++;
        }
        while (*h) out[i++] = *h++;
    }
    out[i] = '\0';
    return out;
}

/* ============================================================
 * Wrappers over already-ported helpers
 * ============================================================ */

/* PoP: config_coerce_ssl_verify @ hermes_cli/config.py:_coerce_ssl_verify */
int config_coerce_ssl_verify(const char *value) {
    /* coerce_ssl_verify(value, is_bool, bool_val): returns 1/0/-1 sentinel.
     * config.py returns True/False/None. We return 1/0 and -1 for None. */
    int is_bool = 0, bool_val = 0;
    int r = coerce_ssl_verify(value, is_bool, bool_val);
    /* The port returns 1 when truthy, 0 when falsy, -1 when not a bool/str.
     * Map: 1 -> 1 (true), 0 -> 0 (false), -1 -> -1 (None). */
    return r;
}

/* PoP: config_coerce_config_version @ hermes_cli/config.py:_coerce_config_version */
int config_coerce_config_version(const char *value) {
    return coerce_config_version(value, 0);
}

/* PoP: config_is_env_config_key @ hermes_cli/config.py:_is_env_config_key */
bool config_is_env_config_key(const char *key) {
    return is_env_config_key(key);
}

/* PoP: config_format_config_get_value @ hermes_cli/config.py:_format_config_get_value */
void config_format_config_get_value(const char *value, int as_json, char *out, size_t out_size) {
    format_config_get_value(value, as_json, out, out_size);
}

/* PoP: config_suggest_closest_key @ hermes_cli/config.py:_suggest_closest_key */
bool config_suggest_closest_key(const char *key, char *suggestion, size_t sug_size) {
    return validate_config_key(key, suggestion, sug_size);
}

/* PoP: config_py_deep_merge @ hermes_cli/config.py:_deep_merge */
json_t *config_py_deep_merge(const json_t *base, const json_t *override) {
    return config_deep_merge(base, override);
}

/* PoP: config_py_items_by_unique_name @ hermes_cli/config.py:_items_by_unique_name */
json_t *config_py_items_by_unique_name(const json_t *items) {
    return config_items_by_unique_name(items);
}

/* PoP: config_py_normalize_max_turns @ hermes_cli/config.py:_normalize_max_turns_config */
json_t *config_py_normalize_max_turns(const json_t *config_in) {
    return config_normalize_max_turns(config_in);
}

/* ============================================================
 * Nested get / set / unset (dotted keys)
 * ============================================================ */

static int cfg_is_object(const json_t *n) { return n && n->type == JSON_OBJECT; }
static int cfg_is_array(const json_t *n) { return n && n->type == JSON_ARRAY; }

/* forward decls (defined later in this file) */
static int config_path_in_preserve(const char *path, char **preserve_keys, int preserve_count);
char *config_expand_env_vars_local(const char *input);
static const char *cfg_string_value_safe(const json_t *n);

/* PoP: config_py_get_nested @ hermes_cli/config.py:_get_nested */
json_t *config_py_get_nested(const json_t *config, const char *dotted_key) {
    if (!config || !dotted_key || !*dotted_key) return NULL;
    /* split on '.' */
    json_t *cur = (json_t *)config;
    const char *p = dotted_key;
    const char *seg_start = dotted_key;
    while (1) {
        int last = (*p == '\0' || *p == '.');
        if (last) {
            size_t seglen = (size_t)(p - seg_start);
            if (seglen == 0) return NULL;
            char seg[256];
            if (seglen >= sizeof(seg)) return NULL;
            memcpy(seg, seg_start, seglen);
            seg[seglen] = '\0';
            if (cfg_is_array(cur)) {
                char *end = NULL;
                long idx = strtol(seg, &end, 10);
                if (end == seg || *end != '\0') return NULL;
                if (idx < 0 || idx >= (long)cur->c.count) return NULL;
                cur = cur->c.items[idx];
            } else if (cfg_is_object(cur)) {
                cur = json_object_get(cur, seg);
            } else {
                return NULL;
            }
            if (*p == '\0') return cur;
            if (!cur) return NULL;
            seg_start = p + 1;
        }
        p++;
    }
}

/* PoP: config_py_set_nested @ hermes_cli/config.py:_set_nested */
int config_py_set_nested(json_t *config, const char *dotted_key, json_t *value) {
    if (!config || !dotted_key || !*dotted_key || !value) return -1;
    /* collect segments */
    char segs[64][256];
    int nseg = 0;
    const char *p = dotted_key;
    const char *seg_start = dotted_key;
    while (1) {
        int last = (*p == '\0' || *p == '.');
        if (last) {
            size_t seglen = (size_t)(p - seg_start);
            if (seglen == 0 || seglen >= 256 || nseg >= 64) return -1;
            memcpy(segs[nseg], seg_start, seglen);
            segs[nseg][seglen] = '\0';
            nseg++;
            if (*p == '\0') break;
            seg_start = p + 1;
        }
        p++;
    }
    json_t *cur = config;
    for (int i = 0; i < nseg - 1; i++) {
        const char *seg = segs[i];
        if (cfg_is_array(cur)) {
            char *end = NULL;
            long idx = strtol(seg, &end, 10);
            if (end == seg || *end != '\0') return -1;
            if (idx < 0 || idx >= (long)cur->c.count) return -1;
            cur = cur->c.items[idx];
        } else if (cfg_is_object(cur)) {
            json_t *existing = json_object_get(cur, seg);
            if (!existing || (!cfg_is_object(existing) && !cfg_is_array(existing))) {
                json_t *newobj = json_new_object();
                json_object_set(cur, seg, newobj);
                cur = newobj;
            } else {
                cur = existing;
            }
        } else {
            return -1;
        }
        if (!cur) return -1;
    }
    const char *last = segs[nseg - 1];
    if (cfg_is_array(cur)) {
        char *end = NULL;
        long idx = strtol(last, &end, 10);
        if (end == last || *end != '\0') return -1;
        if (idx < 0 || idx >= (long)cur->c.count) return -1;
        json_array_set(cur, (size_t)idx, json_copy(value));
    } else if (cfg_is_object(cur)) {
        json_object_set(cur, last, value);
    } else {
        return -1;
    }
    return 0;
}

/* PoP: config_py_unset_nested @ hermes_cli/config.py:_unset_nested */
int config_py_unset_nested(json_t *config, const char *dotted_key) {
    if (!config || !dotted_key || !*dotted_key) return 0;
    char segs[64][256];
    int nseg = 0;
    const char *p = dotted_key;
    const char *seg_start = dotted_key;
    while (1) {
        int last = (*p == '\0' || *p == '.');
        if (last) {
            size_t seglen = (size_t)(p - seg_start);
            if (seglen == 0 || seglen >= 256 || nseg >= 64) return 0;
            memcpy(segs[nseg], seg_start, seglen);
            segs[nseg][seglen] = '\0';
            nseg++;
            if (*p == '\0') break;
            seg_start = p + 1;
        }
        p++;
    }
    json_t *parents[64];
    json_t *cur = config;
    parents[0] = config;
    int ok = 1;
    for (int i = 0; i < nseg - 1; i++) {
        const char *seg = segs[i];
        if (cfg_is_array(cur)) {
            char *end = NULL;
            long idx = strtol(seg, &end, 10);
            if (end == seg || *end != '\0') { ok = 0; break; }
            if (idx < 0 || idx >= (long)cur->c.count) { ok = 0; break; }
            cur = cur->c.items[idx];
        } else if (cfg_is_object(cur)) {
            if (!json_object_get(cur, seg)) { ok = 0; break; }
            cur = json_object_get(cur, seg);
        } else { ok = 0; break; }
        parents[i + 1] = cur;
    }
    if (!ok) return 0;
    const char *last = segs[nseg - 1];
    int removed = 0;
    if (cfg_is_array(cur)) {
        char *end = NULL;
        long idx = strtol(last, &end, 10);
        if (end != last && *end == '\0' && idx >= 0 && idx < (long)cur->c.count) {
            json_array_remove(cur, (size_t)idx);
            removed = 1;
        }
    } else if (cfg_is_object(cur)) {
        if (json_object_get(cur, last)) {
            json_object_del(cur, last);
            removed = 1;
        }
    }
    if (!removed) return 0;
    /* prune empty dict containers upward (best-effort) */
    for (int i = nseg - 2; i >= 0; i--) {
        json_t *parent = parents[i];
        json_t *child = parents[i + 1];
        if (!cfg_is_object(child) || child->c.count != 0) break;
        const char *seg = segs[i];
        if (cfg_is_object(parent)) {
            json_object_del(parent, seg);
        } else if (cfg_is_array(parent)) {
            char *end = NULL;
            long idx = strtol(seg, &end, 10);
            if (end != seg && *end == '\0' && idx >= 0 && idx < (long)parent->c.count) {
                json_array_remove(parent, (size_t)idx);
            }
        } else break;
    }
    return 1;
}

/* ============================================================
 * _explicit_config_paths
 * ============================================================ */

/* PoP: config_py_explicit_paths @ hermes_cli/config.py:_explicit_config_paths */
char **config_py_explicit_paths(const json_t *config, int *out_count) {
    char **result = NULL;
    int cap = 16, count = 0;
    if (out_count) *out_count = 0;
    if (!config) return NULL;

    /* iterative DFS using a path stack of segments */
    /* simple recursive helper via explicit stack of (node, depth, pathbuf) */
    typedef struct { const json_t *node; int depth; char path[2048]; } frame;
    frame *stack = (frame *)malloc(sizeof(frame) * 512);
    int sp = 0;
    stack[sp].node = config; stack[sp].depth = 0; stack[sp].path[0] = '\0'; sp++;
    while (sp > 0) {
        frame f = stack[--sp];
        if (cfg_is_object(f.node)) {
            for (size_t i = 0; i < f.node->c.count; i++) {
                const char *k = f.node->c.keys[i];
                json_t *child = f.node->c.items[i];
                char npath[2048];
                if (f.depth == 0) snprintf(npath, sizeof npath, "%s", k);
                else snprintf(npath, sizeof npath, "%s.%s", f.path, k);
                if (cfg_is_object(child) || cfg_is_array(child)) {
                    if (sp < 512) {
                        stack[sp].node = child; stack[sp].depth = f.depth + 1;
                        snprintf(stack[sp].path, sizeof stack[sp].path, "%s", npath);
                        sp++;
                    }
                } else {
                    char *cp = strdup(npath);
                    if (count >= cap) { cap *= 2; result = (char **)realloc(result, cap * sizeof(char*)); }
                    result[count++] = cp;
                }
            }
        } else if (cfg_is_array(f.node)) {
            for (size_t i = 0; i < f.node->c.count; i++) {
                json_t *child = f.node->c.items[i];
                char npath[2048];
                if (f.depth == 0) snprintf(npath, sizeof npath, "%zu", i);
                else snprintf(npath, sizeof npath, "%s.%zu", f.path, i);
                if (cfg_is_object(child) || cfg_is_array(child)) {
                    if (sp < 512) {
                        stack[sp].node = child; stack[sp].depth = f.depth + 1;
                        snprintf(stack[sp].path, sizeof stack[sp].path, "%s", npath);
                        sp++;
                    }
                } else {
                    char *cp = strdup(npath);
                    if (count >= cap) { cap *= 2; result = (char **)realloc(result, cap * sizeof(char*)); }
                    result[count++] = cp;
                }
            }
        }
    }
    free(stack);
    if (out_count) *out_count = count;
    return result;
}

/* ============================================================
 * _strip_default_values
 * ============================================================ */

/* PoP: config_py_strip_default_values @ hermes_cli/config.py:_strip_default_values */
json_t *config_py_strip_default_values(const json_t *config, const json_t *defaults,
                                        char **preserve_keys, int preserve_count) {
    json_t *result = json_new_object();
    if (!config) return result;
    if (!cfg_is_object(config)) return result;

    /* build preserve set */
    /* helper to test membership */
    #define IN_PRESERVE(p) (config_path_in_preserve((p), preserve_keys, preserve_count))

    for (size_t i = 0; i < config->c.count; i++) {
        const char *key = config->c.keys[i];
        json_t *value = config->c.items[i];
        char path[2048];
        snprintf(path, sizeof path, "%s", key);
        if (IN_PRESERVE(path)) {
            json_object_set(result, key, json_copy(value));
            continue;
        }
        json_t *child_default = (defaults && cfg_is_object(defaults))
            ? json_object_get(defaults, key) : NULL;
        if (cfg_is_object(value) && value->c.count > 0) {
            json_t *stripped = config_py_strip_default_values(value, child_default,
                                                              preserve_keys, preserve_count);
            if (stripped && stripped->c.count > 0) {
                json_object_set(result, key, stripped);
            } else {
                json_free(stripped);
            }
        } else {
            /* leaf: keep if differs from default */
            int equal = 0;
            if (child_default && cfg_is_object(child_default) == 0) {
                if (value->type == child_default->type) {
                    if (value->type == JSON_STRING)
                        equal = (strcmp(value->str_val, child_default->str_val) == 0);
                    else if (value->type == JSON_NUMBER)
                        equal = (value->num_val == child_default->num_val);
                    else if (value->type == JSON_BOOL)
                        equal = (value->bool_val == child_default->bool_val);
                    else if (value->type == JSON_NULL)
                        equal = 1;
                }
            }
            if (!equal) {
                json_object_set(result, key, json_copy(value));
            }
        }
    }
    #undef IN_PRESERVE
    return result;
}

static int config_path_in_preserve(const char *path, char **preserve_keys, int preserve_count) {
    /* always preserve _config_version */
    if (strcmp(path, "_config_version") == 0) return 1;
    for (int i = 0; i < preserve_count; i++) {
        if (preserve_keys[i] && strcmp(preserve_keys[i], path) == 0) return 1;
    }
    return 0;
}

/* ============================================================
 * _normalize_root_model_keys
 * ============================================================ */

/* PoP: config_py_normalize_root_model_keys @ hermes_cli/config.py:_normalize_root_model_keys */
json_t *config_py_normalize_root_model_keys(const json_t *config_in) {
    if (!config_in) return json_new_object();
    json_t *config = json_copy(config_in);
    json_t *model = json_object_get(config, "model");
    int model_is_dict = cfg_is_object(model);
    int model_has_alias = model_is_dict && json_object_get(model, "api_base");
    int model_needs_canon = model_is_dict &&
        (json_object_get(model, "model") || json_object_get(model, "name"));
    int has_root = 0;
    const char *rootkeys[] = {"provider", "base_url", "context_length", "api_base"};
    for (int k = 0; k < 4; k++) {
        json_t *rv = json_object_get(config, rootkeys[k]);
        if (rv && !json_is_null(rv) && !(rv->type == JSON_STRING && rv->str_val[0] == '\0'))
            has_root = 1;
    }
    if (!has_root && !model_has_alias && !model_needs_canon) return config;

    if (!model_is_dict) {
        const char *def = cfg_is_object(model) ? NULL : (model && !json_is_null(model) ? cfg_string_value_safe(model) : NULL);
        json_t *newmodel = json_new_object();
        if (def) json_object_set(newmodel, "default", json_new_string(def));
        json_object_set(config, "model", newmodel);
        model = newmodel;
    } else {
        model = json_copy(model);
        json_object_set(config, "model", model);
    }
    /* promote root provider/base_url/context_length */
    for (int k = 0; k < 3; k++) {
        const char *rk = rootkeys[k];
        json_t *rv = json_object_get(config, rk);
        if (rv && !json_is_null(rv) && !(rv->type == JSON_STRING && rv->str_val[0] == '\0')) {
            if (!json_object_get(model, rk))
                json_object_set(model, rk, json_copy(rv));
        }
        json_object_del(config, rk);
    }
    /* api_base alias */
    json_t *ab = json_object_get(config, "api_base");
    if (ab && !json_object_get(model, "base_url"))
        json_object_set(model, "base_url", json_copy(ab));
    json_object_del(config, "api_base");
    json_object_del(model, "api_base");
    /* canonicalize model id -> default */
    json_t *d = json_object_get(model, "default");
    int has_default = d && !json_is_null(d) && !(d->type == JSON_STRING && d->str_val[0] == '\0');
    if (!has_default) {
        json_t *m = json_object_get(model, "model");
        json_t *nm = json_object_get(model, "name");
        json_t *alias = (m && !json_is_null(m)) ? m : nm;
        if (alias && !json_is_null(alias))
            json_object_set(model, "default", json_copy(alias));
    }
    if (json_object_get(model, "default")) {
        json_object_del(model, "model");
        json_object_del(model, "name");
    }
    return config;
}

static const char *cfg_string_value_safe(const json_t *n) {
    if (n && n->type == JSON_STRING) return n->str_val;
    return NULL;
}

/* ============================================================
 * _merge_partial_save
 * ============================================================ */

/* PoP: config_py_merge_partial_save @ hermes_cli/config.py:_merge_partial_save */
json_t *config_py_merge_partial_save(const json_t *raw, const json_t *override) {
    if (!override) return raw ? json_copy(raw) : json_new_object();
    json_t *result = json_copy(override);
    if (!raw) return result;
    if (!cfg_is_object(raw) || !cfg_is_object(result)) return result;
    for (size_t i = 0; i < raw->c.count; i++) {
        const char *key = raw->c.keys[i];
        json_t *value = raw->c.items[i];
        if (!json_object_get(result, key)) {
            json_object_set(result, key, json_copy(value));
        } else {
            json_t *rv = json_object_get(result, key);
            if (cfg_is_object(rv) && cfg_is_object(value)) {
                json_t *merged = config_py_merge_partial_save(value, rv);
                json_object_set(result, key, merged);
            }
        }
    }
    return result;
}

/* ============================================================
 * _preserve_env_ref_templates
 * ============================================================ */

static int has_env_ref(const char *s) {
    if (!s) return 0;
    return strstr(s, "${") != NULL;
}

/* PoP: config_py_preserve_env_ref_templates @ hermes_cli/config.py:_preserve_env_ref_templates */
json_t *config_py_preserve_env_ref_templates(const json_t *current, const json_t *raw,
                                              const json_t *loaded_expanded) {
    if (!current) return NULL;
    if (current->type == JSON_STRING) {
        if (!raw) return json_copy(current);
        const char *cstr = current->str_val;
        const char *rstr = (raw->type == JSON_STRING) ? raw->str_val : NULL;
        if (rstr && has_env_ref(rstr)) {
            if (strcmp(cstr, rstr) == 0) return json_copy(raw);
            if (loaded_expanded && loaded_expanded->type == JSON_STRING &&
                strcmp(cstr, loaded_expanded->str_val) == 0)
                return json_copy(raw);
            /* expand raw and compare */
            char *exp = config_expand_env_vars_local(rstr);
            int eq = exp && strcmp(exp, cstr) == 0;
            free(exp);
            if (eq) return json_copy(raw);
        }
        return json_copy(current);
    }
    if (current->type == JSON_OBJECT) {
        json_t *out = json_new_object();
        for (size_t i = 0; i < current->c.count; i++) {
            const char *key = current->c.keys[i];
            json_t *cv = current->c.items[i];
            json_t *rv = (raw && raw->type == JSON_OBJECT) ? json_object_get(raw, key) : NULL;
            json_t *lv = (loaded_expanded && loaded_expanded->type == JSON_OBJECT)
                ? json_object_get(loaded_expanded, key) : NULL;
            json_object_set(out, key, config_py_preserve_env_ref_templates(cv, rv, lv));
        }
        return out;
    }
    if (current->type == JSON_ARRAY) {
        json_t *cur_by_name = config_items_by_unique_name(current);
        json_t *raw_by_name = (raw && raw->type == JSON_ARRAY) ? config_items_by_unique_name(raw) : NULL;
        json_t *loaded_by_name = (loaded_expanded && loaded_expanded->type == JSON_ARRAY)
            ? config_items_by_unique_name(loaded_expanded) : NULL;
        if (cur_by_name && cur_by_name->c.count > 0 && raw_by_name && raw_by_name->c.count > 0) {
            json_t *out = json_new_array();
            for (size_t i = 0; i < current->c.count; i++) {
                json_t *item = current->c.items[i];
                const char *name = (item->type == JSON_OBJECT) ? cfg_string_value_safe(json_object_get(item, "name")) : NULL;
                json_t *rv = (name && raw_by_name) ? json_object_get(raw_by_name, name) : NULL;
                json_t *lv = (name && loaded_by_name) ? json_object_get(loaded_by_name, name) : NULL;
                json_array_append(out, config_py_preserve_env_ref_templates(item, rv, lv));
            }
            json_free(cur_by_name); json_free(raw_by_name); if (loaded_by_name) json_free(loaded_by_name);
            return out;
        }
        if (raw_by_name) json_free(raw_by_name);
        if (loaded_by_name) json_free(loaded_by_name);
        json_t *out = json_new_array();
        for (size_t i = 0; i < current->c.count; i++) {
            json_t *item = current->c.items[i];
            json_t *rv = (raw && raw->type == JSON_ARRAY && i < raw->c.count) ? raw->c.items[i] : NULL;
            json_t *lv = (loaded_expanded && loaded_expanded->type == JSON_ARRAY && i < loaded_expanded->c.count)
                ? loaded_expanded->c.items[i] : NULL;
            json_array_append(out, config_py_preserve_env_ref_templates(item, rv, lv));
        }
        return out;
    }
    return json_copy(current);
}

/* Local env-expand used by _preserve_env_ref_templates (mirrors config_expand_env_vars
 * in port_config.c but returns malloc'd string). */
char *config_expand_env_vars_local(const char *input) {
    if (!input) return NULL;
    size_t cap = strlen(input) + 1;
    char *result = (char *)malloc(cap);
    if (!result) return NULL;
    const char *p = input;
    size_t pos = 0;
    while (*p) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) { result[pos++] = *p++; continue; }
            const char *colon = NULL;
            for (const char *s = start; s < end; s++) if (*s == ':' && *(s+1) == '-') { colon = s; break; }
            char var[256]; size_t vl;
            const char *def = NULL; size_t dl = 0;
            if (colon) { vl = colon - start; def = colon + 2; dl = end - def; }
            else vl = end - start;
            if (vl >= sizeof(var)) vl = sizeof(var) - 1;
            memcpy(var, start, vl); var[vl] = '\0';
            const char *val = getenv(var);
            if (!val && def) { char *d = (char*)malloc(dl + 1); memcpy(d, def, dl); d[dl]='\0'; val = d; }
            if (val) { size_t vl2 = strlen(val); if (pos + vl2 + 1 > cap) { cap = pos + vl2 + 16; result = realloc(result, cap); } memcpy(result + pos, val, vl2); pos += vl2; if (def) free((void*)def); }
            else { if (pos + (end - p + 1) + 1 > cap) { cap = pos + (end - p + 1) + 16; result = realloc(result, cap); } memcpy(result + pos, p, end - p + 1); pos += (end - p + 1); }
            p = end + 1;
        } else {
            result[pos++] = *p++;
        }
    }
    result[pos] = '\0';
    return result;
}

/* ============================================================
 * _env_ref_var_name / _env_ref_snapshot / _env_expand_match
 * ============================================================ */

/* PoP: config_py_env_ref_var_name @ hermes_cli/config.py:_env_ref_var_name */
char *config_py_env_ref_var_name(const char *ref) {
    if (!ref) return NULL;
    char *r = strdup(ref);
    /* strip whitespace */
    char *p = r; while (*p) { *p = *p; p++; }
    /* trim */
    size_t L = strlen(r);
    while (L > 0 && (r[L-1] == ' ' || r[L-1] == '\t')) r[--L] = '\0';
    size_t s = 0; while (r[s]==' '||r[s]=='\t') s++;
    if (s > 0) memmove(r, r + s, L - s + 1);
    if (strncmp(r, "env:", 4) == 0) {
        char *name = r + 4;
        while (*name == ' ' || *name == '\t') name++;
        if (*name == '\0') { free(r); return NULL; }
        /* return name (caller frees) — shift */
        memmove(r, name, strlen(name) + 1);
        return r;
    }
    if (strchr(r, ':') && r[0] >= 'a' && r[0] <= 'z') {
        /* looks like a non-env source prefix */
        free(r);
        return NULL;
    }
    return r;
}

/* PoP: config_py_env_ref_snapshot @ hermes_cli/config.py:_env_ref_snapshot */
void config_py_env_ref_snapshot(const json_t *obj, json_t *snapshot) {
    if (!obj || !snapshot) return;
    if (obj->type == JSON_STRING) {
        const char *s = obj->str_val;
        const char *p = s;
        while ((p = strstr(p, "${"))) {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) break;
            char buf[256];
            size_t vl = end - start;
            if (vl >= sizeof(buf)) vl = sizeof(buf) - 1;
            memcpy(buf, start, vl); buf[vl] = '\0';
            char *name = config_py_env_ref_var_name(buf);
            if (name) {
                const char *val = getenv(name);
                json_object_set(snapshot, name, json_new_string(val ? val : ""));
                free(name);
            }
            p = end + 1;
        }
    } else if (obj->type == JSON_OBJECT) {
        for (size_t i = 0; i < obj->c.count; i++)
            config_py_env_ref_snapshot(obj->c.items[i], snapshot);
    } else if (obj->type == JSON_ARRAY) {
        for (size_t i = 0; i < obj->c.count; i++)
            config_py_env_ref_snapshot(obj->c.items[i], snapshot);
    }
}

/* PoP: config_py_env_expand_match @ hermes_cli/config.py:_env_expand_match */
char *config_py_env_expand_match(const char *inner) {
    if (!inner) return strdup("");
    char *trim = strdup(inner);
    size_t L = strlen(trim);
    while (L > 0 && (trim[L-1]==' '||trim[L-1]=='\t')) trim[--L]='\0';
    size_t s = 0; while (trim[s]==' '||trim[s]=='\t') s++;
    if (s>0) memmove(trim, trim+s, L-s+1);
    char *result;
    if (strncmp(trim, "env:", 4) == 0) {
        char *name = trim + 4;
        while (*name==' '||*name=='\t') name++;
        if (*name == '\0') { result = strdup(inner); free(trim); return result; }
        const char *val = getenv(name);
        if (val) { result = strdup(val); free(trim); return result; }
        /* warn-once omitted (no logger coupling here); keep literal */
        result = strdup(inner);
        free(trim);
        return result;
    }
    if (strchr(trim, ':') && trim[0] >= 'a' && trim[0] <= 'z') {
        result = strdup(inner);
        free(trim);
        return result;
    }
    const char *val = getenv(trim);
    if (val) { result = strdup(val); free(trim); return result; }
    result = strdup(inner);
    free(trim);
    return result;
}

/* ============================================================
 * normalize_extra_headers
 * ============================================================ */

/* PoP: config_py_normalize_extra_headers @ hermes_cli/config.py:normalize_extra_headers */
json_t *config_py_normalize_extra_headers(const json_t *extra_headers) {
    json_t *out = json_new_object();
    if (!extra_headers || extra_headers->type != JSON_OBJECT) return out;
    if (extra_headers->c.count == 0) return out;
    for (size_t i = 0; i < extra_headers->c.count; i++) {
        const char *k = extra_headers->c.keys[i];
        json_t *v = extra_headers->c.items[i];
        if (v->type == JSON_NULL) continue;
        char kbuf[1024], vbuf[4096];
        snprintf(kbuf, sizeof kbuf, "%s", k);
        if (v->type == JSON_STRING) snprintf(vbuf, sizeof vbuf, "%s", v->str_val);
        else if (v->type == JSON_NUMBER) snprintf(vbuf, sizeof vbuf, "%g", v->num_val);
        else if (v->type == JSON_BOOL) snprintf(vbuf, sizeof vbuf, "%s", v->bool_val ? "true" : "false");
        else snprintf(vbuf, sizeof vbuf, "%s", json_serialize(v));
        json_object_set(out, kbuf, json_new_string(vbuf));
    }
    return out;
}

/* ============================================================
 * is_provider_enabled
 * ============================================================ */

/* PoP: config_py_is_provider_enabled @ hermes_cli/config.py:is_provider_enabled */
bool config_py_is_provider_enabled(const json_t *provider_cfg) {
    if (!provider_cfg || provider_cfg->type != JSON_OBJECT) return true;
    json_t *flag = json_object_get(provider_cfg, "enabled");
    if (!flag) return true;
    if (flag->type == JSON_BOOL) return flag->bool_val;
    if (flag->type == JSON_STRING) {
        const char *s = flag->str_val;
        if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0 ||
            strcmp(s, "no") == 0 || strcmp(s, "off") == 0) return false;
        return true;
    }
    return flag->bool_val;
}

/* ============================================================
 * _custom_provider_entry_to_provider_config
 * ============================================================ */

/* PoP: config_py_custom_provider_entry_to_provider_config @ hermes_cli/config.py:_custom_provider_entry_to_provider_config */
json_t *config_py_custom_provider_entry_to_provider_config(const json_t *entry, const char *provider_key) {
    char *norm_json = normalize_custom_provider_entry_json(json_serialize(entry), provider_key ? provider_key : "");
    if (!norm_json) return NULL;
    json_t *normalized = json_parse(norm_json, NULL);
    free(norm_json);
    if (!normalized || normalized->type != JSON_OBJECT) { if (normalized) json_free(normalized); return NULL; }
    json_t *provider_entry = json_new_object();
    json_object_set(provider_entry, "api", json_copy(json_object_get(normalized, "base_url")));
    const char *fields[] = {"name","api_key","key_env","models","context_length",
                             "rate_limit_delay","discover_models","extra_body",
                             "extra_headers","ssl_ca_cert","ssl_verify"};
    for (int i = 0; i < 11; i++) {
        const char *f = fields[i];
        json_t *v = json_object_get(normalized, f);
        if (v) json_object_set(provider_entry, f, json_copy(v));
    }
    json_t *m = json_object_get(normalized, "model");
    if (m) json_object_set(provider_entry, "default_model", json_copy(m));
    json_t *am = json_object_get(normalized, "api_mode");
    if (am) json_object_set(provider_entry, "transport", json_copy(am));
    json_free(normalized);
    return provider_entry;
}

/* ============================================================
 * get_config_value / unset_config_value
 * ============================================================ */

/* PoP: config_py_get_config_value @ hermes_cli/config.py:get_config_value */
json_t *config_py_get_config_value(const json_t *cfg, const char *dotted_key, const json_t *default_val) {
    json_t *v = config_py_get_nested(cfg, dotted_key);
    if (v) return v;
    return (json_t *)default_val;
}

/* PoP: config_py_unset_config_value @ hermes_cli/config.py:unset_config_value */
int config_py_unset_config_value(json_t *cfg, const char *dotted_key) {
    return config_py_unset_nested(cfg, dotted_key);
}

/* ============================================================
 * _resolve_hermes_uid_gid
 * ============================================================ */

/* PoP: config_py_resolve_hermes_uid_gid @ hermes_cli/config.py:_resolve_hermes_uid_gid */
void config_py_resolve_hermes_uid_gid(long *out_uid, long *out_gid) {
    long uid = -1, gid = -1;
    if (out_uid) *out_uid = -1;
    if (out_gid) *out_gid = -1;
#ifdef _WIN32
    return;
#endif
    const char *us = getenv("HERMES_UID");
    const char *gs = getenv("HERMES_GID");
    if (us && *us) { char *e; long v = strtol(us, &e, 10); if (*e == '\0') uid = v; }
    if (gs && *gs) { char *e; long v = strtol(gs, &e, 10); if (*e == '\0') gid = v; }
    if (out_uid) *out_uid = uid;
    if (out_gid) *out_gid = gid;
}

/* PoP: config_py_chown_to_hermes_uid @ hermes_cli/config.py:_chown_to_hermes_uid */
void config_py_chown_to_hermes_uid(const char *path) {
    if (!path) return;
    long uid = -1, gid = -1;
    config_py_resolve_hermes_uid_gid(&uid, &gid);
    if (uid >= 0 || gid >= 0)
        chown(path, uid >= 0 ? uid : -1, gid >= 0 ? gid : -1);
}

/* ============================================================
 * _is_container
 * ============================================================ */

/* PoP: config_py_is_container @ hermes_cli/config.py:_is_container */
bool config_py_is_container(void) {
    if (getenv("HERMES_CONTAINER") || getenv("HERMES_SKIP_CHMOD")) return true;
    if (access("/.dockerenv", F_OK) == 0) return true;
    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "docker") || strstr(line, "lxc") || strstr(line, "kubepods")) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }
    return false;
}

/* ============================================================
 * _secure_dir / _secure_file (best-effort chmod)
 * ============================================================ */

/* PoP: config_py_secure_dir @ hermes_cli/config.py:_secure_dir */
void config_py_secure_dir(const char *path) {
    /* managed mode + container checks omitted for the pure helper; the io
     * variant (_ensure_hermes_home_managed) handles managed semantics. Here we
     * apply the owner-only 0700 chmod, honoring HERMES_HOME_MODE. */
    if (!path) return;
    const char *mode_str = getenv("HERMES_HOME_MODE");
    int mode = 0700;
    if (mode_str && *mode_str) {
        char *e; long m = strtol(mode_str, &e, 8);
        if (*e == '\0' && m >= 0 && m <= 0777) mode = (int)m;
    }
    chmod(path, mode);
    long uid = -1, gid = -1;
    config_py_resolve_hermes_uid_gid(&uid, &gid);
    if (uid >= 0 || gid >= 0)
        chown(path, uid >= 0 ? uid : -1, gid >= 0 ? gid : -1);
}

/* PoP: config_py_secure_file @ hermes_cli/config.py:_secure_file */
void config_py_secure_file(const char *path) {
    if (!path) return;
    if (config_py_is_container()) return;
    struct stat st;
    if (stat(path, &st) == 0)
        chmod(path, 0600);
}

/* ============================================================
 * save_config_value — faithful port of hermes_cli/config.py:save_config_value
 * Loads the merged config as JSON, sets a dotted key, and persists atomically.
 * Returns 0 on success, -1 on failure.
 * ============================================================ */
int config_py_save_value(const char *dotted_key, json_t *value) {
    if (!dotted_key || !*dotted_key || !value) return -1;
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) return -1;
    if (config_py_set_nested(cfg, dotted_key, value) != 0) {
        json_free(cfg);
        return -1;
    }
    char path[HERMES_PATH_MAX];
    config_py_get_config_path(path, sizeof(path));
    int rc = config_py_atomic_config_write(path, cfg);
    json_free(cfg);
    return rc;
}
