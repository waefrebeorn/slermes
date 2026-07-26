/*
 * port_moa_config_pure.c — Faithful C11 ports of the remaining module-level
 * pure helpers from hermes_cli/moa_config.py that were not covered by
 * port_moa_config.c (which ports normalize_moa_config and friends).
 *
 * Each carries /* PoP: c_fn @ hermes_cli/moa_config.py:py_fn *\/ so the parity
 * scanner credits it. No async/runtime coupling — pure data transforms.
 */

#include "port_moa_config_pure.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "hermes_json.h"   /* json_t */

/* PoP: moa_coerce_float_or_none @ hermes_cli/moa_config.py:_coerce_float_or_none */
/* Returns 1 and sets *out on success, returns 0 (None) when unset/blank/invalid. */
int moa_coerce_float_or_none(const char *value, double *out) {
    if (!value || !*value) return 0;
    char *end = NULL;
    double d = strtod(value, &end);
    if (end == value || *end != '\0') return 0;
    *out = d;
    return 1;
}

/* PoP: moa_coerce_int @ hermes_cli/moa_config.py:_coerce_int */
int moa_coerce_int(const char *value, int def) {
    if (!value || !*value) return def;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        /* try float→int */
        char *e2 = NULL;
        double d = strtod(value, &e2);
        if (e2 == value || *e2 != '\0') return def;
        return (int)d;
    }
    return (int)v;
}

/* PoP: moa_coerce_int_or_none @ hermes_cli/moa_config.py:_coerce_int_or_none */
/* Returns 1 + *out (positive int) or 0 (None) when unset/blank/invalid/<=0. */
int moa_coerce_int_or_none(const char *value, long *out) {
    if (!value || !*value) return 0;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        char *e2 = NULL;
        double d = strtod(value, &e2);
        if (e2 == value || *e2 != '\0') return 0;
        v = (long)d;
    }
    if (v <= 0) return 0;
    *out = v;
    return 1;
}

/* PoP: moa_coerce_fanout @ hermes_cli/moa_config.py:_coerce_fanout */
/* Returns malloc'd normalized fanout ("per_iteration" or "user_turn"). */
char *moa_coerce_fanout(const char *value) {
    char buf[32];
    if (value) {
        size_t i = 0;
        for (; value[i] && i < sizeof buf - 1; i++)
            buf[i] = (char)tolower((unsigned char)value[i]);
        buf[i] = '\0';
        if (strcmp(buf, "per_iteration") == 0) return strdup("per_iteration");
        if (strcmp(buf, "user_turn") == 0) return strdup("user_turn");
    }
    return strdup("per_iteration");
}

/* Canonicalize a reasoning-effort string (mirrors parse_reasoning_effort):
 * returns malloc'd canonical effort, "none" when disabled, or NULL when
 * unset/invalid. */
static char *clean_reasoning_effort_str(const char *value) {
    if (!value) return NULL;
    /* True → None */
    if (strcmp(value, "True") == 0 || strcmp(value, "true") == 0) return NULL;
    /* lower-case compare */
    char buf[32];
    size_t i = 0;
    for (; value[i] && i < sizeof buf - 1; i++)
        buf[i] = (char)tolower((unsigned char)value[i]);
    buf[i] = '\0';
    if (strcmp(buf, "none") == 0 || strcmp(buf, "false") == 0 || strcmp(buf, "off") == 0
        || strcmp(buf, "disabled") == 0 || strcmp(buf, "0") == 0)
        return strdup("none");
    /* known efforts */
    static const char *const known[] = {
        "low", "medium", "high", "xhigh", "none", NULL
    };
    for (int k = 0; known[k]; k++) {
        if (strcmp(buf, known[k]) == 0) return strdup(buf);
    }
    return NULL;  /* invalid → None */
}

/* PoP: moa_clean_reasoning_effort @ hermes_cli/moa_config.py:_clean_reasoning_effort */
char *moa_clean_reasoning_effort(const char *value) {
    return clean_reasoning_effort_str(value);
}

/* PoP: moa_slot_problem @ hermes_cli/moa_config.py:_slot_problem */
/* Returns malloc'd problem string, or NULL when the slot is valid. */
char *moa_slot_problem(const json_t *slot) {
    if (!slot || slot->type != JSON_OBJECT) {
        return strdup("must be an object with 'provider' and 'model'");
    }
    const char *provider = json_get_str(slot, "provider", "");
    const char *model = json_get_str(slot, "model", "");
    if (!provider[0] && !model[0])
        return strdup("provider and model are required");
    if (!provider[0])
        return strdup("provider is required");
    if (!model[0]) {
        char *s = malloc(256);
        snprintf(s, 256,
                 "model is required (provider '%s' has no model selected)", provider);
        return s;
    }
    if (strcmp(provider, "moa") == 0)
        return strdup("the Mixture of Agents provider cannot be used inside a preset (recursive MoA)");
    return NULL;
}

/* PoP: moa_validate_moa_payload @ hermes_cli/moa_config.py:validate_moa_payload */
/* Returns a JSON array string (malloc'd) of human-readable problem strings;
 * empty array means safe to save. */
char *moa_validate_moa_payload(const json_t *raw) {
    char **problems = NULL;
    int n = 0, cap = 8;
    problems = malloc(sizeof(char*) * cap);
    char buf[1024];

    /* gather presets */
    const json_t *presets_raw = raw ? json_obj_get(raw, "presets") : NULL;
    /* We iterate presets as (name, preset) pairs; libjson has no direct
     * key-iteration helper besides c.keys/c.items, so use them. */
    if (presets_raw && presets_raw->type == JSON_OBJECT && presets_raw->c.count > 0) {
        for (size_t i = 0; i < presets_raw->c.count; i++) {
            const char *name = presets_raw->c.keys[i];
            const json_t *preset = presets_raw->c.items[i];
            char label[256];
            snprintf(label, sizeof label, "%s", name ? name : "");
            if (!label[0]) strcpy(label, "(unnamed)");
            if (!preset || preset->type != JSON_OBJECT) {
                snprintf(buf, sizeof buf, "preset '%s': must be an object", label);
                if (n >= cap) { cap *= 2; problems = realloc(problems, sizeof(char*)*cap); }
                problems[n++] = strdup(buf);
                continue;
            }
            const json_t *refs = json_obj_get(preset, "reference_models");
            int complete = 0;
            if (refs && refs->type == JSON_ARRAY) {
                for (size_t r = 0; r < refs->c.count; r++) {
                    char *issue = moa_slot_problem(refs->c.items[r]);
                    if (issue) {
                        snprintf(buf, sizeof buf,
                                 "preset '%s' reference %zu: %s", label, r + 1, issue);
                        if (n >= cap) { cap *= 2; problems = realloc(problems, sizeof(char*)*cap); }
                        problems[n++] = strdup(buf);
                        free(issue);
                    } else complete++;
                }
            }
            if (!complete) {
                snprintf(buf, sizeof buf,
                         "preset '%s': needs at least one complete reference model", label);
                if (n >= cap) { cap *= 2; problems = realloc(problems, sizeof(char*)*cap); }
                problems[n++] = strdup(buf);
            }
            char *agg_issue = moa_slot_problem(json_obj_get(preset, "aggregator"));
            if (agg_issue) {
                snprintf(buf, sizeof buf, "preset '%s' aggregator: %s", label, agg_issue);
                if (n >= cap) { cap *= 2; problems = realloc(problems, sizeof(char*)*cap); }
                problems[n++] = strdup(buf);
                free(agg_issue);
            }
        }
    } else {
        /* Legacy flat payload: the top-level object is the default preset. */
        if (!raw || raw->type != JSON_OBJECT) {
            if (n < cap) problems[n++] = strdup("MoA config must be an object");
        } else {
            const json_t *refs = json_obj_get(raw, "reference_models");
            int complete = 0;
            if (refs && refs->type == JSON_ARRAY) {
                for (size_t r = 0; r < refs->c.count; r++) {
                    char *issue = moa_slot_problem(refs->c.items[r]);
                    if (issue) {
                        snprintf(buf, sizeof buf, "preset '(default)' reference %zu: %s", r + 1, issue);
                        if (n >= cap) { cap *= 2; problems = realloc(problems, sizeof(char*)*cap); }
                        problems[n++] = strdup(buf);
                        free(issue);
                    } else complete++;
                }
            }
            if (!complete) {
                if (n < cap) problems[n++] = strdup("preset '(default)': needs at least one complete reference model");
            }
            char *agg_issue = moa_slot_problem(json_obj_get(raw, "aggregator"));
            if (agg_issue) {
                snprintf(buf, sizeof buf, "preset '(default)' aggregator: %s", agg_issue);
                if (n < cap) problems[n++] = strdup(buf);
                free(agg_issue);
            }
        }
    }

    /* serialize to JSON array */
    size_t total = 2;
    for (int i = 0; i < n; i++) total += strlen(problems[i]) + 4;
    char *out = malloc(total + 1);
    out[0] = '['; out[1] = '\0';
    for (int i = 0; i < n; i++) {
        char jb[4096];
        snprintf(jb, sizeof jb, "\"%s\"%s", problems[i], (i + 1 < n) ? "," : "");
        strcat(out, jb);
        free(problems[i]);
    }
    strcat(out, "]");
    free(problems);
    return out;
}
