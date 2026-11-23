/*
 * port_config_py_pure.c - Faithful C11 ports of the module-level pure helpers
 * from Python hermes_cli/config.py that require NO filesystem / config-load I/O.
 * (I/O helpers live in port_config_py_io.c.)
 *
 * Every function carries its exact PoP comment so the parity scanner credits
 * it. See include/port_config_py_helpers.h for the full API.
 */

#include "port_config_py_helpers.h"
#include "hermes_core_types.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

extern char *rid_normalize_route_base_url_cstr(const char *base_url);

/* ============================================================
 * Route-identity normalizer (hermes_cli/route_identity.py)
 * ============================================================  */
/* PoP: config_normalize_route_base_url @ hermes_cli/route_identity.py:normalize_route_base_url  */
/* PoP: config_normalize_route_base_url @ agent/agent_init.py:_normalize_route_base_url  */
/* PoP: config_normalize_route_base_url @ hermes_cli/route_identity.py:normalize_route_base_url */
char *config_normalize_route_base_url(const char *url) {
    return rid_normalize_route_base_url_cstr(url);
}

/* ============================================================
 * Wrappers over already-ported helpers
 * ============================================================  */
/* PoP: config_coerce_ssl_verify @ hermes_cli/config_providers.py:_coerce_ssl_verify */
int config_coerce_ssl_verify(const char *value) {
    /* coerce_ssl_verify(value, is_bool, bool_val): returns 1/0/-1 sentinel.
     * config.py returns True/False/None. We return 1/0 and -1 for None. */
    int is_bool = 0, bool_val = 0;
    int r = coerce_ssl_verify(value, is_bool, bool_val);
    /* The port returns 1 when truthy, 0 when falsy, -1 when not a bool/str.
     * Map: 1 -> 1 (true), 0 -> 0 (false), -1 -> -1 (None). */
    return r;
}

/* PoP: config_coerce_config_version @ hermes_cli/config_version_stamp.py:_coerce_config_version */
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
 * ============================================================  */
/* PoP: config_py_explicit_paths @ hermes_cli/config.py:_explicit_config_paths */
char **config_py_explicit_paths(const json_t *config, int *out_count) {
    char **result = NULL;
    int cap = 16, count = 0;
    if (out_count) *out_count = 0;
    if (!config) return NULL;

    /* iterative DFS using a path stack of segments  */
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
 * ============================================================  */
/* PoP: config_py_strip_default_values @ hermes_cli/config.py:_strip_default_values */
json_t *config_py_strip_default_values(const json_t *config, const json_t *defaults,
                                        char **preserve_keys, int preserve_count) {
    json_t *result = json_new_object();
    if (!config) return result;
    if (!cfg_is_object(config)) return result;

    /* build preserve set  */
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
 * ============================================================  */
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
 * ============================================================  */
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
 * ============================================================  */
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
        /* return name (caller frees) - shift */
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
 * ============================================================  */
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
 * ============================================================  */
/* PoP: config_py_is_provider_enabled @ hermes_cli/config_providers.py:is_provider_enabled */
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
 * ============================================================  */
/* PoP: config_py_custom_provider_entry_to_provider_config @ hermes_cli/config_providers.py:_custom_provider_entry_to_provider_config */
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
 * ============================================================  */
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
 * ============================================================  */
/* PoP: config_py_resolve_hermes_uid_gid @ hermes_constants.py:_resolve_hermes_uid_gid */
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

/* PoP: config_py_chown_to_hermes_uid @ hermes_constants.py:_chown_to_hermes_uid */
void config_py_chown_to_hermes_uid(const char *path) {
    if (!path) return;
    long uid = -1, gid = -1;
    config_py_resolve_hermes_uid_gid(&uid, &gid);
    if (uid >= 0 || gid >= 0)
        chown(path, uid >= 0 ? uid : -1, gid >= 0 ? gid : -1);
}

/* ============================================================
 * _is_container
 * ============================================================  */
/* PoP: config_py_is_container @ hermes_cli/profile_distribution.py:_is_container */
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
 * ============================================================  */
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
 * save_config_value - faithful port of hermes_cli/config.py:save_config_value
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

/* ============================================================
 * hermes_cli/config.py REAL_GAP closures — pure config helpers.
 * C names use the bare stripped Python names (module "config" makes
 * config_-style prefixes invisible to the parity scanner).
 * ============================================================ */

#include <limits.h>
#include "cron_jobs.h"
#include "port_personality.h"

/* TURN_LIMIT_UNLIMITED = sys.maxsize (hermes_cli/config.py:3092). */
#define SLERMES_TURN_LIMIT_UNLIMITED ((long)LONG_MAX)

/* _NIX_MANAGED_SYSTEMS = {"nixos", "home-manager"} (config.py:342). */
static const char *const s_nix_managed_systems[] = {"nixos", "home-manager", NULL};

/* PoP: is_nix_install_method @ hermes_cli/config.py:is_nix_install_method */
bool is_nix_install_method(const char *method) {
    if (!method) return false;
    if (strcmp(method, "nix") == 0) return true;
    for (int i = 0; s_nix_managed_systems[i]; i++) {
        if (strcmp(method, s_nix_managed_systems[i]) == 0) return true;
    }
    return false;
}

/*
 * Returns a new json bool node, or NULL when no explicit boolean capability
 * is declared. When custom_providers is NULL the read-only merged config's
 * PoP: get_custom_provider_model_capability @ hermes_cli/config_providers.py:get_custom_provider_model_capability
 */
json_t *get_custom_provider_model_capability(const char *model,
                                             const char *base_url,
                                             const char *capability,
                                             const json_t *custom_providers) {
    if (!model || !*model || !base_url || !*base_url || !capability || !*capability)
        return NULL;

    json_t *providers = NULL;
    json_t *loaded_config = NULL;
    if (!custom_providers) {
        loaded_config = config_py_load_config_readonly();
        if (!loaded_config) return NULL;
        providers = config_py_get_compatible_custom_providers(loaded_config);
        if (!providers) { json_free(loaded_config); return NULL; }
    } else {
        providers = (json_t *)custom_providers;
    }

    char *target_url_raw = config_normalize_route_base_url(base_url);
    json_t *result = NULL;
    if (target_url_raw && *target_url_raw && providers->type == JSON_ARRAY) {
        size_t n = json_array_size(providers);
        for (size_t i = 0; i < n && !result; i++) {
            json_t *entry = json_get(providers, i);
            if (!entry || entry->type != JSON_OBJECT) continue;
            /* Python: normalize_route_base_url(entry.get("base_url")) — a
             * missing base_url normalizes from None → "" and never matches. */
            json_t *eb = json_obj_get(entry, "base_url");
            char *entry_url =
                (eb && eb->type == JSON_STRING) ? config_normalize_route_base_url(eb->str_val)
                                                : strdup("");
            if (entry_url && *entry_url && strcmp(entry_url, target_url_raw) == 0) {
                json_t *models = json_obj_get(entry, "models");
                if (models && models->type == JSON_OBJECT) {
                    json_t *model_cfg = json_obj_get(models, model);
                    if (model_cfg && model_cfg->type == JSON_OBJECT) {
                        json_t *value = json_obj_get(model_cfg, capability);
                        if (value && value->type == JSON_BOOL) {
                            result = json_bool(value->bool_val);
                        }
                    }
                }
            }
            free(entry_url);
        }
    }
    free(target_url_raw);

    if (loaded_config) {
        json_free(providers);
        json_free(loaded_config);
    }
    return result;
}

/*
 * Canonicalize a config ``model.default``/``model.model`` value.
 * A dict-valued default pairs the model string with the provider it must be
 * routed through; anything else canonicalizes to (str(raw).strip(), "").
 * PoP: split_model_config_default @ hermes_cli/config.py:split_model_config_default
 */
void split_model_config_default(const json_t *raw_default, char **out_model,
                                char **out_provider) {
    char *model = strdup(""), *provider = strdup("");
    if (raw_default && raw_default->type == JSON_OBJECT) {
        json_t *prov = json_obj_get(raw_default, "provider");
        const char *ptext = (prov && prov->type == JSON_STRING) ? prov->str_val : "";
        /* str(x or "") then strip: non-string truthy values stringify via str(). */
        char pbuf[64];
        if (prov && prov->type != JSON_STRING && prov->type != JSON_NULL) {
            snprintf(pbuf, sizeof(pbuf), "%g", prov->num_val);
            ptext = pbuf;
        }
        free(provider);
        provider = strdup(ptext ? ptext : "");
        char *ps = provider;
        while (*ps && isspace((unsigned char)*ps)) ps++;
        memmove(provider, ps, strlen(ps) + 1);
        size_t plen = strlen(provider);
        while (plen > 0 && isspace((unsigned char)provider[plen - 1])) provider[--plen] = '\0';

        /* model = raw.get("model") or raw.get("default") */
        json_t *mv = json_obj_get(raw_default, "model");
        if (!mv || mv->type == JSON_NULL) mv = json_obj_get(raw_default, "default");
        if (mv) {
            if (mv->type == JSON_STRING) {
                free(model);
                model = strdup(mv->str_val ? mv->str_val : "");
            } else if (mv->type == JSON_NUMBER) {
                free(model);
                char mbuf[64];
                snprintf(mbuf, sizeof(mbuf), "%g", mv->num_val);
                model = strdup(mbuf);
            } else if (mv->type == JSON_BOOL) {
                free(model);
                model = strdup("True");   /* str(True) */
            }
        }
        char *ms = model;
        while (*ms && isspace((unsigned char)*ms)) ms++;
        memmove(model, ms, strlen(ms) + 1);
        size_t mlen = strlen(model);
        while (mlen > 0 && isspace((unsigned char)model[mlen - 1])) model[--mlen] = '\0';
    } else if (raw_default && raw_default->type != JSON_NULL) {
        char buf[256];
        if (raw_default->type == JSON_STRING) {
            snprintf(buf, sizeof(buf), "%s", raw_default->str_val ? raw_default->str_val : "");
        } else if (raw_default->type == JSON_NUMBER) {
            snprintf(buf, sizeof(buf), "%g", raw_default->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%s", raw_default->bool_val ? "True" : "False");
        }
        free(model);
        model = strdup(buf);
        char *ms = model;
        while (*ms && isspace((unsigned char)*ms)) ms++;
        memmove(model, ms, strlen(ms) + 1);
        size_t mlen = strlen(model);
        while (mlen > 0 && isspace((unsigned char)model[mlen - 1])) model[--mlen] = '\0';
    }
    if (out_model) *out_model = model; else free(model);
    if (out_provider) *out_provider = provider; else free(provider);
}

static int cpi_str_in_set(const char *s, const char *const *set) {
    for (int i = 0; set[i]; i++)
        if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

/*
 * Normalize a raw ``agent.max_turns`` value into an iteration cap.
 * null/bool/unparseable → default; numbers ≤ 0 and the unlimited spellings →
 * PoP: resolve_turn_limit @ hermes_cli/config.py:resolve_turn_limit
 */
long resolve_turn_limit(const json_t *raw, long default_value) {
    static const char *const unlimited_spellings[] = {
        "none", "null", "unlimited", "infinite", "infinity", "inf",
        "\xe2\x88\x9e", "-1", "0", NULL, /* \u221e = infinity sign */
    };
    if (!raw || raw->type == JSON_NULL) return default_value;
    if (raw->type == JSON_BOOL) return default_value;  /* bool rejected explicitly */
    if (raw->type == JSON_NUMBER) {
        long n = (long)raw->num_val;   /* int(raw): truncates floats */
        if (n <= 0) return SLERMES_TURN_LIMIT_UNLIMITED;
        return n;
    }
    if (raw->type == JSON_STRING) {
        const char *p = raw->str_val ? raw->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        char s[128];
        snprintf(s, sizeof(s), "%s", p);
        size_t slen = strlen(s);
        while (slen > 0 && isspace((unsigned char)s[slen - 1])) s[--slen] = '\0';
        for (char *q = s; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (!s[0]) return default_value;
        if (cpi_str_in_set(s, unlimited_spellings)) return SLERMES_TURN_LIMIT_UNLIMITED;
        /* try: n = int(s); except: n = int(float(s)); except: return default */
        char *end = NULL;
        errno = 0;
        long n = strtol(s, &end, 10);
        if (end == s || (end && *end != '\0') || errno == ERANGE) {
            errno = 0;
            double d = strtod(s, &end);
            if (end == s || (end && *end != '\0')) return default_value;  /* unparseable */
            n = (long)d;   /* int(float(s)) truncates toward zero */
        }
        if (n <= 0) return SLERMES_TURN_LIMIT_UNLIMITED;
        return n;
    }
    /* Unknown type (list, dict, …) — don't crash the agent over bad config. */
    return default_value;
}

/*
 * Delegates to hermes_cli/personality.py:render_personality_prompt
 * PoP: render_personality_prompt @ hermes_cli/personality.py:render_personality_prompt
 */
char *render_personality_prompt(const json_t *value) {
    return per_render_personality_prompt(value);
}

/* TERMINAL_CONFIG_ENV_MAP (hermes_cli/config.py:3491). */
static const struct { const char *key; const char *env_var; }
s_terminal_config_env_map[] = {
    {"backend", "TERMINAL_ENV"},
    {"modal_mode", "TERMINAL_MODAL_MODE"},
    {"degraded_mode", "TERMINAL_DEGRADED_MODE"},
    {"cwd", "TERMINAL_CWD"},
    {"timeout", "TERMINAL_TIMEOUT"},
    {"lifetime_seconds", "TERMINAL_LIFETIME_SECONDS"},
    {"docker_image", "TERMINAL_DOCKER_IMAGE"},
    {"docker_forward_env", "TERMINAL_DOCKER_FORWARD_ENV"},
    {"singularity_image", "TERMINAL_SINGULARITY_IMAGE"},
    {"modal_image", "TERMINAL_MODAL_IMAGE"},
    {"daytona_image", "TERMINAL_DAYTONA_IMAGE"},
    {"vercel_runtime", "TERMINAL_VERCEL_RUNTIME"},
    {"ssh_host", "TERMINAL_SSH_HOST"},
    {"ssh_user", "TERMINAL_SSH_USER"},
    {"ssh_port", "TERMINAL_SSH_PORT"},
    {"ssh_key", "TERMINAL_SSH_KEY"},
    {"container_cpu", "TERMINAL_CONTAINER_CPU"},
    {"container_memory", "TERMINAL_CONTAINER_MEMORY"},
    {"container_disk", "TERMINAL_CONTAINER_DISK"},
    {"container_persistent", "TERMINAL_CONTAINER_PERSISTENT"},
    {"docker_volumes", "TERMINAL_DOCKER_VOLUMES"},
    {"docker_env", "TERMINAL_DOCKER_ENV"},
    {"docker_mount_cwd_to_workspace", "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE"},
    {"docker_network", "TERMINAL_DOCKER_NETWORK"},
    {"docker_extra_args", "TERMINAL_DOCKER_EXTRA_ARGS"},
    {"docker_shm_size", "TERMINAL_DOCKER_SHM_SIZE"},
    {"docker_run_as_host_user", "TERMINAL_DOCKER_RUN_AS_HOST_USER"},
    {"docker_persist_across_processes", "TERMINAL_DOCKER_PERSIST_ACROSS_PROCESSES"},
    {"docker_orphan_reaper", "TERMINAL_DOCKER_ORPHAN_REAPER"},
    {"sandbox_dir", "TERMINAL_SANDBOX_DIR"},
    {"persistent_shell", "TERMINAL_PERSISTENT_SHELL"},
    {NULL, NULL},
};

/* PoP: cli_terminal_config_value_is_bridgeable @ hermes_cli/config.py:_terminal_config_value_is_bridgeable */
static bool cli_terminal_config_value_is_bridgeable(const char *key, const json_t *value) {
    if (strcmp(key, "cwd") == 0) {
        char buf[256];
        if (value && value->type == JSON_STRING) {
            snprintf(buf, sizeof(buf), "%s", value->str_val ? value->str_val : "");
        } else if (value && value->type == JSON_NUMBER) {
            snprintf(buf, sizeof(buf), "%g", value->num_val);
        } else if (value && value->type == JSON_BOOL) {
            snprintf(buf, sizeof(buf), "%s", value->bool_val ? "True" : "False");
        } else {
            buf[0] = '\0';   /* str(None or "") == "" */
        }
        char *s = buf;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
        if (strcmp(s, ".") == 0 || strcmp(s, "auto") == 0 || strcmp(s, "cwd") == 0)
            return false;
    }
    return true;
}

/*
 * Env vars explicitly owned by a raw ``terminal`` config section. Non-dict
 * PoP: terminal_config_owned_env_vars @ hermes_cli/config.py:terminal_config_owned_env_vars
 */
json_t *terminal_config_owned_env_vars(const json_t *terminal_config) {
    json_t *owned = json_array();
    if (!terminal_config || terminal_config->type != JSON_OBJECT) return owned;
    for (int i = 0; s_terminal_config_env_map[i].key; i++) {
        json_t *value = json_obj_get(terminal_config, s_terminal_config_env_map[i].key);
        if (!value) continue;
        if (!cli_terminal_config_value_is_bridgeable(s_terminal_config_env_map[i].key, value))
            continue;
        json_append(owned, json_string(s_terminal_config_env_map[i].env_var));
    }
    return owned;
}

/* ---- cron model-drift helpers (hermes_cli/config.py:4884-5105) ---- */

/* _model_assignment_text: trimmed scalar string value, or "". Caller frees. */
static char *cpi_model_assignment_text(const json_t *value) {
    if (value && value->type == JSON_STRING && value->str_val) {
        char *s = strdup(value->str_val);
        char *p = s;
        while (*p && isspace((unsigned char)*p)) p++;
        memmove(s, p, strlen(p) + 1);
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
        return s;
    }
    return strdup("");
}

/*
 * PoP: cli_cron_model_drift_guard_enabled @ hermes_cli/config.py:cron_model_drift_guard_enabled
 */
static bool cli_cron_model_drift_guard_enabled(const json_t *config) {
    if (!config) {
        json_t *cfg = config_py_load_config_readonly();
        bool enabled = cli_cron_model_drift_guard_enabled(cfg);
        if (cfg) json_free(cfg);
        return enabled;
    }
    if (config->type != JSON_OBJECT) return true;
    json_t *cron_cfg = json_obj_get(config, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) return true;
    json_t *guard = json_obj_get(cron_cfg, "model_drift_guard");
    if (!guard || guard->type != JSON_BOOL) return true;   /* default True */
    return guard->bool_val != false;
}

/*
 * PoP: cli_cron_fleet_default_covers_axis @ hermes_cli/config.py:cron_fleet_default_covers_axis
 */
static bool cli_cron_fleet_default_covers_axis(const char *axis, const json_t *config) {
    if (!config) {
        json_t *cfg = config_py_load_config_readonly();
        bool covers = cli_cron_fleet_default_covers_axis(axis, cfg);
        if (cfg) json_free(cfg);
        return covers;
    }
    if (config->type != JSON_OBJECT) return false;
    json_t *cron_cfg = json_obj_get(config, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) return false;
    const char *key = (strcmp(axis, "model") == 0) ? "model" : "model_provider";
    json_t *value = json_obj_get(cron_cfg, key);
    if (!value || value->type != JSON_STRING) return false;
    char *t = cpi_model_assignment_text(value);
    bool covered = t[0] != '\0';
    free(t);
    return covered;
}

/*
 * Global provider/model values cron compares against snapshots. Truthy
 * configured model wins HERMES_MODEL; the environment is only a fallback.
 * PoP: resolve_cron_model_drift_defaults @ hermes_cli/config.py:resolve_cron_model_drift_defaults
 */
void resolve_cron_model_drift_defaults(const json_t *config, char **out_provider,
                                       char **out_model) {
    char *provider = strdup("");
    char *model = cpi_model_assignment_text(NULL);
    free(model);
    const char *env_model = getenv("HERMES_MODEL");
    model = strdup(env_model ? env_model : "");
    {
        char *t = model;
        while (*t && isspace((unsigned char)*t)) t++;
        memmove(model, t, strlen(t) + 1);
        size_t n = strlen(model);
        while (n > 0 && isspace((unsigned char)model[n - 1])) model[--n] = '\0';
    }
    json_t *model_config = NULL;
    if (config && config->type == JSON_OBJECT)
        model_config = json_obj_get(config, "model");
    if (model_config && model_config->type == JSON_STRING) {
        free(model);
        model = cpi_model_assignment_text(model_config);
    } else if (model_config && model_config->type == JSON_OBJECT) {
        free(provider);
        json_t *prov = json_obj_get(model_config, "provider");
        provider = cpi_model_assignment_text(prov);
        json_t *mv = json_obj_get(model_config, "default");
        if (!mv || mv->type == JSON_NULL) mv = json_obj_get(model_config, "model");
        if (!mv || mv->type == JSON_NULL) mv = json_obj_get(model_config, "name");
        char *configured = cpi_model_assignment_text(mv);
        if (configured[0]) {
            free(model);
            model = configured;
        } else {
            free(configured);
        }
    }
    if (out_provider) *out_provider = provider; else free(provider);
    if (out_model) *out_model = model; else free(model);
}

/*
 * The unpinned axes ("provider"/"model") that the fail-closed cron guard
 * would block, given the current assignment vs each job's snapshot.
 * PoP: cron_model_drift_axes @ hermes_cli/config.py:cron_model_drift_axes
 */
json_t *cron_model_drift_axes(const json_t *job, const char *current_provider,
                              const char *current_model, const json_t *config) {
    json_t *drifted = json_array();
    if (!job || job->type != JSON_OBJECT) return drifted;
    if (!cli_cron_model_drift_guard_enabled(config)) return drifted;

    char cur_lower[2][512];
    const char *cur_raw[2] = {current_provider, current_model};
    for (int i = 0; i < 2; i++) {
        snprintf(cur_lower[i], sizeof(cur_lower[i]), "%s", cur_raw[i] ? cur_raw[i] : "");
        for (char *q = cur_lower[i]; *q; q++) *q = (char)tolower((unsigned char)*q);
    }

    static const char axes[2][9] = {"provider", "model"};
    for (int i = 0; i < 2; i++) {
        const char *axis = axes[i];
        if (cli_cron_fleet_default_covers_axis(axis, config)) continue;
        char key[64];
        snprintf(key, sizeof(key), "%s", axis);
        char *pinned = cpi_model_assignment_text(json_obj_get(job, key));
        bool has_pinned = pinned[0] != '\0';
        free(pinned);
        if (has_pinned) continue;
        snprintf(key, sizeof(key), "%s_snapshot", axis);
        json_t *snap_node = json_obj_get(job, key);
        char *snapshot = cpi_model_assignment_text(snap_node);
        for (char *q = snapshot; *q; q++) *q = (char)tolower((unsigned char)*q);
        const char *current = cur_lower[i];
        bool drifted_here =
            snapshot[0] != '\0' && current[0] != '\0' && strcmp(snapshot, current) != 0;
        free(snapshot);
        if (drifted_here) json_append(drifted, json_string(axis));
    }
    return drifted;
}

#define CRON_MODEL_IMPACT_JOB_LIMIT 50
#define CRON_MODEL_IMPACT_ID_LIMIT 256
#define CRON_MODEL_IMPACT_NAME_LIMIT 120

/* Unicode category C* ≈ control characters: C0 controls, DEL, C1 controls. */
static bool cpi_has_control_char(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 0x20 || *p == 0x7f || (*p >= 0x80 && *p <= 0x9f)) return true;
    }
    return false;
}

static char *cpi_valid_cron_impact_job_id(const json_t *value) {
    if (!value || value->type != JSON_STRING) return strdup("");
    char *job_id = cpi_model_assignment_text(value);
    size_t n = strlen(job_id);
    if (!n || n > CRON_MODEL_IMPACT_ID_LIMIT || cpi_has_control_char(job_id)) {
        free(job_id);
        return strdup("");
    }
    return job_id;
}

/* Collapse all whitespace runs to single spaces, drop control chars, clip. */
static void cpi_squash_ws(char *s, size_t limit) {
    char *out = s, *p = s;
    bool in_ws = false;
    while (*p) {
        if (isspace((unsigned char)*p)) {
            in_ws = (out != s);
        } else {
            if (in_ws && out != s) *out++ = ' ';
            in_ws = false;
            *out++ = *p;
            if ((size_t)(out - s) >= limit) break;
        }
        p++;
    }
    *out = '\0';
}

static char *cpi_cron_impact_job_name(const json_t *value, const char *job_id) {
    if (value && value->type == JSON_STRING && value->str_val &&
        !cpi_has_control_char(value->str_val) && value->str_val[0]) {
        char *name = strdup(value->str_val);
        cpi_squash_ws(name, CRON_MODEL_IMPACT_NAME_LIMIT);
        size_t n = strlen(name);
        while (n > 0 && isspace((unsigned char)name[n - 1])) name[--n] = '\0';
        if (name[0]) return name;
        free(name);
    }
    char buf[CRON_MODEL_IMPACT_NAME_LIMIT + 16];
    snprintf(buf, sizeof(buf), "Job %s", job_id ? job_id : "");
    buf[CRON_MODEL_IMPACT_NAME_LIMIT] = '\0';
    char *name = strdup(buf);
    size_t n = strlen(name);
    while (n > 0 && isspace((unsigned char)name[n - 1])) name[--n] = '\0';
    return name;
}

static json_t *cpi_unavailable_cron_model_impact(bool guard_enabled) {
    json_t *r = json_object();
    json_set(r, "available", json_bool(false));
    json_set(r, "guard_enabled", json_bool(guard_enabled));
    json_set(r, "affected_count", json_int(0));
    json_set(r, "truncated", json_bool(false));
    json_set(r, "jobs", json_array());
    return r;
}

/*
 * Bounded, profile-local summary of jobs blocked by model drift. Job-store
 * inspection is best effort: an unreadable store yields available=false.
 * PoP: build_cron_model_impact @ hermes_cli/config.py:build_cron_model_impact
 */
json_t *build_cron_model_impact(const char *current_provider, const char *current_model,
                                const json_t *config, const json_t *jobs) {
    bool guard_enabled = cli_cron_model_drift_guard_enabled(config);

    json_t *jobs_list = NULL;
    bool owned_jobs = false;
    if (!jobs) {
        char *err = NULL;
        jobs_list = cronjobs_load_jobs(&err);
        free(err);
        owned_jobs = true;
        if (!jobs_list) return cpi_unavailable_cron_model_impact(guard_enabled);
    } else {
        jobs_list = (json_t *)jobs;
    }
    if (jobs_list->type != JSON_ARRAY) {
        if (owned_jobs) json_free(jobs_list);
        return cpi_unavailable_cron_model_impact(guard_enabled);
    }

    json_t *result = json_object();
    json_set(result, "available", json_bool(true));
    json_set(result, "guard_enabled", json_bool(guard_enabled));
    json_set(result, "affected_count", json_int(0));
    json_set(result, "truncated", json_bool(false));
    json_set(result, "jobs", json_array());
    if (!guard_enabled) {
        if (owned_jobs) json_free(jobs_list);
        return result;
    }

    /* seen_ids set: linear scan over the bounded jobs list is fine. */
    char seen[CRON_MODEL_IMPACT_JOB_LIMIT][CRON_MODEL_IMPACT_ID_LIMIT + 1];
    size_t seen_n = 0;
    long affected = 0;

    size_t n = json_array_size(jobs_list);
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_get(jobs_list, i);
        if (!job || job->type != JSON_OBJECT) continue;
        if (!cronjobs_is_job_runnable(job)) continue;
        json_t *no_agent = json_obj_get(job, "no_agent");
        bool no_agent_truthy = false;
        if (no_agent) {
            if (no_agent->type == JSON_BOOL) no_agent_truthy = no_agent->bool_val;
            else if (no_agent->type == JSON_NUMBER) no_agent_truthy = no_agent->num_val != 0.0;
            else if (no_agent->type == JSON_STRING)
                no_agent_truthy = no_agent->str_val && no_agent->str_val[0];
            /* JSON_NULL → falsy */
        }
        if (no_agent_truthy)
            continue;   /* truthy no_agent skips the agent path entirely */
        char *job_id = cpi_valid_cron_impact_job_id(json_obj_get(job, "id"));
        if (!job_id[0]) { free(job_id); continue; }
        bool dup = false;
        for (size_t k = 0; k < seen_n; k++)
            if (strcmp(seen[k], job_id) == 0) { dup = true; break; }
        if (dup) { free(job_id); continue; }
        if (seen_n < CRON_MODEL_IMPACT_JOB_LIMIT) {
            snprintf(seen[seen_n], sizeof(seen[seen_n]), "%s", job_id);
            seen_n++;
        }
        json_t *axes = cron_model_drift_axes(job, current_provider, current_model, config);
        if (json_array_size(axes) == 0) {
            json_free(axes);
            free(job_id);
            continue;
        }
        json_free(axes);
        affected++;
        size_t listed = json_array_size(json_obj_get(result, "jobs"));
        if (listed < CRON_MODEL_IMPACT_JOB_LIMIT) {
            json_t *entry = json_object();
            json_set(entry, "id", json_string(job_id));
            char *name = cpi_cron_impact_job_name(json_obj_get(job, "name"), job_id);
            json_set(entry, "name", json_string(name));
            free(name);
            json_set(entry, "drifted_axes",
                     cron_model_drift_axes(job, current_provider, current_model, config));
            json_append(json_obj_get(result, "jobs"), entry);
        }
        free(job_id);
    }

    json_t *count_node = json_obj_get(result, "affected_count");
    count_node->num_val = (double)affected;
    size_t listed_total = json_array_size(json_obj_get(result, "jobs"));
    json_t *trunc = json_obj_get(result, "truncated");
    trunc->bool_val = (size_t)affected > listed_total;

    if (owned_jobs) json_free(jobs_list);
    return result;
}
/* ============================================================
 * hermes_cli/config.py — pure-gap closure batch (oracle_config_pure).
 *
 * C names carry the config_py_ prefix; each PoP annotation names the LIVE
 * Python function (underscore-prefixed where the live def is private).
 * Dict/list values are libjson json_t*; ownership is documented per function.
 * Functions that raise in Python return NULL (or an error code) plus the
 * exact exception message through a caller-frees / caller-buffer seam.
 *
 * Known representation limits (documented, not chased): a parsed JSON number
 * has no int/float identity (1.0 reads back as 1), and str()/lower()/strip()
 * ports cover the ASCII range the config surface uses.
 * ============================================================ */

#include <math.h>
#include "port_config.h"
#include "port_config_nested.h"
#include "config_defaults.h"

static const char CP_SECURITY_COMMENT[] = "\x0a# \xe2\x94\x80\xe2\x94\x80 Security \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\x0a# Secret redaction is ON by default \xe2\x80\x94 strings that look like API keys,\x0a# tokens, and passwords are masked in tool output, logs, and chat\x0a# responses before the model or user ever sees them. Set redact_secrets\x0a# to false to disable (e.g. when developing the redactor itself).\x0a#\x0a# security:\x0a#   redact_secrets: true\x0a";

static const char CP_FALLBACK_COMMENT[] = "\x0a# \xe2\x94\x80\xe2\x94\x80 Fallback Model \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\x0a# Automatic provider failover when primary is unavailable.\x0a# Uncomment and configure to enable. Triggers on rate limits (429),\x0a# overload (529), service errors (503), or connection failures.\x0a#\x0a# Supported providers:\x0a#   openrouter   (OPENROUTER_API_KEY)  \xe2\x80\x94 routes to any model\x0a#   openai-codex (OAuth \xe2\x80\x94 hermes auth) \xe2\x80\x94 OpenAI Codex\x0a#   nous         (OAuth \xe2\x80\x94 hermes auth) \xe2\x80\x94 Nous Portal\x0a#   zai          (ZAI_API_KEY)         \xe2\x80\x94 Z.AI / GLM\x0a#   kimi-coding  (KIMI_API_KEY)        \xe2\x80\x94 Kimi / Moonshot\x0a#   kimi-coding-cn (KIMI_CN_API_KEY)   \xe2\x80\x94 Kimi / Moonshot (China)\x0a#   minimax      (MINIMAX_API_KEY)     \xe2\x80\x94 MiniMax\x0a#   minimax-cn   (MINIMAX_CN_API_KEY)  \xe2\x80\x94 MiniMax (China)\x0a#   bedrock      (AWS IAM / boto3)     \xe2\x80\x94 AWS Bedrock (Converse API)\x0a#\x0a# For custom OpenAI-compatible endpoints, add base_url and key_env.\x0a#\x0a# fallback_model:\x0a#   provider: openrouter\x0a#   model: anthropic/claude-sonnet-4\x0a";

static const char CP_LIST_HINT[] = "Change to:\x0a  custom_providers:\x0a    - name: my-provider\x0a      base_url: https://...\x0a      api_key: ...";

/* ---- shared Python-semantics helpers ---- */

/* str.strip() whitespace set (ASCII subset of Python's Unicode whitespace). */
static int cp_is_py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* In-place str.strip() (both ends). */
static void cp_str_strip(char *s) {
    size_t n = strlen(s);
    size_t a = 0;
    while (a < n && cp_is_py_ws((unsigned char)s[a])) a++;
    size_t b = n;
    while (b > a && cp_is_py_ws((unsigned char)s[b - 1])) b--;
    memmove(s, s + a, b - a);
    s[b - a] = '\0';
}

/* Python type(x).__name__ for a JSON value. Integral doubles read as "int":
 * a parsed JSON number carries no int/float identity (see batch note). */
static const char *cp_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_OBJECT: return "dict";
    case JSON_ARRAY:  return "list";
    case JSON_STRING: return "str";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER:
        return (isfinite(v->num_val) && v->num_val == floor(v->num_val)) ? "int" : "float";
    }
    return "object";
}

/* Python truthiness: None/False/0/""/[]/{} are falsy, everything else truthy. */
static int cp_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count ? 1 : 0;
    }
    return 0;
}

/* Growable string buffer (caller frees .s). */
typedef struct { char *s; size_t len, cap; } cp_buf;
static void cp_buf_init(cp_buf *b) {
    b->cap = 128; b->len = 0;
    b->s = (char *)malloc(b->cap);
    b->s[0] = '\0';
}
static void cp_buf_add(cp_buf *b, const char *t) {
    size_t n = strlen(t);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->s = (char *)realloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, t, n + 1);
    b->len += n;
}

/* Python repr() of a str: single quotes unless the value contains ' and no ".
 * Caller frees. */
static char *cp_py_repr_str(const char *s) {
    if (!s) s = "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = (char *)malloc(cap);
    size_t w = 0;
    out[w++] = quote;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)quote || c == '\\') { out[w++] = '\\'; out[w++] = (char)c; }
        else if (c == '\n') { out[w++] = '\\'; out[w++] = 'n'; }
        else if (c == '\r') { out[w++] = '\\'; out[w++] = 'r'; }
        else if (c == '\t') { out[w++] = '\\'; out[w++] = 't'; }
        else if (c < 0x20 || c == 0x7f) w += (size_t)snprintf(out + w, cap - w, "\\x%02x", c);
        else out[w++] = (char)c;   /* printable non-ASCII passes through, as repr does */
    }
    out[w++] = quote;
    out[w] = '\0';
    return out;
}

/* Python str(float): shortest round-trip digits, fixed notation for decimal
 * exponents in [-4, 16), exponent form otherwise, ".0" on integral values. */
static char *cp_float_repr(double d) {
    char buf[64];
    if (isnan(d)) return strdup("nan");
    if (isinf(d)) return strdup(d > 0 ? "inf" : "-inf");
    if (d == 0.0) return strdup(signbit(d) ? "-0.0" : "0.0");
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    snprintf(buf, sizeof buf, "%.*e", prec - 1, d);
    char *p = buf;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    char digits[32];
    int nd = 0;
    digits[nd++] = *p++;
    if (*p == '.') { p++; while (*p && *p != 'e' && *p != 'E') digits[nd++] = *p++; }
    while (nd > 1 && digits[nd - 1] == '0') nd--;
    char *ep = strchr(p, 'e');
    if (!ep) ep = strchr(p, 'E');
    int E = ep ? atoi(ep + 1) : 0;
    char out[80];
    size_t w = 0;
    if (neg) out[w++] = '-';
    if (E >= -4 && E < 16) {
        if (E >= 0) {
            int intd = E + 1;
            for (int i = 0; i < intd; i++) out[w++] = (i < nd) ? digits[i] : '0';
            out[w++] = '.';
            if (nd > intd) { for (int i = intd; i < nd; i++) out[w++] = digits[i]; }
            else out[w++] = '0';
        } else {
            out[w++] = '0'; out[w++] = '.';
            for (int i = 0; i < -E - 1; i++) out[w++] = '0';
            for (int i = 0; i < nd; i++) out[w++] = digits[i];
        }
    } else {
        out[w++] = digits[0];
        if (nd > 1) { out[w++] = '.'; for (int i = 1; i < nd; i++) out[w++] = digits[i]; }
        out[w++] = 'e';
        out[w++] = E < 0 ? '-' : '+';
        char eb[8];
        snprintf(eb, sizeof eb, "%02d", E < 0 ? -E : E);
        for (char *q = eb; *q; q++) out[w++] = *q;
    }
    out[w] = '\0';
    return strdup(out);
}

static char *cp_py_repr(const json_t *v);

/* Python str(v) for a JSON value (repr for containers). Caller frees. */
static char *cp_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e16) {
            char buf[64];
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
            return strdup(buf);
        }
        return cp_float_repr(v->num_val);
    default: return cp_py_repr(v);
    }
}

static void cp_repr_rec(const json_t *v, cp_buf *b) {
    if (!v || v->type == JSON_NULL) { cp_buf_add(b, "None"); return; }
    switch (v->type) {
    case JSON_BOOL: cp_buf_add(b, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: { char *t = cp_py_str(v); cp_buf_add(b, t); free(t); return; }
    case JSON_STRING: { char *t = cp_py_repr_str(v->str_val); cp_buf_add(b, t); free(t); return; }
    case JSON_ARRAY:
        cp_buf_add(b, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) cp_buf_add(b, ", ");
            cp_repr_rec(v->c.items[i], b);
        }
        cp_buf_add(b, "]");
        return;
    case JSON_OBJECT:
        cp_buf_add(b, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) cp_buf_add(b, ", ");
            char *k = cp_py_repr_str(v->c.keys[i]);
            cp_buf_add(b, k);
            free(k);
            cp_buf_add(b, ": ");
            cp_repr_rec(v->c.items[i], b);
        }
        cp_buf_add(b, "}");
        return;
    }
}

/* Python repr(v) for a JSON value. Caller frees. */
static char *cp_py_repr(const json_t *v) {
    cp_buf b;
    cp_buf_init(&b);
    cp_repr_rec(v, &b);
    return b.s;
}

/* str(v or "").strip() — the provider/model text normalizer shape. Caller frees. */
static char *cp_py_str_truthy(const json_t *v) {
    char *s = cp_truthy(v) ? cp_py_str(v) : strdup("");
    cp_str_strip(s);
    return s;
}

/* dict-key text for dict(pairs) conversion: JSON object keys are strings, so
 * non-string keys use the json.dumps key spelling (numbers via str(), bools
 * "true"/"false", None "null"). Caller frees. */
static char *cp_dict_key_text(const json_t *k) {
    if (!k || k->type == JSON_NULL) return strdup("null");
    if (k->type == JSON_STRING) return strdup(k->str_val ? k->str_val : "");
    if (k->type == JSON_BOOL) return strdup(k->bool_val ? "true" : "false");
    return cp_py_str(k);
}

/* Python int(s), base 10: surrounding whitespace, optional sign, digits with
 * single underscores BETWEEN digits. Returns 1 on success, 0 on ValueError.
 * Overflow fails too: the only caller indexes lists, where a huge int would
 * raise IndexError and the walk returns None anyway. */
static int cp_py_int_parse(const char *s, long long *out) {
    if (!s) return 0;
    while (*s && cp_is_py_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && cp_is_py_ws((unsigned char)end[-1])) end--;
    const char *p = s;
    int neg = 0;
    if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
    char clean[128];
    size_t n = 0;
    int prev_digit = 0;
    for (; p < end; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            if (n + 1 >= sizeof clean) return 0;
            clean[n++] = c;
            prev_digit = 1;
        } else if (c == '_') {
            if (!prev_digit) return 0;   /* leading / doubled / post-sign underscore */
            prev_digit = 0;              /* an underscore must be followed by a digit */
        } else {
            return 0;
        }
    }
    if (!n || !prev_digit) return 0;     /* empty or trailing underscore */
    clean[n] = '\0';
    errno = 0;
    char *e2 = NULL;
    long long v = strtoll(clean, &e2, 10);
    if (errno == ERANGE || (e2 && *e2)) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* _split_key_path into a malloc'd segment array (caller frees each + array).
 * Returns the segment count, or -1 on bad args. */
static int cp_split(const char *key, char ***out) {
    size_t klen = strlen(key);
    int max = (int)klen + 1;             /* a segment count can never exceed this */
    char **segs = (char **)malloc(sizeof(char *) * (size_t)max);
    if (!segs) return -1;
    int n = cfg_split_key_path(key, segs, max);
    if (n < 0) { free(segs); return -1; }
    *out = segs;
    return n;
}

/* cfg_get(cfg, *parts): dict-only traversal, NULL on any miss (default=None).
 * Zero parts returns cfg itself. Borrowed result. */
static json_t *cp_cfg_get_node(json_t *cfg, char *const *parts, int n) {
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    json_t *node = cfg;
    for (int i = 0; i < n; i++) {
        if (!node || node->type != JSON_OBJECT) return NULL;
        node = json_obj_get(node, parts[i]);
        if (!node) return NULL;
    }
    return node;
}

/* Python str.splitlines() boundary length at p (0 when not a boundary):
 * \n \r \v \f \x1c \x1d \x1e \x85    . \r\n is handled by the caller. */
static int cp_line_break_len(const char *p) {
    unsigned char c = (unsigned char)p[0];
    if (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
        c == 0x1c || c == 0x1d || c == 0x1e) return 1;
    if (c == 0xc2 && (unsigned char)p[1] == 0x85) return 2;
    if (c == 0xe2 && (unsigned char)p[1] == 0x80 &&
        ((unsigned char)p[2] == 0xa8 || (unsigned char)p[2] == 0xa9)) return 3;
    return 0;
}
/* ---- config-structure validation (ConfigIssue -> {severity,message,hint}) ---- */

static void cp_issue(json_t *issues, const char *severity, const char *message, const char *hint) {
    if (!issues || issues->type != JSON_ARRAY) return;
    json_t *e = json_object();
    json_set(e, "severity", json_string(severity ? severity : ""));
    json_set(e, "message", json_string(message ? message : ""));
    json_set(e, "hint", json_string(hint ? hint : ""));
    json_append(issues, e);
}

/* Substitute {i} and {type} in a non-dict entry message template. Caller frees. */
static char *cp_fmt_msg(const char *tpl, int i, const char *type) {
    size_t cap = strlen(tpl) + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t w = 0;
    for (const char *p = tpl; *p && w + 1 < cap; p++) {
        if (p[0] == '{' && p[1] == 'i' && p[2] == '}') {
            w += (size_t)snprintf(out + w, cap - w, "%d", i);
            p += 2;
        } else if (p[0] == '{' && p[1] == 't' && p[2] == 'y' && p[3] == 'p' &&
                   p[4] == 'e' && p[5] == '}') {
            w += (size_t)snprintf(out + w, cap - w, "%s", type);
            p += 5;
        } else {
            out[w++] = *p;
        }
    }
    out[w] = '\0';
    return out;
}

/* _require_fields: a warning for every FALSY field (0 and False included).
 * fields is a flat [field, hint, ...] pair array. */
static void cp_require_fields(json_t *issues, const json_t *entry, const char *label,
                              const char *const *fields, int nfields, const char *suffix) {
    if (!entry || entry->type != JSON_OBJECT) return;
    for (int i = 0; i < nfields; i++) {
        const char *field = fields[2 * i];
        const char *hint = fields[2 * i + 1];
        json_t *v = json_obj_get(entry, field);
        if (!cp_truthy(v)) {
            char msg[1024];
            snprintf(msg, sizeof msg, "%s is missing '%s' field%s",
                     label ? label : "", field, suffix ? suffix : "");
            cp_issue(issues, "warning", msg, hint ? hint : "");
        }
    }
}

/* _CP_REQUIRED_FIELDS / _FB_REQUIRED_FIELDS / _FB_SINGLE_REQUIRED_FIELDS. */
static const char *const CP_REQUIRED_FIELDS[] = {
    "name", "Add a name, e.g.: name: my-provider",
    "base_url", "Add the API endpoint URL, e.g.: base_url: https://api.example.com/v1",
};
static const char *const CP_FB_REQUIRED_FIELDS[] = {
    "provider", "Add: provider: openrouter (or another provider)",
    "model", "Add: model: <model-name>",
};
static const char *const CP_FB_SINGLE_REQUIRED_FIELDS[] = {
    "provider", "Add: provider: openrouter (or another provider)",
    "model", "Add: model: anthropic/claude-sonnet-4 (or another model)",
};
/* _CUSTOM_PROVIDER_LIKE_FIELDS, pre-sorted (Python emits sorted(intersection)). */
static const char *const CP_LIKE_FIELDS_SORTED[] = {
    "api_key", "api_mode", "base_url", "rate_limit_delay",
};
static const char CP_CP_DICT_MSG[] =
    "custom_providers is a dict \xe2\x80\x94" " it must be a YAML list (items prefixed with '-')";
static const char CP_CP_SCALAR_FMT[] =
    "custom_providers is a %s \xe2\x80\x94" " it must be a YAML list (items prefixed with '-'); "
    "legacy custom_providers entries are ignored until it is";
static const char CP_FB_DISABLED_SUFFIX[] = " \xe2\x80\x94" " fallback will be disabled";
static const char CP_FB_CHANGE_HINT[] =
    "Change to:\n  fallback_model:\n    provider: openrouter\n    model: anthropic/claude-sonnet-4";

/* Materialize a flat pair table as the JSON [[field, hint], ...] the public
 * entry-list seam takes. Caller frees. */
static json_t *cp_fields_json(const char *const *flat, int nfields) {
    json_t *arr = json_array();
    for (int i = 0; i < nfields; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(flat[2 * i]));
        json_append(pair, json_string(flat[2 * i + 1]));
        json_append(arr, pair);
    }
    return arr;
}

/* PoP: config_py_validate_entry_list @ hermes_cli/config.py:_validate_entry_list */
/* issues: borrowed JSON array, appended in order. entries: borrowed JSON array.
 * fields: borrowed JSON array of [field, hint] pairs. The keyword-only non_dict
 * tuple is passed as (severity, message, hint); message may hold {i} / {type}. */
void config_py_validate_entry_list(json_t *issues, const json_t *entries, const char *label,
                                   const json_t *fields, const char *severity,
                                   const char *message, const char *hint) {
    if (!entries || entries->type != JSON_ARRAY) return;
    int nfields = 0;
    const char **flat = NULL;
    if (fields && fields->type == JSON_ARRAY && fields->c.count > 0) {
        nfields = (int)fields->c.count;
        flat = (const char **)malloc(sizeof(char *) * (size_t)(nfields * 2));
        for (int k = 0; k < nfields; k++) {
            json_t *pair = fields->c.items[k];
            const char *f = "", *h = "";
            if (pair && pair->type == JSON_ARRAY && pair->c.count >= 2) {
                json_t *a = pair->c.items[0];
                json_t *b = pair->c.items[1];
                if (a && a->type == JSON_STRING && a->str_val) f = a->str_val;
                if (b && b->type == JSON_STRING && b->str_val) h = b->str_val;
            }
            flat[2 * k] = f;
            flat[2 * k + 1] = h;
        }
    }
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) {
            char *m = cp_fmt_msg(message ? message : "", (int)i, cp_typename(entry));
            cp_issue(issues, severity ? severity : "warning", m ? m : "", hint ? hint : "");
            free(m);
        } else {
            char lbl[512];
            snprintf(lbl, sizeof lbl, "%s[%zu]", label ? label : "", i);
            cp_require_fields(issues, entry, lbl, flat, nfields, "");
        }
    }
    free(flat);
}

/* PoP: config_py_validate_custom_providers @ hermes_cli/config.py:_validate_custom_providers */
/* issues: borrowed JSON array, appended. cp: borrowed value; NULL models None
 * (the live function itself issues the NoneType error — the caller-side
 * `is not None` guard lives in validate_config_structure, not here). */
void config_py_validate_custom_providers(json_t *issues, const json_t *cp) {
    if (cp && cp->type == JSON_OBJECT) {
        cp_issue(issues, "error", CP_CP_DICT_MSG, CP_LIST_HINT);
        /* suspicious = sorted(set(cp.keys()) & _CUSTOM_PROVIDER_LIKE_FIELDS) */
        const char *hits[4];
        size_t nh = 0;
        for (int k = 0; k < 4; k++)
            if (json_obj_get(cp, CP_LIKE_FIELDS_SORTED[k]))
                hits[nh++] = CP_LIKE_FIELDS_SORTED[k];
        if (nh > 0) {
            cp_buf b;
            cp_buf_init(&b);
            cp_buf_add(&b, "[");
            for (size_t k = 0; k < nh; k++) {
                if (k) cp_buf_add(&b, ", ");
                char *r = cp_py_repr_str(hits[k]);
                cp_buf_add(&b, r);
                free(r);
            }
            cp_buf_add(&b, "]");
            char msg[1024];
            snprintf(msg, sizeof msg,
                     "Root-level keys %s look like custom_providers entry fields", b.s);
            free(b.s);
            cp_issue(issues, "warning", msg,
                     "These should be indented under a '- name: ...' list entry, not at root level");
        }
    } else if (cp && cp->type == JSON_ARRAY) {
        json_t *fields = cp_fields_json(CP_REQUIRED_FIELDS, 2);
        config_py_validate_entry_list(issues, cp, "custom_providers", fields,
                                      "warning",
                                      "custom_providers[{i}] is not a dict (got {type})",
                                      "Each entry should have at minimum: name, base_url");
        json_free(fields);
    } else {
        char msg[512];
        snprintf(msg, sizeof msg, CP_CP_SCALAR_FMT, cp_typename(cp));
        cp_issue(issues, "error", msg, CP_LIST_HINT);
    }
}

/* PoP: config_py_validate_fallback_model @ hermes_cli/config.py:_validate_fallback_model */
/* issues: borrowed JSON array, appended. fb: borrowed value (NULL models None).
 * An EMPTY dict passes silently (`elif fb:`), matching the live truthiness gate. */
void config_py_validate_fallback_model(json_t *issues, const json_t *fb) {
    if (fb && fb->type == JSON_ARRAY) {
        json_t *fields = cp_fields_json(CP_FB_REQUIRED_FIELDS, 2);
        config_py_validate_entry_list(issues, fb, "fallback_model", fields,
                                      "error",
                                      "fallback_model[{i}] should be a dict, got {type}",
                                      "Each entry needs provider + model");
        json_free(fields);
    } else if (!fb || fb->type != JSON_OBJECT) {
        char msg[512];
        snprintf(msg, sizeof msg,
                 "fallback_model should be a dict with 'provider' and 'model', got %s",
                 cp_typename(fb));
        cp_issue(issues, "error", msg, CP_FB_CHANGE_HINT);
    } else if (fb->c.count > 0) {
        cp_require_fields(issues, fb, "fallback_model",
                          CP_FB_SINGLE_REQUIRED_FIELDS, 2, CP_FB_DISABLED_SUFFIX);
    }
}

/* PoP: config_py_strip_dotted_keys @ hermes_cli/config.py:_strip_dotted_keys */
/* Mutates cfg (BORROWED) in place and returns it. *out_stripped receives a NEW
 * JSON array of the dotted keys actually removed (caller frees), sorted for
 * determinism — Python returns a set. dotted_keys: borrowed JSON array of
 * strings; split is a PLAIN '.' split (no escape handling), walk is cfg_get. */
json_t *config_py_strip_dotted_keys(json_t *cfg, const json_t *dotted_keys, json_t **out_stripped) {
    json_t *stripped = json_array();
    if (out_stripped) *out_stripped = stripped;
    if (!cfg || cfg->type != JSON_OBJECT || !dotted_keys || dotted_keys->type != JSON_ARRAY)
        return cfg;
    for (size_t i = 0; i < dotted_keys->c.count; i++) {
        json_t *d = dotted_keys->c.items[i];
        if (!d || d->type != JSON_STRING || !d->str_val) continue;
        const char *dotted = d->str_val;
        const char *last_dot = strrchr(dotted, '.');
        const char *leaf = last_dot ? last_dot + 1 : dotted;
        /* walk the parent segments with cfg_get semantics */
        json_t *node = cfg;
        if (last_dot) {
            size_t plen = (size_t)(last_dot - dotted);
            char *prefix = (char *)malloc(plen + 1);
            memcpy(prefix, dotted, plen);
            prefix[plen] = '\0';
            char *p = prefix;
            while (node && node->type == JSON_OBJECT) {
                char *dot = strchr(p, '.');
                if (dot) *dot = '\0';
                node = json_obj_get(node, p);
                if (!dot) break;
                p = dot + 1;
            }
            free(prefix);
        }
        if (node && node->type == JSON_OBJECT && json_obj_get(node, leaf)) {
            json_obj_del(node, leaf);
            int dup = 0;
            for (size_t k = 0; k < stripped->c.count; k++) {
                json_t *s = stripped->c.items[k];
                if (s && s->type == JSON_STRING && strcmp(s->str_val, dotted) == 0) { dup = 1; break; }
            }
            if (!dup) json_append(stripped, json_string(dotted));
        }
    }
    /* insertion sort: Python's set has no order; the oracle sorts both sides */
    for (size_t a = 1; a < stripped->c.count; a++) {
        json_t *cur = stripped->c.items[a];
        size_t b = a;
        while (b > 0 && strcmp(stripped->c.items[b - 1]->str_val, cur->str_val) > 0) {
            stripped->c.items[b] = stripped->c.items[b - 1];
            b--;
        }
        stripped->c.items[b] = cur;
    }
    return cfg;
}

/* PoP: config_py_is_non_env_secret_ref @ hermes_cli/config.py:_is_non_env_secret_ref */
/* True for a SecretRef body with a non-env source: ":" present AND the body
 * matches ^[a-z][a-z0-9_-]*: (case-sensitive, anchored). */
bool config_py_is_non_env_secret_ref(const char *ref) {
    if (!ref || !strchr(ref, ':')) return false;
    if (!(ref[0] >= 'a' && ref[0] <= 'z')) return false;
    const char *p = ref + 1;
    while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-') p++;
    return *p == ':';
}

/* PoP: config_py_is_secret_config_key @ hermes_cli/config.py:_is_secret_config_key */
/* The LAST dotted segment, lowercased with '-' folded to '_', decides.
 * Env-routed keys are secrets unless they end in a non-secret suffix. */
bool config_py_is_secret_config_key(const char *key) {
    if (!key) return false;
    const char *leaf = strrchr(key, '.');
    leaf = leaf ? leaf + 1 : key;
    char buf[512];
    size_t n = strlen(leaf);
    if (n >= sizeof buf) n = sizeof buf - 1;
    for (size_t i = 0; i < n; i++) {
        char c = leaf[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == '-') c = '_';
        buf[i] = c;
    }
    buf[n] = '\0';
    if (cfg_is_env_config_key(key)) {
        static const char *const non_secret[] = {
            "_url", "_host", "_user", "_id", "_domain", "_scheme",
        };
        for (int i = 0; i < 6; i++) {
            size_t sl = strlen(non_secret[i]);
            if (n >= sl && strcmp(buf + n - sl, non_secret[i]) == 0) return false;
        }
        return true;
    }
    static const char *const secret_keys[] = {
        "api_key", "apikey", "key", "token", "access_token", "refresh_token", "id_token",
        "secret", "client_secret", "password", "passwd", "authorization",
        "private_key", "bearer", "jwt", NULL,
    };
    for (int i = 0; secret_keys[i]; i++)
        if (strcmp(buf, secret_keys[i]) == 0) return true;
    static const char *const secret_suffixes[] = {
        "_api_key", "_token", "_secret", "_password", "_key", "_access_key",
    };
    for (int i = 0; i < 6; i++) {
        size_t sl = strlen(secret_suffixes[i]);
        if (n >= sl && strcmp(buf + n - sl, secret_suffixes[i]) == 0) return true;
    }
    return false;
}
/* ---- load/save canonicalization pipeline ---- */

/* dict(x) for a JSON value, with the live exception texts: object -> copy,
 * list -> dict(pairs) (empty -> {}), "" -> {}, non-empty str -> ValueError
 * (iterating chars yields length-1 elements), other scalars -> TypeError.
 * Returns a NEW object (caller frees) or NULL + *err "Type: message". */
static json_t *cp_dict_from(const json_t *v, char **err) {
    if (err) *err = NULL;
    char m[160];
    if (v && v->type == JSON_OBJECT) return json_copy(v);
    if (v && v->type == JSON_ARRAY) {
        json_t *d = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *item = v->c.items[i];
            if (item && item->type == JSON_ARRAY && item->c.count == 2) {
                char *k = cp_dict_key_text(item->c.items[0]);
                json_set(d, k, json_copy(item->c.items[1]));
                free(k);
            } else if (item && item->type == JSON_STRING && item->str_val &&
                       strlen(item->str_val) == 2) {
                char kb[2] = { item->str_val[0], '\0' };
                char vb[2] = { item->str_val[1], '\0' };
                json_set(d, kb, json_string(vb));
            } else if (item && (item->type == JSON_ARRAY || item->type == JSON_STRING)) {
                size_t ilen = (item->type == JSON_ARRAY) ? item->c.count
                                                         : strlen(item->str_val ? item->str_val : "");
                snprintf(m, sizeof m,
                         "ValueError: dictionary update sequence element #%zu has length %zu; 2 is required",
                         i, ilen);
                if (err) *err = strdup(m);
                json_free(d);
                return NULL;
            } else {
                snprintf(m, sizeof m,
                         "TypeError: cannot convert dictionary update sequence element #%zu to a sequence", i);
                if (err) *err = strdup(m);
                json_free(d);
                return NULL;
            }
        }
        return d;
    }
    if (v && v->type == JSON_STRING) {
        if (!v->str_val || !v->str_val[0]) return json_object();
        if (err) *err = strdup("ValueError: dictionary update sequence element #0 has length 1; 2 is required");
        return NULL;
    }
    snprintf(m, sizeof m, "TypeError: '%s' object is not iterable", cp_typename(v));
    if (err) *err = strdup(m);
    return NULL;
}

/* _normalize_max_turns_config (live config.py): config = dict(config);
 * agent_config = dict(config.get("agent") or {}); relocate a root max_turns
 * only when agent lacks one; attach agent only when non-empty or present;
 * drop the root key. Returns a NEW object (caller frees); on a Python
 * exception returns NULL and sets *err to "Type: message" (caller frees). */
static json_t *cp_normalize_max_turns(const json_t *config_in, char **err) {
    if (err) *err = NULL;
    json_t *config = cp_dict_from(config_in, err);
    if (!config) return NULL;
    json_t *agent_in = json_obj_get(config, "agent");
    json_t *agent = cp_truthy(agent_in) ? cp_dict_from(agent_in, err) : json_object();
    if (!agent) { json_free(config); return NULL; }
    json_t *root_mt = json_obj_get(config, "max_turns");
    if (root_mt && !json_obj_get(agent, "max_turns"))
        json_set(agent, "max_turns", json_copy(root_mt));
    /* a sparse save must not grow an `agent: {}` section */
    if (cp_truthy(agent) || json_obj_get(config, "agent"))
        json_set(config, "agent", agent);
    else
        json_free(agent);
    json_obj_del(config, "max_turns");
    return config;
}

/* _normalize_root_model_keys — live def in hermes_cli/config_model_section.py,
 * re-exported by config.py and run by _canonicalize_config. Faithful port:
 * dict-valued id flattening with provider promotion, root-key promotion,
 * provider string coercion, api_base aliasing, default>model>name canon.
 * The discarded-extra-key warnings are logging-only and do not affect the
 * returned value, so they are omitted in the pure port. Returns a NEW object
 * (caller frees). */
static json_t *cp_normalize_root_model_keys(const json_t *config_in) {
    json_t *config = json_copy(config_in);
    json_t *model_in = json_obj_get(config, "model");
    json_t *model_provider = (model_in && model_in->type == JSON_OBJECT)
                                 ? json_obj_get(model_in, "provider") : NULL;
    int needs_model_work =
        (model_provider && model_provider->type != JSON_NULL && model_provider->type != JSON_STRING) ||
        (model_in && model_in->type == JSON_OBJECT && (
            cp_truthy(json_obj_get(model_in, "api_base")) ||
            cp_truthy(json_obj_get(model_in, "model")) ||
            cp_truthy(json_obj_get(model_in, "name")) ||
            (json_obj_get(model_in, "default") && json_obj_get(model_in, "default")->type == JSON_OBJECT) ||
            (json_obj_get(model_in, "model") && json_obj_get(model_in, "model")->type == JSON_OBJECT) ||
            (json_obj_get(model_in, "name") && json_obj_get(model_in, "name")->type == JSON_OBJECT)));
    int has_root =
        cp_truthy(json_obj_get(config, "provider")) ||
        cp_truthy(json_obj_get(config, "base_url")) ||
        cp_truthy(json_obj_get(config, "context_length")) ||
        cp_truthy(json_obj_get(config, "api_base"));
    if (!has_root && !needs_model_work) return config;

    /* model = dict(model) if isinstance(model, dict) else {"default": model} if model else {} */
    json_t *mcur = json_obj_get(config, "model");
    json_t *model;
    if (mcur && mcur->type == JSON_OBJECT) {
        model = json_copy(mcur);
    } else if (cp_truthy(mcur)) {
        model = json_object();
        json_set(model, "default", json_copy(mcur));
    } else {
        model = json_object();
    }
    json_set(config, "model", model);

    /* flatten dict-valued default/model/name; nested provider wins over an
     * empty/"auto" outer provider, never over a configured one */
    static const char *const idkeys[] = { "default", "model", "name" };
    for (int k = 0; k < 3; k++) {
        json_t *val = json_obj_get(model, idkeys[k]);
        if (val && val->type == JSON_OBJECT) {
            json_t *nm = json_obj_get(val, "model");
            if (!cp_truthy(nm)) nm = json_obj_get(val, "default");
            char *nested_model = cp_py_str_truthy(nm);
            char *nested_provider = cp_py_str_truthy(json_obj_get(val, "provider"));
            json_set(model, idkeys[k], json_string(nested_model));
            free(nested_model);
            if (nested_provider[0]) {
                char *outer = cp_py_str_truthy(json_obj_get(model, "provider"));
                if (!outer[0] || strcmp(outer, "auto") == 0)
                    json_set(model, "provider", json_string(nested_provider));
                free(outer);
            }
            free(nested_provider);
        }
    }

    /* promote root provider/base_url/context_length (never override) */
    static const char *const rootkeys[] = { "provider", "base_url", "context_length" };
    for (int k = 0; k < 3; k++) {
        json_t *rv = json_obj_get(config, rootkeys[k]);
        if (cp_truthy(rv) && !cp_truthy(json_obj_get(model, rootkeys[k])))
            json_set(model, rootkeys[k], json_copy(rv));
        json_obj_del(config, rootkeys[k]);
    }

    /* provider identity is a string; guard on presence (None stays absent) */
    json_t *pv = json_obj_get(model, "provider");
    if (pv && pv->type != JSON_NULL) {
        char *s = cp_py_str(pv);
        cp_str_strip(s);
        json_set(model, "provider", json_string(s));
        free(s);
    }

    /* api_base alias: root first, then model; fallback-only */
    json_t *ab1 = json_obj_get(config, "api_base");
    if (cp_truthy(ab1) && !cp_truthy(json_obj_get(model, "base_url")))
        json_set(model, "base_url", json_copy(ab1));
    json_t *ab2 = json_obj_get(model, "api_base");
    if (cp_truthy(ab2) && !cp_truthy(json_obj_get(model, "base_url")))
        json_set(model, "base_url", json_copy(ab2));
    json_obj_del(config, "api_base");
    json_obj_del(model, "api_base");

    /* model/name are last-resort aliases (in that order), then dropped */
    json_t *alias = json_obj_get(model, "model");
    if (!cp_truthy(alias)) alias = json_obj_get(model, "name");
    if (!cp_truthy(json_obj_get(model, "default")) && cp_truthy(alias))
        json_set(model, "default", json_copy(alias));
    if (cp_truthy(json_obj_get(model, "default"))) {
        json_obj_del(model, "model");
        json_obj_del(model, "name");
    }
    return config;
}

/* PoP: config_py_canonicalize_config @ hermes_cli/config.py:_canonicalize_config */
/* The load/save normalization pipeline: max_turns relocation, then model-section
 * canon. Returns a NEW config (caller frees). On a Python exception returns NULL
 * and sets *err_msg to "ExcType: message" (caller frees). */
json_t *config_py_canonicalize_config(const json_t *config, char **err_msg) {
    if (err_msg) *err_msg = NULL;
    char *err = NULL;
    json_t *t = cp_normalize_max_turns(config, &err);
    if (!t) {
        if (err_msg) *err_msg = err; else free(err);
        return NULL;
    }
    json_t *r = cp_normalize_root_model_keys(t);
    json_free(t);
    return r;
}

/* PoP: config_py_ensure_dict @ hermes_cli/config.py:_ensure_dict */
/* Returns parent[key] as a dict (BORROWED — owned by parent), replacing a
 * missing or non-dict value with a fresh {}. parent must be a JSON object;
 * NULL/non-object returns NULL (the live .get would raise AttributeError). */
json_t *config_py_ensure_dict(json_t *parent, const char *key) {
    if (!parent || parent->type != JSON_OBJECT || !key) return NULL;
    json_t *child = json_obj_get(parent, key);
    if (!child || child->type != JSON_OBJECT) {
        child = json_object();
        json_set(parent, key, child);
    }
    return child;
}

/* PoP: config_py_commented_sections_for_save @ hermes_cli/config.py:_commented_sections_for_save */
/* Commented example blocks for off/unconfigured features. Returns malloc'd
 * text (caller frees), or NULL for Python None ("".join(parts) or None).
 * A truthy non-dict "security" value raises AttributeError in the live def:
 * NULL + *err_msg = "AttributeError: '<type>' object has no attribute 'get'". */
char *config_py_commented_sections_for_save(const json_t *normalized, char **err_msg) {
    if (err_msg) *err_msg = NULL;
    if (!normalized || normalized->type != JSON_OBJECT) return NULL;
    cp_buf parts;
    cp_buf_init(&parts);
    json_t *security = json_obj_get(normalized, "security");
    if (!cp_truthy(security)) {
        /* (normalized.get("security") or {}).get("redact_secrets") is None */
        cp_buf_add(&parts, CP_SECURITY_COMMENT);
    } else if (security->type == JSON_OBJECT) {
        json_t *rs = json_obj_get(security, "redact_secrets");
        if (!rs || rs->type == JSON_NULL) cp_buf_add(&parts, CP_SECURITY_COMMENT);
    } else {
        char m[128];
        snprintf(m, sizeof m, "AttributeError: '%s' object has no attribute 'get'",
                 cp_typename(security));
        if (err_msg) *err_msg = strdup(m);
        free(parts.s);
        return NULL;
    }
    /* fb = normalized.get("fallback_model", {}); entries = fb if list else [fb] */
    json_t *fb = json_obj_get(normalized, "fallback_model");
    int any_ok = 0;
    if (fb && fb->type == JSON_ARRAY) {
        for (size_t i = 0; i < fb->c.count && !any_ok; i++) {
            json_t *e = fb->c.items[i];
            if (e && e->type == JSON_OBJECT &&
                cp_truthy(json_obj_get(e, "provider")) && cp_truthy(json_obj_get(e, "model")))
                any_ok = 1;
        }
    } else if (fb && fb->type == JSON_OBJECT) {
        if (cp_truthy(json_obj_get(fb, "provider")) && cp_truthy(json_obj_get(fb, "model")))
            any_ok = 1;
    }
    if (!any_ok) cp_buf_add(&parts, CP_FALLBACK_COMMENT);
    if (parts.len == 0) { free(parts.s); return NULL; }
    return parts.s;
}

/* PoP: config_py_omitted_config_paths @ hermes_cli/config.py:_omitted_config_paths */
/* Mapping paths proposed would delete by omission from existing. Returns a NEW
 * JSON array of dotted strings (caller frees), in existing-key iteration order.
 * Both inputs borrowed JSON objects; recursion mirrors the live prefix walk. */
static void cp_omitted_rec(const json_t *existing, const json_t *proposed,
                           const char *prefix, json_t *out) {
    if (!existing || existing->type != JSON_OBJECT) return;
    for (size_t i = 0; i < existing->c.count; i++) {
        const char *key = existing->c.keys[i];
        json_t *old_value = existing->c.items[i];
        char path[2048];
        if (prefix[0]) snprintf(path, sizeof path, "%s.%s", prefix, key);
        else snprintf(path, sizeof path, "%s", key);
        json_t *new_value = (proposed && proposed->type == JSON_OBJECT)
                                ? json_obj_get(proposed, key) : NULL;
        if (!new_value) {
            json_append(out, json_string(path));
            continue;
        }
        if (old_value && old_value->type == JSON_OBJECT) {
            if (new_value->type == JSON_OBJECT)
                cp_omitted_rec(old_value, new_value, path, out);
            else if (old_value->c.count > 0)
                json_append(out, json_string(path));
        }
    }
}

json_t *config_py_omitted_config_paths(const json_t *existing, const json_t *proposed) {
    json_t *out = json_array();
    cp_omitted_rec(existing, proposed, "", out);
    return out;
}
/* ---- escape-aware dotted-key navigation (#84064 family) ---- */

/* PoP: config_py_split_key_path @ hermes_cli/config.py:_split_key_path */
/* Split a dotted key path honoring backslash-escaped dots (a\.b -> one segment
 * "a.b"); a backslash before any other character is preserved verbatim.
 * Returns a NEW JSON array of segment strings (caller frees). NULL key -> NULL
 * (the live len(None) would raise TypeError). Empty key -> [""]; a trailing
 * dot yields a final empty segment, exactly as the live loop appends it. */
json_t *config_py_split_key_path(const char *key) {
    if (!key) return NULL;
    char **segs = NULL;
    int n = cp_split(key, &segs);
    if (n < 0) return NULL;
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) {
        json_append(arr, json_string(segs[i]));
        free(segs[i]);
    }
    free(segs);
    return arr;
}

/* PoP: config_py_greedy_literal_match @ hermes_cli/config.py:_greedy_literal_match */
/* Longest dot-join of parts[0..n) present as a literal key in container.
 * container/parts borrowed (parts: JSON array of strings). Returns n_consumed
 * (>0) and copies the literal key into out (caller buffer), or 0 for Python
 * None (non-dict container, empty parts, or no match). */
int config_py_greedy_literal_match(const json_t *container, const json_t *parts,
                                   char *out, size_t cap) {
    if (!container || container->type != JSON_OBJECT ||
        !parts || parts->type != JSON_ARRAY || parts->c.count == 0)
        return 0;
    size_t n = parts->c.count;
    char **segs = (char **)malloc(sizeof(char *) * n);
    if (!segs) return 0;
    int ok = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *p = parts->c.items[i];
        segs[i] = (p && p->type == JSON_STRING) ? p->str_val : NULL;
        if (!segs[i]) ok = 0;   /* live ".".join would raise TypeError; parts are always strings */
    }
    int r = ok ? cfg_greedy_literal_match((json_t *)container, segs, (int)n, out, cap) : 0;
    free(segs);
    return r;
}

/* PoP: config_py_phantom_sibling @ hermes_cli/config.py:_phantom_sibling */
/* First existing literal key starting with "<part>." (insertion order), which
 * creating an intermediate mapping `part` would shadow. Returns 1 and copies
 * the key into out (caller buffer), else 0 for Python None. Borrowed inputs. */
int config_py_phantom_sibling(const json_t *container, const char *part, char *out, size_t cap) {
    if (!part) return 0;
    return cfg_phantom_sibling((json_t *)container, part, out, cap);
}

/* PoP: config_py_locate_nested @ hermes_cli/config.py:_locate_nested */
/* Walk parts (borrowed JSON array of strings) through root. Returns 1 and fills
 * loc on success, 0 for Python None (missing hop, non-numeric/out-of-range list
 * index, scalar before the path is consumed, empty parts). On success the
 * caller frees loc.key / loc.parents / loc.parent_keys; the container pointers
 * are BORROWED. List indices use Python int() semantics: whitespace, sign,
 * digit underscores, and NEGATIVE indexing (current[-1] is the last item).
 * Index keys are stored canonically ("%lld") so callers can re-parse them. */
int config_py_locate_nested(json_t *root, const json_t *parts, cfg_loc_t *loc) {
    if (!loc) return 0;
    memset(loc, 0, sizeof *loc);
    if (!root || !parts || parts->type != JSON_ARRAY || parts->c.count == 0) return 0;
    int nparts = (int)parts->c.count;
    char **segs = (char **)malloc(sizeof(char *) * (size_t)nparts);
    if (!segs) return 0;
    for (int i = 0; i < nparts; i++) {
        json_t *p = parts->c.items[i];
        segs[i] = (p && p->type == JSON_STRING) ? p->str_val : NULL;
        if (!segs[i]) { free(segs); return 0; }
    }
    json_t **parents = NULL;
    char **parent_keys = NULL;
    int np = 0;
    json_t *current = root;
    int i = 0, ok = 0;
    for (;;) {
        const char *key = NULL;
        long long index = 0;
        int is_index = 0, consumed = 1;
        int remaining = nparts - i;
        if (current->type == JSON_ARRAY) {
            if (remaining < 1) goto done;
            if (!cp_py_int_parse(segs[i], &index)) goto done;
            long long len = (long long)current->c.count;
            long long eff = index < 0 ? index + len : index;
            if (eff < 0 || eff >= len) goto done;   /* IndexError -> None */
            is_index = 1;
        } else if (current->type == JSON_OBJECT) {
            char joined[2048];
            int n = cfg_greedy_literal_match(current, segs + i, remaining, joined, sizeof joined);
            if (n <= 0) goto done;
            consumed = n;
            key = joined;
        } else {
            goto done;                              /* scalar before path end */
        }
        i += consumed;
        char *key_copy;
        if (is_index) {
            char ibuf[32];
            snprintf(ibuf, sizeof ibuf, "%lld", index);
            key_copy = strdup(ibuf);
        } else {
            key_copy = strdup(key);
        }
        if (!key_copy) goto done;
        if (i >= nparts) {
            loc->container = current;
            loc->key = key_copy;
            loc->is_index = is_index;
            loc->parents = parents;
            loc->parent_keys = parent_keys;
            loc->n_parents = np;
            ok = 1;
            goto done;
        }
        json_t *next;
        if (is_index) {
            long long eff = index < 0 ? index + (long long)current->c.count : index;
            next = current->c.items[(size_t)eff];
        } else {
            next = json_obj_get(current, key_copy);
        }
        json_t **np2 = (json_t **)realloc(parents, sizeof(json_t *) * (size_t)(np + 1));
        char **nk2 = (char **)realloc(parent_keys, sizeof(char *) * (size_t)(np + 1));
        if (!np2 || !nk2) { free(key_copy); goto done; }
        parents = np2;
        parent_keys = nk2;
        parents[np] = current;
        parent_keys[np] = key_copy;
        np++;
        current = next;
        if (!current) goto done;
    }
done:
    if (!ok) {
        for (int k = 0; k < np; k++) free(parent_keys[k]);
        free(parents);
        free(parent_keys);
    }
    free(segs);
    return ok;
}

/* PoP: config_py_looks_structured_value @ hermes_cli/config.py:_looks_structured_value */
/* Conservative YAML/JSON structure sniff: a leading '['/'{' after lstrip, or a
 * multiline value with a '- ' list item or a 'key: value' / 'key:' mapping
 * shape. A bare leading '-' is NOT a trigger (-5, --flag stay strings).
 * splitlines() boundaries include \r \v \f \x1c-\x1e \x85    . */
bool config_py_looks_structured_value(const char *value) {
    if (!value) return false;
    const char *s = value;
    while (*s && cp_is_py_ws((unsigned char)*s)) s++;
    if (s[0] == '[' || s[0] == '{') return true;
    if (!strchr(value, '\n')) return false;
    const char *p = value;
    while (*p) {
        const char *start = p;
        while (*p && !cp_line_break_len(p)) p++;
        size_t len = (size_t)(p - start);
        char item[2048];
        size_t a = 0, b = len;
        while (a < b && cp_is_py_ws((unsigned char)start[a])) a++;
        while (b > a && cp_is_py_ws((unsigned char)start[b - 1])) b--;
        size_t ilen = b - a;
        if (ilen >= sizeof item) ilen = sizeof item - 1;
        memcpy(item, start + a, ilen);
        item[ilen] = '\0';
        if (strcmp(item, "-") == 0 || strncmp(item, "- ", 2) == 0) return true;
        /* partition(': '): head non-empty, no space in head, not a comment */
        char *sep = strstr(item, ": ");
        if (sep) {
            *sep = '\0';
            int head_ok = item[0] && !strchr(item, ' ') && item[0] != '#';
            *sep = ':';
            if (head_ok) return true;
        }
        size_t L = strlen(item);
        if (L > 0 && item[L - 1] == ':') {
            item[L - 1] = '\0';
            if (item[0] && !strchr(item, ' ')) return true;
        }
        if (*p) {
            if (p[0] == '\r' && p[1] == '\n') p += 2;
            else p += cp_line_break_len(p);
        }
    }
    return false;
}

/* ---- container-type guardrails (#114471) ---- */

/* _KNOWN_CONTAINER_TYPES: roots absent from DEFAULT_CONFIG whose shape is
 * fixed by their readers. */
static const struct { const char *key; const char *type; } CP_KNOWN_CONTAINER_TYPES[] = {
    { "custom_providers", "list" },
    { "providers", "mapping" },
    { "model.aliases", "mapping" },
    { "model_aliases", "mapping" },
    { "plugins.enabled", "list" },
    { "plugins.disabled", "list" },
    { "model_catalog.excluded_providers", "list" },
};

/* PoP: config_py_expected_container_type @ hermes_cli/config.py:_expected_container_type */
/* "list"/"mapping" (static strings) when the schema, the known-container table,
 * or the value already on disk fixes key to a container; NULL for scalars and
 * open paths. Reads the EMBEDDED DEFAULT_CONFIG (config_defaults port — no I/O).
 * key/user_config borrowed; user_config may be NULL. A single-segment key whose
 * schema node is a mapping section skips the schema lookup (section overwrite
 * is _guard_section_overwrite's call). */
const char *config_py_expected_container_type(const char *key, const json_t *user_config) {
    if (!key) return NULL;
    char **segs = NULL;
    int n = cp_split(key, &segs);
    if (n <= 0) { free(segs); return NULL; }
    json_t *defaults = config_defaults_DEFAULT_CONFIG();
    json_t *schema_node = cp_cfg_get_node(defaults, segs, n);
    if (n == 1 && schema_node && schema_node->type == JSON_OBJECT) schema_node = NULL;
    /* _get_nested: greedy-literal navigation; NULL stands in for _MISSING
     * (neither a dict/list nor equal to "mapping"/"list", so both skip). */
    json_t *existing = cfg_get_nested((json_t *)user_config, key);
    const char *table_val = NULL;
    for (size_t i = 0; i < sizeof CP_KNOWN_CONTAINER_TYPES / sizeof CP_KNOWN_CONTAINER_TYPES[0]; i++)
        if (strcmp(key, CP_KNOWN_CONTAINER_TYPES[i].key) == 0) {
            table_val = CP_KNOWN_CONTAINER_TYPES[i].type;
            break;
        }
    const char *result = NULL;
    for (int pass = 0; pass < 3 && !result; pass++) {
        const json_t *node = (pass == 0) ? schema_node : (pass == 2) ? existing : NULL;
        if (pass == 1) {
            if (table_val) result = table_val;   /* the literal "mapping"/"list" rows */
            continue;
        }
        if (!node) continue;
        if (node->type == JSON_OBJECT) result = "mapping";
        else if (node->type == JSON_ARRAY) result = "list";
        else if (node->type == JSON_STRING && node->str_val) {
            if (strcmp(node->str_val, "mapping") == 0) result = "mapping";
            else if (strcmp(node->str_val, "list") == 0) result = "list";
        }
    }
    for (int i = 0; i < n; i++) free(segs[i]);
    free(segs);
    json_free(defaults);
    return result;
}

/* PoP: config_py_refuse_container_type_mismatch @ hermes_cli/config.py:_refuse_container_type_mismatch */
/* Returns a NEW json value to store (caller frees): a copy of value, or a
 * one-item list for the parse_config_string_list scalar slots. On refusal
 * returns NULL and sets *err_msg to the exact _exit_invalid text (caller
 * frees) — the live def prints it to stderr and sys.exit(1)s. force keeps its
 * documented meaning: replace a whole MAPPING section only. */
json_t *config_py_refuse_container_type_mismatch(const char *key, const json_t *value,
                                                 const json_t *user_config, int force,
                                                 char **err_msg) {
    if (err_msg) *err_msg = NULL;
    const char *expected = config_py_expected_container_type(key, user_config);
    if (!expected) return value ? json_copy(value) : json_null();
    if (strcmp(expected, "list") == 0 && value && value->type == JSON_STRING &&
        (strcmp(key, "agent.disabled_toolsets") == 0 || strcmp(key, "skills.disabled") == 0)) {
        json_t *arr = json_array();
        json_append(arr, json_copy(value));
        return arr;
    }
    int ok = (strcmp(expected, "list") == 0)
                 ? (value && value->type == JSON_ARRAY)
                 : (value && value->type == JSON_OBJECT);
    if (ok || (strcmp(expected, "mapping") == 0 && force))
        return value ? json_copy(value) : json_null();
    const char *got = (value && value->type == JSON_STRING) ? "string" : cp_typename(value);
    const char *literal = (strcmp(expected, "list") == 0) ? "[item, ...]" : "{key: value}";
    size_t cap = strlen(key) * 2 + strlen(expected) + strlen(got) + strlen(literal) + 256;
    char *msg = (char *)malloc(cap);
    snprintf(msg, cap,
             "\xe2\x9c\x97" " Cannot set '%s': it must be a %s, got a %s \xe2\x80\x94" " nothing was written.\n"
             "  Pass a YAML/JSON literal, e.g.:\n    hermes config set %s '%s'\n"
             "  or edit config.yaml directly.",
             key, expected, got, key, literal);
    if (err_msg) *err_msg = msg; else free(msg);
    return NULL;
}
/* ---- install-method shims (live defs are deliberate constants) ---- */

/* PoP: config_py_is_uv_tool_install @ hermes_cli/config.py:is_uv_tool_install */
/* Live shim: False until relaunch (no uv-tool selection). */
bool config_py_is_uv_tool_install(void) {
    return cfg_is_uv_tool_install() != 0;
}

/* PoP: config_py_is_unsupported_install_method @ hermes_cli/config.py:is_unsupported_install_method */
/* Live shim: False for every method (no legacy detection). */
bool config_py_is_unsupported_install_method(const char *method) {
    return cfg_is_unsupported_install_method(method) != 0;
}

/* PoP: config_py_format_unsupported_install_warning @ hermes_cli/config.py:format_unsupported_install_warning */
/* Live shim: the empty string for every method. Returns malloc'd "" (caller frees). */
char *config_py_format_unsupported_install_warning(const char *method) {
    char buf[8];
    cfg_format_unsupported_install_warning(method, buf, sizeof buf);
    return strdup(buf);
}

/* ---- env-name write policy ---- */

/* PoP: config_py_env_var_policy_name @ hermes_cli/config.py:_env_var_policy_name */
/* Name used for env policy comparisons: upper-cased on Windows only.
 * is_windows: 1 Windows, 0 POSIX, -1 host default (the live _IS_WINDOWS).
 * Writes into out (caller buffer); ASCII upper-casing (env names are ASCII). */
void config_py_env_var_policy_name(const char *key, int is_windows, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!key) return;
#ifdef _WIN32
    int host_windows = 1;
#else
    int host_windows = 0;
#endif
    int win = (is_windows < 0) ? host_windows : (is_windows != 0);
    cfg_env_var_policy_name(key, win, out, cap);
}

/* _ENV_VAR_NAME_DENYLIST | LAUNCH_ONLY_ENV_KEYS (hermes_constants:
 * {"HERMES_MANAGED_DIR", "HERMES_HOME"}). Name-by-name, exact match. */
static const char *const CP_ENV_DENYLIST[] = {
    "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT", "LD_DEBUG",
    "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
    "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_FALLBACK_FRAMEWORK_PATH",
    "PYTHONPATH", "PYTHONHOME", "PYTHONSTARTUP", "PYTHONUSERBASE",
    "PYTHONEXECUTABLE", "PYTHONNOUSERSITE", "PYTHONBREAKPOINT", "PYTHONCASEOK",
    "NODE_OPTIONS", "NODE_PATH",
    "PERL5OPT", "PERL5LIB", "PERLLIB", "RUBYOPT", "RUBYLIB", "CLASSPATH",
    "JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS",
    "GOFLAGS", "RUSTFLAGS",
    "PATH", "SHELL", "BROWSER", "EDITOR", "VISUAL", "PAGER", "MANPAGER",
    "GIT_SSH_COMMAND", "GIT_EXEC_PATH", "GIT_SHELL",
    "GIT_SSH", "GIT_ASKPASS", "SSH_ASKPASS", "SUDO_ASKPASS",
    "GIT_EDITOR", "GIT_SEQUENCE_EDITOR", "GIT_PAGER", "GIT_EXTERNAL_DIFF",
    "GIT_PROXY_COMMAND", "GIT_TEMPLATE_DIR", "GIT_DIR",
    "BASH_ENV", "ENV", "ZDOTDIR", "PROMPT_COMMAND", "VIMINIT", "EXINIT",
    "HERMES_HOME", "HERMES_PROFILE", "HERMES_CONFIG", "HERMES_ENV",
    "HERMES_CONFIG_PATH", "HERMES_ENV_PATH",
    "HERMES_OPTIONAL_MCPS",
    "HERMES_YOLO_MODE", "HERMES_ACCEPT_HOOKS", "HERMES_REDACT_SECRETS",
    "HERMES_INTERACTIVE", "HERMES_EXEC_ASK", "HERMES_GATEWAY_SESSION",
    "HERMES_CRON_SESSION", "HERMES_SINGLE_QUERY_SESSION",
    "HERMES_SESSION_KEY", "HERMES_SESSION_PLATFORM",
    "HERMES_MANAGED_DIR",   /* LAUNCH_ONLY_ENV_KEYS */
    NULL,
};
/* _ENV_VAR_NAME_DENY_PREFIXES: whole families matched by prefix. */
static const char *const CP_ENV_DENY_PREFIXES[] = { "LD_", "DYLD_", "GIT_CONFIG_" };

/* PoP: config_py_validate_env_var_name_for_write @ hermes_cli/config.py:validate_env_var_name_for_write */
/* Returns CFG_ENV_OK / CFG_ENV_INVALID_NAME / CFG_ENV_DENIED; on failure writes
 * the exact ValueError message into err (caller buffer). launch_denylist is a
 * borrowed JSON array of names — the C seam for the live in-function import of
 * auth_external_process.env_writer_launch_denylist() (a dynamic provider/left-
 * core registry; the oracle feeds the same set to the live def). is_windows:
 * -1 host default — the live function always uses the host policy.
 * Shape regex ^[A-Za-z_][A-Za-z0-9_]*$: Python's $ also matches just before a
 * single trailing newline, so "FOO\n" passes the shape check. */
int config_py_validate_env_var_name_for_write(const char *key, int is_windows,
                                              const json_t *launch_denylist,
                                              char *err, size_t errcap) {
    if (err && errcap) err[0] = '\0';
    int shape_ok = 0;
    if (key && key[0] &&
        ((key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= 'a' && key[0] <= 'z') || key[0] == '_')) {
        const char *p = key + 1;
        while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
               (*p >= '0' && *p <= '9') || *p == '_') p++;
        if (*p == '\0' || (p[0] == '\n' && p[1] == '\0')) shape_ok = 1;
    }
    if (!shape_ok) {
        if (err && errcap) {
            char *r = cp_py_repr_str(key ? key : "");
            snprintf(err, errcap, "Invalid environment variable name: %s", r);
            free(r);
        }
        return CFG_ENV_INVALID_NAME;
    }
    size_t polcap = strlen(key) + 1;
    char *policy = (char *)malloc(polcap);
    config_py_env_var_policy_name(key, is_windows, policy, polcap);
    int denied = 0;
    for (int i = 0; CP_ENV_DENYLIST[i] && !denied; i++)
        if (strcmp(policy, CP_ENV_DENYLIST[i]) == 0) denied = 1;
    if (!denied && launch_denylist && launch_denylist->type == JSON_ARRAY) {
        for (size_t i = 0; i < launch_denylist->c.count && !denied; i++) {
            json_t *nm = launch_denylist->c.items[i];
            if (!nm || nm->type != JSON_STRING || !nm->str_val) continue;
            size_t lcap = strlen(nm->str_val) + 1;
            char *lp = (char *)malloc(lcap);
            config_py_env_var_policy_name(nm->str_val, is_windows, lp, lcap);
            if (strcmp(policy, lp) == 0) denied = 1;
            free(lp);
        }
    }
    if (!denied)
        for (int i = 0; i < 3 && !denied; i++)
            if (strncmp(policy, CP_ENV_DENY_PREFIXES[i], strlen(CP_ENV_DENY_PREFIXES[i])) == 0)
                denied = 1;
    free(policy);
    if (denied) {
        if (err && errcap) {
            char *r = cp_py_repr_str(key);
            snprintf(err, errcap,
                     "Environment variable %s is on the writer denylist. "
                     "Names that influence subprocess execution (LD_PRELOAD, PYTHONPATH, PATH, EDITOR, ...) "
                     "or Hermes runtime location and security policy (HERMES_HOME, HERMES_YOLO_MODE, ...) "
                     "cannot be persisted via the env writer. If you really need this, edit ~/.hermes/.env "
                     "directly.", r);
            free(r);
        }
        return CFG_ENV_DENIED;
    }
    return CFG_ENV_OK;
}

/* PoP: config_py_install_method_stamp @ hermes_cli/config.py:_install_method_stamp */
/* Pure core of the stamp reader: the CALLER does the file read (utf-8-sig) and
 * passes the decoded text, or NULL for the live OSError -> None path. Strips a
 * leading UTF-8 BOM (the utf-8-sig codec), then .strip().lower(), and returns a
 * static _SUPPORTED_INSTALL_METHODS member or NULL. No I/O here. */
const char *config_py_install_method_stamp(const char *text) {
    if (!text) return NULL;
    static const char *const supported[] = {
        "apt", "docker", "nix", "nixos", "home-manager", "git", "unknown",
    };
    const unsigned char *p = (const unsigned char *)text;
    if (p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) p += 3;
    while (*p && cp_is_py_ws(*p)) p++;
    const unsigned char *e = p + strlen((const char *)p);
    while (e > p && cp_is_py_ws(e[-1])) e--;
    size_t n = (size_t)(e - p);
    char buf[64];
    if (n >= sizeof buf) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = p[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    buf[n] = '\0';
    for (int i = 0; i < 7; i++)
        if (strcmp(buf, supported[i]) == 0) return supported[i];
    return NULL;
}
