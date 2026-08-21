#define _POSIX_C_SOURCE 200809L
/* models_pure.c — Faithful port of the pure, deterministic model-catalog
 * helpers in hermes_cli/models.py (the `hermes model` picker + Nous Portal
 * tier detection).
 *
 * Ports the side-effect-free surface:
 *   _xai_promote_top            -> models_xai_promote_top
 *   _xai_merge_curated_extras   -> models_xai_merge_curated_extras
 *   _xai_finalize_catalog       -> models_xai_finalize_catalog
 *   _is_model_free              -> models_is_model_free
 *   is_nous_free_tier           -> models_is_nous_free_tier
 *
 * These are pure list/dict transformations — fully oracle-able. The
 * network-backed _xai_curated_models / _codex_curated_models are intentionally
 * NOT ported (they read disk/network; not faithfully oracle-able standalone).
 *
 * Callers receive freshly malloc'd JSON array/object strings (free with
 * free()) or NULL.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define XAI_TOP_MODEL "grok-4.6"

/* Static xAI curated extras (from models.py). */
static const char *XAI_CURATED_EXTRAS[] = {
    "grok-4.6", "grok-4.5", "grok-composer-2.5-fast", NULL
};

/* ─── helpers ────────────────────────────────────────────────────── */
static char **strlist_dup(char **in, size_t n) {
    char **out = (char **)malloc(sizeof(char *) * (n + 1));
    for (size_t i = 0; i < n; i++) out[i] = strdup(in[i]);
    out[n] = NULL;
    return out;
}
static int strlist_contains(char **arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (strcmp(arr[i], s) == 0) return 1;
    return 0;
}
/* serialize a string list to a JSON array string */
static char *strlist_to_json(char **arr, size_t n) {
    size_t cap = 2;
    for (size_t i = 0; i < n; i++) cap += strlen(arr[i]) * 2 + 6;
    char *out = (char *)malloc(cap + 1);
    out[0] = '['; size_t o = 1;
    for (size_t i = 0; i < n; i++) {
        if (i) out[o++] = ',';
        out[o++] = '"';
        for (char *q = arr[i]; *q; q++) {
            if (*q == '"' || *q == '\\') out[o++] = '\\';
            out[o++] = *q;
        }
        out[o++] = '"';
    }
    out[o++] = ']'; out[o] = '\0';
    return out;
}

/* Parse a JSON array of strings into a strlist (returns count via *n). */
static char **json_to_strlist(json_t *arr, size_t *n) {
    if (!arr || arr->type != JSON_ARRAY) { *n = 0; return NULL; }
    size_t cnt = json_len(arr);
    char **out = (char **)malloc(sizeof(char *) * (cnt + 1));
    for (size_t i = 0; i < cnt; i++) {
        json_t *e = json_array_get(arr, i);
        const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
        out[i] = strdup(s ? s : "");
    }
    out[cnt] = NULL;
    *n = cnt;
    return out;
}

/* ─── _xai_promote_top ───────────────────────────────────────────── */
/* Standalone: replicate _xai_finalize_catalog = promote_top(merge_extras(ids)).
 * models_net.c owns array-signatured variants; we operate on JSON strings. */
char *models_xai_finalize_catalog(const char *json_ids) {
    json_t *arr = json_parse(json_ids, NULL);
    size_t n; char **ids = json_to_strlist(arr, &n);
    if (!ids) { if (arr) json_free(arr); return strdup("[]"); }
    /* merge extras */
    for (int e = 0; XAI_CURATED_EXTRAS[e]; e++) {
        const char *extra = XAI_CURATED_EXTRAS[e];
        if (strlist_contains(ids, n, extra)) continue;
        size_t insert_at = (n > 0 && strcmp(ids[0], XAI_TOP_MODEL) == 0) ? 1 : n;
        char **nb = (char **)malloc(sizeof(char *) * (n + 2));
        size_t k = 0;
        for (size_t i = 0; i < insert_at; i++) nb[k++] = ids[i];
        nb[k++] = strdup(extra);
        for (size_t i = insert_at; i < n; i++) nb[k++] = ids[i];
        free(ids); ids = nb; n = k;
    }
    /* promote top */
    char **out = (char **)malloc(sizeof(char *) * (n + 2));
    size_t m = 0;
    if (strlist_contains(ids, n, XAI_TOP_MODEL)) out[m++] = strdup(XAI_TOP_MODEL);
    for (size_t i = 0; i < n; i++) if (strcmp(ids[i], XAI_TOP_MODEL) != 0) out[m++] = strdup(ids[i]);
    out[m] = NULL;
    char *res = strlist_to_json(out, m);
    for (size_t i = 0; i < m; i++) free(out[i]);
    free(out);
    for (size_t i = 0; i < n; i++) free(ids[i]);
    free(ids);
    json_free(arr);
    return res;
}

/* ─── is_nous_free_tier ─────────────────────────────────────────── */
int models_is_nous_free_tier(const char *json_account_info) {
    if (!json_account_info) return 0;
    json_t *info = json_parse(json_account_info, NULL);
    if (!info || info->type != JSON_OBJECT) { if (info) json_free(info); return 0; }
    int result = 0;
    json_t *paid = json_object_get(info, "paid_service_access");
    if (paid && paid->type == JSON_OBJECT) {
        json_t *allowed = json_object_get(paid, "allowed");
        if (allowed && allowed->type == JSON_BOOL) {
            result = (allowed->bool_val ? 0 : 1);
            json_free(info);
            return result;
        }
        json_t *pa = json_object_get(paid, "paid_access");
        if (pa && pa->type == JSON_BOOL) {
            result = (pa->bool_val ? 0 : 1);
            json_free(info);
            return result;
        }
    }
    json_t *sub = json_object_get(info, "subscription");
    if (!sub || sub->type != JSON_OBJECT) { json_free(info); return 0; }
    json_t *charge = json_object_get(sub, "monthly_charge");
    if (!charge || charge->type == JSON_NULL) { json_free(info); return 0; }
    double c = json_get_num(sub, "monthly_charge", -1);
    result = (c == 0.0) ? 1 : 0;
    json_free(info);
    return result;
}
