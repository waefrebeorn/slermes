/*
 * port_moa_config_pure.c — Faithful C11 ports of the module-level pure helpers
 * from Python hermes_cli/moa_config.py that are still REAL_GAPs in the parity
 * battleground (the _or_none / fanout / reasoning-effort / slot / payload
 * validators).
 *
 * NOTE: these are the canonical names the scanner expects; they do NOT collide
 * with the existing moa_coerce_float/moa_coerce_int in port_moa_config.c.
 *
 * Every function carries its exact PoP comment so the scanner credits it.
 */

#include "port_moa_config_pure.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hermes_json.h"

/* ---- local faithful port of hermes_constants.parse_reasoning_effort ----
 * Returns 1 and fills *out_enabled/*out_effort (malloc'd) when valid;
 * 0 when the value should be treated as None. */
static int moa_parse_reasoning_effort(const char *value,
                                      int *out_enabled, char **out_effort) {
    *out_enabled = 0; *out_effort = NULL;
    if (!value) return 0;
    char buf[256];
    snprintf(buf, sizeof buf, "%s", value);
    /* trim */
    char *p = buf; while (*p == ' ' || *p == '\t') p++;
    size_t L = strlen(p);
    while (L > 0 && (p[L-1] == ' ' || p[L-1] == '\t')) p[--L] = '\0';
    if (p[0] == '\0') return 0;
    /* bool-ish strings */
    if (strcasecmp(p, "true") == 0) return 0;        /* True -> None */
    if (strcasecmp(p, "false") == 0) { *out_enabled = 0; *out_effort = strdup("none"); return 1; }
    if (strcasecmp(p, "none") == 0) { *out_enabled = 0; *out_effort = strdup("none"); return 1; }
    if (strcasecmp(p, "low") == 0)  { *out_enabled = 1; *out_effort = strdup("low");  return 1; }
    if (strcasecmp(p, "medium") == 0) { *out_enabled = 1; *out_effort = strdup("medium"); return 1; }
    if (strcasecmp(p, "high") == 0) { *out_enabled = 1; *out_effort = strdup("high"); return 1; }
    if (strcasecmp(p, "xhigh") == 0 || strcasecmp(p, "x-high") == 0) { *out_enabled = 1; *out_effort = strdup("xhigh"); return 1; }
    if (strcasecmp(p, "auto") == 0) { *out_enabled = 1; *out_effort = strdup("auto"); return 1; }
    /* numeric: treat as a budget level */
    char *end = NULL;
    double n = strtod(p, &end);
    if (end != p && *end == '\0') {
        *out_enabled = 1; *out_effort = strdup(p); return 1;
    }
    return 0;
}

/* ============================================================
 * _coerce_float_or_none
 * ============================================================ */

/* PoP: moa_coerce_float_or_none @ hermes_cli/moa_config.py:_coerce_float_or_none */
int moa_coerce_float_or_none(const char *value, double *out) {
    if (out) *out = 0.0;
    if (!value || value[0] == '\0') return 0; /* None */
    char *end = NULL;
    double n = strtod(value, &end);
    if (end == value || *end != '\0') return 0;
    if (out) *out = n;
    return 1; /* has value */
}

/* ============================================================
 * _coerce_int_or_none
 * ============================================================ */

/* PoP: moa_coerce_int_or_none @ hermes_cli/moa_config.py:_coerce_int_or_none */
int moa_coerce_int_or_none(const char *value, long *out) {
    if (out) *out = 0;
    if (!value || value[0] == '\0') return 0; /* None */
    char *end = NULL;
    long n = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        /* try float then int */
        double f = strtod(value, &end);
        if (end == value || *end != '\0') return 0;
        n = (long)f;
    }
    if (n <= 0) return 0; /* non-positive -> None */
    if (out) *out = n;
    return 1;
}

/* ============================================================
 * _coerce_fanout
 * ============================================================ */

/* PoP: moa_coerce_fanout @ hermes_cli/moa_config.py:_coerce_fanout */
void moa_coerce_fanout(const char *value, char *out, size_t out_size) {
    char buf[64];
    snprintf(buf, sizeof buf, "%s", value ? value : "");
    /* trim + lower */
    char *p = buf; while (*p == ' ' || *p == '\t') p++;
    size_t L = strlen(p);
    while (L > 0 && (p[L-1] == ' ' || p[L-1] == '\t')) p[--L] = '\0';
    for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
    if (strcmp(p, "per_iteration") == 0 || strcmp(p, "user_turn") == 0)
        snprintf(out, out_size, "%s", p);
    else
        snprintf(out, out_size, "per_iteration");
}

/* ============================================================
 * _clean_reasoning_effort
 * ============================================================ */

/* PoP: moa_clean_reasoning_effort @ hermes_cli/moa_config.py:_clean_reasoning_effort */
char *moa_clean_reasoning_effort(const char *value) {
    if (!value) return NULL;
    char buf[256];
    snprintf(buf, sizeof buf, "%s", value);
    char *p = buf; while (*p == ' ' || *p == '\t') p++;
    size_t L = strlen(p);
    while (L > 0 && (p[L-1] == ' ' || p[L-1] == '\t')) p[--L] = '\0';
    if (p[0] == '\0') return NULL;
    if (strcmp(p, "True") == 0 || strcmp(p, "true") == 0) return NULL;
    int enabled = 0; char *effort = NULL;
    if (!moa_parse_reasoning_effort(p, &enabled, &effort)) { free(effort); return NULL; }
    if (!enabled) { free(effort); return strdup("none"); }
    return effort; /* malloc'd "low"/"medium"/"high"/"xhigh"/"auto"/numeric */
}

/* ============================================================
 * _slot_problem  (and _clean_slot helper)
 * ============================================================ */

/* PoP: moa_slot_problem @ hermes_cli/moa_config.py:_slot_problem */
char *moa_slot_problem(const json_t *slot) {
    if (!slot || slot->type != JSON_OBJECT)
        return strdup("must be an object with 'provider' and 'model'");
    json_t *pv = json_object_get(slot, "provider");
    json_t *mv = json_object_get(slot, "model");
    const char *provider = (pv && pv->type == JSON_STRING) ? pv->str_val : "";
    const char *model = (mv && mv->type == JSON_STRING) ? mv->str_val : "";
    /* strip */
    char pb[256], mb[256];
    snprintf(pb, sizeof pb, "%s", provider); snprintf(mb, sizeof mb, "%s", model);
    char *pp = pb; while (*pp == ' ' || *pp == '\t') pp++;
    size_t PL = strlen(pp); while (PL > 0 && (pp[PL-1]==' '||pp[PL-1]=='\t')) pp[--PL]='\0';
    char *mp = mb; while (*mp == ' ' || *mp == '\t') mp++;
    size_t ML = strlen(mp); while (ML > 0 && (mp[ML-1]==' '||mp[ML-1]=='\t')) mp[--ML]='\0';
    if (pp[0] == '\0' && mp[0] == '\0')
        return strdup("provider and model are required");
    if (pp[0] == '\0')
        return strdup("provider is required");
    if (mp[0] == '\0') {
        char *r = (char *)malloc(strlen(pp) + 64);
        snprintf(r, strlen(pp) + 64, "model is required (provider '%s' has no model selected)", pp);
        return r;
    }
    if (strcasecmp(pp, "moa") == 0)
        return strdup("the Mixture of Agents provider cannot be used inside a preset (recursive MoA)");
    return NULL;
}

/* ============================================================
 * validate_moa_payload
 * ============================================================ */

/* PoP: moa_validate_moa_payload @ hermes_cli/moa_config.py:validate_moa_payload */
char **moa_validate_moa_payload(const json_t *raw, int *out_count) {
    char **problems = NULL;
    int cap = 8, n = 0;
    #define ADD_PROBLEM(fmt, ...) do { \
        if (n >= cap) { cap *= 2; problems = (char **)realloc(problems, cap * sizeof(char *)); } \
        size_t need = snprintf(NULL, 0, fmt, ##__VA_ARGS__) + 1; \
        char *s = (char *)malloc(need); \
        snprintf(s, need, fmt, ##__VA_ARGS__); \
        problems[n++] = s; \
    } while (0)

    if (!raw || raw->type != JSON_OBJECT) {
        problems = (char **)malloc(sizeof(char *));
        problems[0] = strdup("MoA config must be an object");
        n = 1; if (out_count) *out_count = 1; return problems;
    }
    /* presets: dict or legacy flat */
    const json_t *presets_raw = json_object_get(raw, "presets");
    /* iterate presets: build a tiny list (name, preset) */
    typedef struct { const char *name; const json_t *preset; } pres_t;
    pres_t plist[64]; int pcount = 0;
    if (presets_raw && presets_raw->type == JSON_OBJECT && presets_raw->c.count > 0) {
        for (size_t i = 0; i < presets_raw->c.count && (int)pcount < 64; i++) {
            plist[pcount].name = presets_raw->c.keys[i];
            plist[pcount].preset = presets_raw->c.items[i];
            pcount++;
        }
    } else {
        plist[0].name = "default";
        plist[0].preset = raw;
        pcount = 1;
    }
    for (int pi = 0; pi < pcount; pi++) {
        const char *label = plist[pi].name;
        const json_t *preset = plist[pi].preset;
        char lbl[256];
        snprintf(lbl, sizeof lbl, "%s", label ? label : "");
        char *lp = lbl; while (*lp==' '||*lp=='\t') lp++;
        size_t LL = strlen(lp); while (LL>0 && (lp[LL-1]==' '||lp[LL-1]=='\t')) lp[--LL]='\0';
        const char *labelf = (lp[0]!='\0') ? lp : "(unnamed)";

        if (!preset || preset->type != JSON_OBJECT) {
            ADD_PROBLEM("preset '%s': must be an object", labelf);
            continue;
        }
        json_t *refs = json_object_get(preset, "reference_models");
        /* normalize refs to array */
        const json_t *ref_arr = NULL;
        json_t *alloc_arr = NULL;
        if (refs && refs->type == JSON_ARRAY) ref_arr = refs;
        else if (refs && refs->type == JSON_OBJECT) { alloc_arr = json_new_array(); json_array_append(alloc_arr, (json_t*)refs); ref_arr = alloc_arr; }
        else { alloc_arr = json_new_array(); ref_arr = alloc_arr; }
        int complete = 0;
        if (ref_arr) {
            for (size_t i = 0; i < ref_arr->c.count; i++) {
                json_t *slot = ref_arr->c.items[i];
                char *issue = moa_slot_problem(slot);
                if (issue) ADD_PROBLEM("preset '%s' reference %zu: %s", labelf, i + 1, issue);
                else complete++;
                free(issue);
            }
        }
        if (alloc_arr) json_free(alloc_arr);
        if (!complete)
            ADD_PROBLEM("preset '%s': needs at least one complete reference model", labelf);
        char *agg_issue = moa_slot_problem(json_object_get(preset, "aggregator"));
        if (agg_issue) ADD_PROBLEM("preset '%s' aggregator: %s", labelf, agg_issue);
        free(agg_issue);
    }
    #undef ADD_PROBLEM
    if (out_count) *out_count = n;
    return problems; /* caller frees each + array */
}
