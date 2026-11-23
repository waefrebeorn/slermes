/*
 * model_catalog.c - faithful C11 port of pure helpers from upstream Hermes
 * acp_adapter/model_catalog.py (gate w11_modelcatal_563c9e).
 *
 * PoP: w11_modelcatal_563c9e_encode_model_choice @ acp_adapter/model_catalog.py:encode_model_choice
 * PoP: w11_modelcatal_563c9e_semantic_provider @ acp_adapter/model_catalog.py:_semantic_provider
 * PoP: w11_modelcatal_563c9e_empty_catalog_applies @ acp_adapter/model_catalog.py:_empty_catalog_applies
 *
 * Inventory loading / live model discovery / _ModelCatalog state machine need
 * the Hermes config, network and acp.schema objects and are not ported here.
 */
#include "noc_w11_modelcatal_563c9e.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *str_dup_n(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *str_dup(const char *s) { return str_dup_n(s ? s : "", strlen(s ? s : "")); }

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)p[a])) a++;
    while (b > a && py_isspace((unsigned char)p[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (!o) return NULL;
    for (size_t i = a; i < b; i++) o[i - a] = (char)tolower((unsigned char)p[i]);
    o[b - a] = '\0';
    return o;
}

static char *py_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)p[a])) a++;
    while (b > a && py_isspace((unsigned char)p[b - 1])) b--;
    return str_dup_n(p + a, b - a);
}

char *w11_modelcatal_563c9e_encode_model_choice(const char *provider, const char *model) {
    char *rm = py_strip(model);
    if (!rm || !*rm) { free(rm); return str_dup(""); }
    char *rp = py_strip_lower(provider);
    if (!rp || !*rp) { free(rp); return rm; }
    size_t n = strlen(rp) + 1 + strlen(rm) + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s:%s", rp, rm);
    free(rp); free(rm);
    return out;
}

char *w11_modelcatal_563c9e_semantic_provider(const char *provider_id, const char *normalized) {
    char *raw = py_strip_lower(provider_id);
    if (!raw) return str_dup("");
    if (strcmp(raw, "ollama") == 0 || strcmp(raw, "custom:ollama") == 0) {
        free(raw);
        return str_dup("ollama");
    }
    if (strncmp(raw, "custom:", 7) == 0) return raw;
    free(raw);
    return str_dup(normalized ? normalized : "");
}

/* normalize_provider seam: lookup by key, identity when absent. Returns owned str. */
static char *norm_lookup(const json_t *norm_map, const char *key) {
    if (norm_map && norm_map->type == JSON_OBJECT) {
        json_t *v = json_obj_get(norm_map, key);
        if (v && v->type == JSON_STRING) return str_dup(v->str_val ? v->str_val : "");
    }
    return str_dup(key);
}

bool w11_modelcatal_563c9e_empty_catalog_applies(const char *provider_id,
                                                  const json_t *candidates,
                                                  const json_t *norm_map) {
    char *raw = py_strip_lower(provider_id);
    char *normalized = norm_lookup(norm_map, raw ? raw : "");
    bool any = false;
    size_t count = (candidates && candidates->type == JSON_ARRAY) ? candidates->c.count : 0;
    for (size_t i = 0; i < count; i++) {
        json_t *cv = candidates->c.items[i];
        const char *cand = (cv && cv->type == JSON_STRING) ? (cv->str_val ? cv->str_val : "") : "";
        int hit;
        if (normalized && strcmp(normalized, "custom") == 0) {
            char *cp = NULL;
            size_t need = strlen("custom:") + strlen(cand) + 1;
            cp = (char *)malloc(need);
            if (cp) snprintf(cp, need, "custom:%s", cand);
            hit = (strcmp(cand, raw ? raw : "") == 0) ||
                  (cp && raw && strcmp(cp, raw) == 0) ||
                  (raw && strcmp(raw, "custom") == 0 && strcmp(cand, "custom") == 0);
            free(cp);
        } else {
            char *craw = NULL, *ccust = NULL, *ncmp = norm_lookup(norm_map, cand);
            size_t n1 = strlen("custom:") + strlen(normalized ? normalized : "") + 1;
            size_t n2 = strlen("custom:") + strlen(raw ? raw : "") + 1;
            craw = (char *)malloc(n1);
            ccust = (char *)malloc(n2);
            if (craw) snprintf(craw, n1, "custom:%s", normalized ? normalized : "");
            if (ccust) snprintf(ccust, n2, "custom:%s", raw ? raw : "");
            hit = (strcmp(cand, raw ? raw : "") == 0) ||
                  (craw && strcmp(cand, craw) == 0) ||
                  (ccust && strcmp(cand, ccust) == 0) ||
                  (ncmp && normalized && strcmp(ncmp, normalized) == 0);
            free(craw); free(ccust); free(ncmp);
        }
        if (hit) { any = true; break; }
    }
    free(raw); free(normalized);
    return any;
}
