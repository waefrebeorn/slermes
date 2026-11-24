/*
 * config_worker_save.c — faithful C11 port of hermes_cli/config_worker_save.py
 * (gate w17_configwork_3400f3).
 *
 * PoP: w17_configwork_3400f3_schema_default_branch @ hermes_cli/config_worker_save.py:_schema_default_branch
 * PoP: w17_configwork_3400f3_three_way @ hermes_cli/config_worker_save.py:_three_way
 *
 * Both helpers are pure tree transformations: mappings recurse, list edits add
 * and remove items by value, and a caller-dropped key is left alone. The
 * _SAME_AS_BASE frozen sentinel is represented by a NULL frozen argument.
 */
#include "noc_w17_configwork_3400f3.h"

#include <stdlib.h>
#include <string.h>

static int is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }

static json_t *dup_node(const json_t *v) { return v ? json_copy(v) : json_null(); }

static int obj_has(const json_t *obj, const char *key) {
    return is_dict(obj) && json_obj_get(obj, key) != NULL;
}

/* Python == on arbitrary JSON values (recursive; True == 1, False == 0). */
static int jequal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) {
        if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
            return (a->bool_val ? 1.0 : 0.0) == b->num_val;
        if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
            return a->num_val == (b->bool_val ? 1.0 : 0.0);
        return 0;
    }
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY:
        case JSON_OBJECT:
            n = a->c.count;
            if (n != b->c.count) return 0;
            if (a->type == JSON_ARRAY) {
                for (i = 0; i < n; ++i) if (!jequal(a->c.items[i], b->c.items[i])) return 0;
                return 1;
            }
            for (i = 0; i < n; ++i) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !jequal(a->c.items[i], bv)) return 0;
            }
            return 1;
    }
    return 0;
}

static int in_array(const json_t *item, const json_t *arr) {
    size_t i, n = is_list(arr) ? arr->c.count : 0;
    for (i = 0; i < n; ++i) if (jequal(item, arr->c.items[i])) return 1;
    return 0;
}

json_t *w17_configwork_3400f3_schema_default_branch(const json_t *snapshot, const json_t *defaults) {
    json_t *out;
    size_t i, n;
    if (!is_dict(snapshot) || !is_dict(defaults)) return dup_node(snapshot ? snapshot : json_null());
    out = json_object();
    n = snapshot->c.count;
    for (i = 0; i < n; ++i) {
        const char *key = snapshot->c.keys[i];
        const json_t *value = snapshot->c.items[i];
        const json_t *def = json_obj_get(defaults, key);
        if (!def) { json_set(out, key, dup_node(value)); continue; }
        if (jequal(value, def)) continue;
        if (is_dict(value) && is_dict(def)) {
            json_t *nested = w17_configwork_3400f3_schema_default_branch(value, def);
            if (is_dict(nested) && nested->c.count > 0) json_set(out, key, nested);
            else json_free(nested);
        } else {
            json_set(out, key, dup_node(value));
        }
    }
    return out;
}

static json_t *three_way_impl(const json_t *live, const json_t *desired, const json_t *base,
                              const json_t *frozen, int sentinel) {
    const json_t *fmap;
    json_t *merged, *out;
    size_t i, k;

    if (sentinel) frozen = base;
    if (jequal(desired, base)) return dup_node(live ? live : json_null());

    if (is_dict(desired) && is_dict(base) && is_dict(live)) {
        fmap = is_dict(frozen) ? frozen : NULL;
        merged = json_copy(live);
        for (i = 0; i < desired->c.count; ++i) {
            const char *key = desired->c.keys[i];
            const json_t *value = desired->c.items[i];
            const json_t *bval = json_obj_get(base, key);
            json_t *res;
            if (!bval) { json_set(merged, key, dup_node(value)); continue; }
            if (!jequal(value, bval)) {
                int kind = is_dict(bval) ? 1 : is_list(bval) ? 2 : 0;
                const json_t *current = json_obj_get(live, key);
                if (kind && !(kind == 1 ? is_dict(current) : is_list(current))) {
                    if (obj_has(live, key) || (fmap && obj_has(fmap, key))) continue;
                    current = bval;             /* default-only branch: copy base[key] */
                }
                {
                    const json_t *fv = fmap ? json_obj_get(fmap, key) : NULL;
                    res = three_way_impl(current, value, bval, fv, 0);
                    json_set(merged, key, res);
                }
            }
        }
        return merged;
    }

    if (is_list(desired) && is_list(base) && is_list(live)) {
        out = json_array();
        for (i = 0; i < live->c.count; ++i) {
            const json_t *item = live->c.items[i];
            int removed = 0;
            for (k = 0; k < base->c.count; ++k) {
                if (!in_array(base->c.items[k], desired) && jequal(item, base->c.items[k])) {
                    removed = 1;
                    break;
                }
            }
            if (!removed) json_append(out, json_copy(item));
        }
        for (i = 0; i < desired->c.count; ++i) {
            const json_t *item = desired->c.items[i];
            if (!in_array(item, base) && !in_array(item, out)) json_append(out, json_copy(item));
        }
        return out;
    }

    return dup_node(desired ? desired : json_null());
}

json_t *w17_configwork_3400f3_three_way(const json_t *live, const json_t *desired,
                                        const json_t *base, const json_t *frozen) {
    return three_way_impl(live, desired, base, frozen, frozen == NULL);
}
