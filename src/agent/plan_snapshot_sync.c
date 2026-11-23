/*
 * plan_snapshot_sync.c — C home for upstream Hermes agent/plan_snapshot_sync.py.
 *
 * PoP: w10_plansnapsh_242594_snapshot_full_ids @ agent/plan_snapshot_sync.py:_snapshot_full_ids
 * PoP: w10_plansnapsh_242594_plan_snapshot_is_stale @ agent/plan_snapshot_sync.py:plan_snapshot_is_stale
 *
 * Pure comparison of reply-header credits state against the cached plan
 * snapshot. Background refresh / inflight bookkeeping stays in the host.
 * The oracle seeds `_window_seed_fields(window)` as `getattr(window,"seed",None)`,
 * so a window with a truthy "seed" field is a window-plan snapshot.
 */
#include "noc_w10_plansnapsh_242594.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 0;
}

static int full_gauge(const json_t *g, const char **id_out) {
    if (!g || g->type != JSON_OBJECT) return 0;
    const json_t *full = json_obj_get(g, "full");
    if (!truthy(full)) return 0;
    const json_t *id = json_obj_get(g, "id");
    if (!id || id->type != JSON_STRING) return 0;
    *id_out = id->str_val;
    return 1;
}

static void sort_unique(json_t *arr) {
    size_t n = arr->c.count;
    for (size_t i = 1; i < n; i++) {
        json_t *key = arr->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(arr->c.items[j - 1]->str_val, key->str_val) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = key;
    }
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        if (w > 0 && !strcmp(arr->c.items[w - 1]->str_val, arr->c.items[i]->str_val)) {
            json_free(arr->c.items[i]);
        } else {
            arr->c.items[w++] = arr->c.items[i];
        }
    }
    arr->c.count = w;
}

/* NULL result means "no window plan" (Python None). */
static json_t *full_ids(const json_t *snapshot, int *has_window) {
    *has_window = 0;
    const json_t *window = snapshot ? json_obj_get(snapshot, "usage_window") : NULL;
    if (!window || window->type != JSON_OBJECT) return NULL;
    /* The oracle seeds _window_seed_fields(window) = getattr(window,"seed",None):
     * only a missing/null seed means "no window plan"; false/0 still count. */
    const json_t *seed = json_obj_get(window, "seed");
    if (!seed || seed->type == JSON_NULL) return NULL;
    *has_window = 1;
    json_t *ids = json_array();
    const json_t *gauges = json_obj_get(window, "gauges");
    if (gauges && gauges->type == JSON_ARRAY) {
        for (size_t i = 0; i < gauges->c.count; i++) {
            const char *id = NULL;
            if (full_gauge(gauges->c.items[i], &id)) json_append(ids, json_string(id));
        }
    }
    sort_unique(ids);
    return ids;
}

json_t *w10_plansnapsh_242594_snapshot_full_ids(const json_t *snapshot) {
    int has_window = 0;
    return full_ids(snapshot, &has_window);
}

static int arrays_equal(const json_t *a, const json_t *b) {
    if (!a || !b) return 0;
    if (a->c.count != b->c.count) return 0;
    for (size_t i = 0; i < a->c.count; i++)
        if (strcmp(a->c.items[i]->str_val, b->c.items[i]->str_val)) return 0;
    return 1;
}

int w10_plansnapsh_242594_plan_snapshot_is_stale(const json_t *state, const json_t *snapshot) {
    if (!state || state->type != JSON_OBJECT || !snapshot || snapshot->type != JSON_OBJECT) return 0;
    const json_t *src = json_obj_get(snapshot, "source");
    if (src && src->type == JSON_STRING && !strcmp(src->str_val, "error")) return 0;
    int on_window = truthy(json_obj_get(state, "is_usage_window"));
    int has_window = 0;
    json_t *full = full_ids(snapshot, &has_window);
    int result;
    if (on_window != has_window) {
        result = 1;
    } else if (!on_window) {
        result = 0;
    } else {
        json_t *sf = json_array();
        const json_t *gauges = json_obj_get(state, "gauges");
        if (gauges && gauges->type == JSON_ARRAY) {
            for (size_t i = 0; i < gauges->c.count; i++) {
                const char *id = NULL;
                if (full_gauge(gauges->c.items[i], &id)) json_append(sf, json_string(id));
            }
        }
        sort_unique(sf);
        result = arrays_equal(sf, full) ? 0 : 1;
        json_free(sf);
    }
    json_free(full);
    return result;
}

