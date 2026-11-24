/*
 * kanban_watchers_common.c — C home for gateway/kanban_watchers_common.py.
 * Faithful port of the live board-enumeration and config-coercion helpers.
 * Collaborators (kb, load_config, check_paused) are injected as decision inputs.
 * PoP: w18_kanbanwatc_c8c510_list_boards @ gateway/kanban_watchers_common.py:_list_boards
 * PoP: w18_kanbanwatc_c8c510_board_slugs @ gateway/kanban_watchers_common.py:_board_slugs
 * PoP: w18_kanbanwatc_c8c510_positive_int @ gateway/kanban_watchers_common.py:_positive_int_setting
 * PoP: w18_kanbanwatc_c8c510_auto_decompose @ gateway/kanban_watchers_common.py:_resolve_auto_decompose_settings
 * PoP: w18_kanbanwatc_c8c510_gc_retention @ gateway/kanban_watchers_common.py:_gc_retention_days
 * PoP: w18_kanbanwatc_c8c510_dispatch_allowed @ gateway/kanban_watchers_common.py:_kanban_dispatch_allowed
 */
#include "noc_w18_kanbanwatc_c8c510.h"
#include <errno.h>
#include <stdlib.h>

/* Python truthiness of a JSON scalar/container. */
static int jobj_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Mimic Python int(v) for the JSON scalar values these settings accept.
 * Returns 1 and stores the value, or 0 when int(v) would raise. */
static int jobj_pyint(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s) return 0;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        if (!*s) return 0;
        errno = 0;
        char *end = NULL;
        long long x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
        if (*end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

/* try: return kb.list_boards(include_archived=False)
 * except Exception: return [kb.read_board_metadata(kb.DEFAULT_BOARD)] */
json_t *w18_kanbanwatc_c8c510_list_boards(int list_raises, json_t *boards, json_t *metadata) {
    json_t *out = json_array();
    if (!list_raises) {
        if (boards && boards->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(boards); i++) json_append(out, json_copy(json_get(boards, i)));
    } else {
        json_append(out, metadata ? json_copy(metadata) : json_null());
    }
    return out;
}

/* [b.get("slug") or kb.DEFAULT_BOARD for b in _list_boards(kb)] */
json_t *w18_kanbanwatc_c8c510_board_slugs(int list_raises, json_t *boards,
                                          json_t *metadata, const char *default_board) {
    json_t *lst = w18_kanbanwatc_c8c510_list_boards(list_raises, boards, metadata);
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(lst); i++) {
        json_t *b = json_get(lst, i);
        json_t *slug = (b && b->type == JSON_OBJECT) ? json_obj_get(b, "slug") : NULL;
        if (slug && jobj_truthy(slug) && slug->type == JSON_STRING)
            json_append(out, json_string(slug->str_val));
        else
            json_append(out, json_string(default_board ? default_board : ""));
    }
    json_free(lst);
    return out;
}

/* raw = cfg.get(key); None -> None; int(raw) fail / <1 -> None; else value */
json_t *w18_kanbanwatc_c8c510_positive_int(json_t *cfg, const char *key) {
    json_t *raw = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, key) : NULL;
    if (!raw || raw->type == JSON_NULL) return json_null();
    long long value;
    if (!jobj_pyint(raw, &value) || value < 1) return json_null();
    return json_number((double)value);
}

/* fails-safe (False,3); per_tick=int(kcfg.get(...,3) or 3) except ->3; max(per_tick,1) */
json_t *w18_kanbanwatc_c8c510_auto_decompose(int config_error, json_t *cfg) {
    json_t *out = json_object();
    int enabled = 1;
    long long per_tick = 3;
    if (config_error) {
        enabled = 0; per_tick = 3;
    } else {
        json_t *kcfg = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "kanban") : NULL;
        if (!kcfg || kcfg->type != JSON_OBJECT) kcfg = NULL;
        if (kcfg) {
            json_t *raw = json_obj_get(kcfg, "auto_decompose_per_tick");
            if (!raw || !jobj_truthy(raw)) raw = NULL;
            long long parsed;
            if (raw && jobj_pyint(raw, &parsed)) per_tick = parsed;
            json_t *ad = json_obj_get(kcfg, "auto_decompose");
            enabled = ad ? jobj_truthy(ad) : 1;  /* default True */
        } else {
            enabled = 1; per_tick = 3;
        }
    }
    if (per_tick < 1) per_tick = 1;
    json_set(out, "enabled", json_bool(enabled));
    json_set(out, "per_tick", json_number((double)per_tick));
    return out;
}

/* int(((load_config() or {}).get("kanban") or {}).get("done_sub_retention_days",30)) ; fail -> 30 */
json_t *w18_kanbanwatc_c8c510_gc_retention(int config_error, json_t *cfg) {
    long long days = 30;
    if (!config_error) {
        json_t *base = (cfg && jobj_truthy(cfg)) ? cfg : NULL;
        json_t *kanban = base ? json_obj_get(base, "kanban") : NULL;
        if (!kanban || !jobj_truthy(kanban)) kanban = NULL;
        json_t *raw = (kanban && kanban->type == JSON_OBJECT)
                    ? json_obj_get(kanban, "done_sub_retention_days") : NULL;
        long long parsed;
        if (raw && jobj_pyint(raw, &parsed)) days = parsed;
    }
    return json_number((double)days);
}

/* ImportError -> True ; else not check_paused("kanban", logger) */
json_t *w18_kanbanwatc_c8c510_dispatch_allowed(int import_error, int paused) {
    return json_bool(import_error ? 1 : (paused ? 0 : 1));
}
