/*
 * agent_init_fallback.c - faithful C11 port of pure helpers from upstream
 * Hermes agent/agent_init_fallback.py (gate w11_agentinit_f7c2eb).
 *
 * PoP: w11_agentinit_f7c2eb_fallback_entries @ agent/agent_init_fallback.py:_fallback_entries
 * PoP: w11_agentinit_f7c2eb_labels @ agent/agent_init_fallback.py:_init_fallback_chain
 * PoP: w11_agentinit_f7c2eb_message @ agent/agent_init_fallback.py:_init_fallback_chain
 *
 * Agent mutation / client construction (_init_fallback_chain,
 * recompute_init_fallback_api_mode) is stateful and not ported; only the
 * normalization and display logic is mirrored here.
 */
#include "noc_w11_agentinit_f7c2eb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* local strdup wrapper (portable name) */
static char *sdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *o = (char *)malloc(n);
    if (o) memcpy(o, s, n);
    return o;
}

/* Python truthiness of a parsed JSON value. */
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

json_t *w11_agentinit_f7c2eb_fallback_entries(const json_t *fallback_model) {
    json_t *out = json_array();
    if (!out) return NULL;
    if (!fallback_model) return out;
    const json_t *list = NULL;
    json_t *single = NULL;
    if (fallback_model->type == JSON_OBJECT) {
        single = json_array();
        json_append(single, json_copy(fallback_model));
        list = single;
    } else if (fallback_model->type == JSON_ARRAY) {
        list = fallback_model;
    }
    if (list) {
        for (size_t i = 0; i < list->c.count; i++) {
            json_t *f = list->c.items[i];
            if (f && f->type == JSON_OBJECT &&
                truthy(json_obj_get(f, "provider")) && truthy(json_obj_get(f, "model"))) {
                json_append(out, json_copy(f));
            }
        }
    }
    json_free(single);
    return out;
}

json_t *w11_agentinit_f7c2eb_labels(const json_t *chain) {
    json_t *out = json_array();
    if (!out) return NULL;
    if (!chain || chain->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < chain->c.count; i++) {
        json_t *f = chain->c.items[i];
        json_t *mv = f ? json_obj_get(f, "model") : NULL;
        json_t *pv = f ? json_obj_get(f, "provider") : NULL;
        const char *m = (mv && mv->type == JSON_STRING) ? (mv->str_val ? mv->str_val : "") : "";
        const char *p = (pv && pv->type == JSON_STRING) ? (pv->str_val ? pv->str_val : "") : "";
        size_t n = strlen(m) + strlen(" ()") + strlen(p) + 1;
        char *label = (char *)malloc(n);
        if (label) {
            snprintf(label, n, "%s (%s)", m, p);
            json_append(out, json_string(label));
            free(label);
        }
    }
    return out;
}

char *w11_agentinit_f7c2eb_message(const json_t *chain) {
    if (!chain || chain->type != JSON_ARRAY || chain->c.count == 0) return sdup("");
    json_t *labels = w11_agentinit_f7c2eb_labels(chain);
    if (!labels) return NULL;
    size_t n = 0;
    char *l0 = NULL;
    if (labels->c.count) {
        json_t *v = labels->c.items[0];
        l0 = (v && v->type == JSON_STRING) ? sdup(v->str_val ? v->str_val : "") : NULL;
    }
    if (chain->c.count == 1) {
        const char *prefix = "\xF0\x9F\x94\x84 Fallback model: "; /* 🔄 Fallback model: */
        size_t need = strlen(prefix) + strlen(l0 ? l0 : "") + 1;
        char *out = (char *)malloc(need);
        if (out) snprintf(out, need, "%s%s", prefix, l0 ? l0 : "");
        free(l0); json_free(labels);
        return out;
    }
    const char *prefix = "\xF0\x9F\x94\x84 Fallback chain (";  /* 🔄 Fallback chain ( */
    const char *mid = " providers): ";
    const char *arrow = " \xE2\x86\x92 ";                       /* " → " */
    size_t need = strlen(prefix) + 32 + strlen(mid) + 1;
    for (size_t i = 0; i < labels->c.count; i++) {
        json_t *v = labels->c.items[i];
        need += strlen((v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : "") + strlen(arrow);
    }
    char *out = (char *)malloc(need + 8);
    if (!out) { free(l0); json_free(labels); return NULL; }
    int w = snprintf(out, need + 8, "%s%zu%s", prefix, chain->c.count, mid);
    size_t pos = (w > 0) ? (size_t)w : 0;
    for (size_t i = 0; i < labels->c.count; i++) {
        json_t *v = labels->c.items[i];
        const char *s = (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : "";
        if (i) { memcpy(out + pos, arrow, strlen(arrow)); pos += strlen(arrow); }
        memcpy(out + pos, s, strlen(s)); pos += strlen(s);
    }
    out[pos] = '\0';
    free(l0); json_free(labels);
    return out;
}
