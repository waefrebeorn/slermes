/*
 * plugins_state.c — faithful C11 port of the pure helpers in upstream Hermes
 * pm/plugins_state.py (gate w6_pluginsstat_d01aee).
 *
 * PoP: w6_pluginsstat_enabled_from_config    @ pm/plugins_state.py:_enabled_from_config
 * PoP: w6_pluginsstat_plugin_spellings_sorted @ pm/plugins_state.py:plugin_spellings
 *
 * The filesystem/backend-touching discovery functions are effectful; these two
 * helpers carry the deterministic selection logic.  The manifest read inside
 * plugin_spellings is injected as an explicit `manifest_name` seam.
 */
#include "noc_w6_pluginsstat_d01aee.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *member(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* Python `set(disabled)` membership for string names. */
static int disabled_has(const json_t *disabled, const char *name) {
    if (!disabled || disabled->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < disabled->c.count; i++) {
        json_t *d = disabled->c.items[i];
        if (d && d->type == JSON_STRING && d->str_val && !strcmp(d->str_val, name))
            return 1;
    }
    return 0;
}

static int array_has_str(const json_t *arr, const char *s) {
    if (!arr) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *d = arr->c.items[i];
        if (d && d->type == JSON_STRING && d->str_val && !strcmp(d->str_val, s))
            return 1;
    }
    return 0;
}

json_t *w6_pluginsstat_enabled_from_config(const json_t *config) {
    json_t *out = json_array();
    json_t *pc = member(config, "plugins");
    if (!pc || pc->type != JSON_OBJECT) return out;
    json_t *enabled = member(pc, "enabled");
    if (!enabled || enabled->type != JSON_ARRAY) return out;
    json_t *disabled = member(pc, "disabled");

    for (size_t i = 0; i < enabled->c.count; i++) {
        json_t *name = enabled->c.items[i];
        if (!name || name->type != JSON_STRING || !name->str_val || !name->str_val[0])
            continue; /* isinstance(name, str) and name */
        const char *s = name->str_val;
        if (array_has_str(out, s)) continue;             /* name not in out */
        if (disabled_has(disabled, s)) continue;         /* name not in disabled */
        const char *slash = strrchr(s, '/');
        const char *leaf = slash ? slash + 1 : s;
        if (disabled_has(disabled, leaf)) continue;      /* bare leaf not in disabled */
        json_append(out, json_copy(name));
    }
    return out;
}

static int cmp_str(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

/* Last path segment, treating both '/' and '\' as separators (WindowsPath.name). */
static const char *path_leaf(const char *p) {
    if (!p) return "";
    const char *best = p;
    for (const char *q = p; *q; q++)
        if (*q == '/' || *q == '\\') best = q + 1;
    return best;
}

json_t *w6_pluginsstat_plugin_spellings_sorted(const char *dir_name,
                                               const char *key,
                                               const char *manifest_name) {
    const char *buf[8];
    size_t n = 0;
    const char *leaf = path_leaf(dir_name);
    if (leaf && *leaf) buf[n++] = leaf;
    if (key && *key) {
        buf[n++] = key;
        const char *slash = strrchr(key, '/');
        buf[n++] = slash ? slash + 1 : key; /* key.rsplit("/", 1)[-1] */
    }
    if (manifest_name && *manifest_name) buf[n++] = manifest_name;

    /* dedupe */
    size_t u = 0;
    for (size_t i = 0; i < n; i++) {
        int seen = 0;
        for (size_t j = 0; j < u; j++) if (!strcmp(buf[i], buf[j])) { seen = 1; break; }
        if (!seen) buf[u++] = buf[i];
    }
    qsort(buf, u, sizeof(buf[0]), cmp_str);

    json_t *out = json_array();
    for (size_t i = 0; i < u; i++) json_append(out, json_string(buf[i]));
    return out;
}
