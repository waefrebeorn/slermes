/*
 * auth_model_picker.c — C port of upstream Hermes hermes_cli/auth_model_picker.py.
 *
 * PoP: w16_authmodel_e58b26_picker_order @ hermes_cli/auth_model_picker.py:_picker_order
 * PoP: w16_authmodel_e58b26_constants @ hermes_cli/auth_model_picker.py:_CUSTOM_LABEL/_SKIP_LABEL/_CURRENT_SUFFIX/_COST_COL/_GAUGE_GAP/_LEVEL_STYLE
 *
 * Faithful non-window (window_usage=None) ordering: current model first, then
 * the rest, deduplicated with Python dict.fromkeys ordering; cursor on current.
 */
#include "noc_w16_authmodel_e58b26.h"
#include <string.h>

static bool list_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *n = json_get(arr, i);
        if (n && n->type == JSON_STRING && !strcmp(n->str_val, s)) return true;
    }
    return false;
}

static void append_unique(json_t *out, const char *s) {
    if (!s) return;
    for (size_t i = 0; i < json_len(out); i++) {
        json_t *n = json_get(out, i);
        if (n && n->type == JSON_STRING && !strcmp(n->str_val, s)) return;
    }
    json_append(out, json_string(s));
}

json_t *w16_authmodel_e58b26_picker_order(const json_t *model_ids, const char *current_model) {
    json_t *ordered = json_array();
    const char *cur = current_model ? current_model : "";
    if (*cur && list_has(model_ids, cur)) append_unique(ordered, cur);
    if (model_ids && model_ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(model_ids); i++) {
            json_t *n = json_get(model_ids, i);
            append_unique(ordered, (n && n->type == JSON_STRING) ? n->str_val : NULL);
        }
    }
    /* sections is always empty when window_usage is None. */
    json_t *sections = json_object();
    int start = 0;
    if (*cur) {
        for (size_t i = 0; i < json_len(ordered); i++) {
            json_t *n = json_get(ordered, i);
            if (n && n->type == JSON_STRING && !strcmp(n->str_val, cur)) { start = (int)i; break; }
        }
    }
    json_t *out = json_array();
    json_append(out, ordered);
    json_append(out, sections);
    json_append(out, json_number((double)start));
    return out;
}

json_t *w16_authmodel_e58b26_constants(void) {
    json_t *out = json_object();
    json_set(out, "custom_label", json_string("Enter custom model name"));
    json_set(out, "skip_label", json_string("Skip (keep current)"));
    json_set(out, "current_suffix", json_string("  \xe2\x86\x90 currently in use"));
    json_set(out, "cost_col", json_number(5));
    json_set(out, "gauge_gap", json_string("  "));
    json_t *style = json_object();
    json_set(style, "free", json_string("green"));
    json_set(style, "top_tier", json_string("yellow"));
    json_set(out, "level_style", style);
    return out;
}
