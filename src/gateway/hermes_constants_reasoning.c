/*
 * port_hermes_constants_reasoning.c — Faithful C11 port of the reasoning-effort
 * resolution chokepoint from hermes_constants.py.
 *
 * Reuses reasoning_parse_effort (port_slash_commands.c) as the leaf parser and
 * builds the resolution layer: tolerant model-name variants, per-model override
 * lookup, and the shared resolve_reasoning_config chokepoint.
 */

#include "port_hermes_constants_reasoning.h"
#include "port_slash_commands.h"   /* reasoning_parse_effort */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── small string helpers ─────────────────────────────────────────── */

static char *xstrdup2(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *o = malloc(n);
    if (o) memcpy(o, s, n);
    return o;
}

/* replace every occurrence of character `from` with `to` (malloc'd copy). */
static char *replace_char(const char *s, char from, char to) {
    char *o = xstrdup2(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) if (*p == from) *p = to;
    return o;
}

/* version-separator recovery: swap `sep` for `rep` only between two ASCII
 * digits (mirrors re.sub(r'(\d)SEP(\d)', ...), NON-OVERLAPPING: each match
 * consumes all three chars, so a trailing digit can't start the next match).
 * malloc'd. */
static char *version_sep_swap(const char *s, char sep, char rep) {
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    size_t w = 0;
    for (size_t j = 0; j < n; ) {
        if (j + 2 < n && isdigit((unsigned char)s[j]) &&
            s[j + 1] == sep && isdigit((unsigned char)s[j + 2])) {
            o[w++] = s[j];      /* leading digit */
            o[w++] = rep;       /* swapped separator */
            o[w++] = s[j + 2];  /* trailing digit */
            j += 3;             /* consume full match (non-overlapping) */
        } else {
            o[w++] = s[j++];
        }
    }
    o[w] = '\0';
    return o;
}

/* dynamic string-vector with insertion-order dedupe. */
typedef struct {
    char **items;
    int count;
    int cap;
} strvec_t;

static void sv_init(strvec_t *v) { v->items = NULL; v->count = 0; v->cap = 0; }

static void sv_add(strvec_t *v, char *owned) {
    if (!owned || owned[0] == '\0') { free(owned); return; }
    for (int i = 0; i < v->count; i++) {
        if (strcmp(v->items[i], owned) == 0) { free(owned); return; }
    }
    if (v->count >= v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->items = realloc(v->items, (size_t)v->cap * sizeof(char *));
    }
    v->items[v->count++] = owned;
}

/* add s plus its dots↔dashes and version-dot derivatives (mirrors
 * _add_with_derivatives). */
static void sv_add_derivatives(strvec_t *v, const char *s) {
    sv_add(v, xstrdup2(s));
    char *all_dashed = replace_char(s, '.', '-');
    sv_add(v, xstrdup2(all_dashed));
    char *all_dotted = replace_char(s, '-', '.');
    sv_add(v, xstrdup2(all_dotted));
    /* version-dot recovery on each base form */
    sv_add(v, version_sep_swap(s, '-', '.'));           /* _dash_to_dot(s) */
    sv_add(v, version_sep_swap(s, '.', '-'));           /* _dot_to_dash(s) */
    sv_add(v, version_sep_swap(all_dashed, '-', '.'));  /* _dash_to_dot(all_dashed) */
    sv_add(v, version_sep_swap(all_dotted, '.', '-'));  /* _dot_to_dash(all_dotted) */
    free(all_dashed);
    free(all_dotted);
}

/* ── _canonical_model_variants ─────────────────────────────────────── */
/* PoP: reasoning_canonical_model_variants @ hermes_constants.py:_canonical_model_variants */
char **reasoning_canonical_model_variants(const char *model, int *out_count) {
    if (out_count) *out_count = 0;
    if (!model || model[0] == '\0') return NULL;

    strvec_t v;
    sv_init(&v);

    /* 1-3. Base variants for the full string. */
    sv_add_derivatives(&v, model);

    /* Split by '/'. */
    int nparts = 1;
    for (const char *p = model; *p; p++) if (*p == '/') nparts++;

    /* 4. Bare model variants (strip provider/aggregator prefix). */
    if (nparts >= 2) {
        const char *bare = strrchr(model, '/');
        bare = bare ? bare + 1 : model;
        sv_add_derivatives(&v, bare);
    }
    /* Strip aggregator only (3+ parts): join parts[1:]. */
    if (nparts >= 3) {
        const char *after_first = strchr(model, '/');
        if (after_first) sv_add_derivatives(&v, after_first + 1);
    }

    /* 5. Prepend known provider prefixes to bare variants. */
    static const char *known_providers[] = {
        "anthropic", "openai", "google", "openrouter", "groq", "mistral",
        "xai", "cohere", "perplexity", "together", "fireworks", "deepseek", NULL
    };
    static const char *known_aggregators[] = {
        "openrouter", "opencode", "fireworks", "groq", "together", NULL
    };

    /* snapshot current variants before step-5 mutation (Python's bare_variants
     * is [v for v in variants if '/' not in v] at THIS point). */
    int base_n = v.count;
    char **base_snapshot = malloc((size_t)base_n * sizeof(char *));
    for (int i = 0; i < base_n; i++) base_snapshot[i] = v.items[i];

    for (int i = 0; i < base_n; i++) {
        const char *cur = base_snapshot[i];
        if (strchr(cur, '/') != NULL) continue;   /* bare only */
        for (int p = 0; known_providers[p]; p++) {
            size_t L = strlen(known_providers[p]) + 1 + strlen(cur) + 1;
            char *buf = malloc(L);
            snprintf(buf, L, "%s/%s", known_providers[p], cur);
            sv_add(&v, buf);
        }
    }
    free(base_snapshot);

    /* Prepend aggregator to single-slash variants. Python computes
     * single_slash_variants AFTER step 5's _add calls, so it INCLUDES the
     * provider/bare entries just added (each has exactly one slash). Snapshot
     * the LIVE list here. */
    int mid_n = v.count;
    char **mid_snapshot = malloc((size_t)mid_n * sizeof(char *));
    for (int i = 0; i < mid_n; i++) mid_snapshot[i] = v.items[i];
    for (int i = 0; i < mid_n; i++) {
        const char *cur = mid_snapshot[i];
        int slashes = 0;
        for (const char *p = cur; *p; p++) if (*p == '/') slashes++;
        if (slashes != 1) continue;
        for (int a = 0; known_aggregators[a]; a++) {
            size_t L = strlen(known_aggregators[a]) + 1 + strlen(cur) + 1;
            char *buf = malloc(L);
            snprintf(buf, L, "%s/%s", known_aggregators[a], cur);
            sv_add(&v, buf);
        }
    }
    free(mid_snapshot);

    /* NULL-terminate. */
    v.items = realloc(v.items, (size_t)(v.count + 1) * sizeof(char *));
    v.items[v.count] = NULL;
    if (out_count) *out_count = v.count;
    return v.items;
}

static void free_variants(char **arr) {
    if (!arr) return;
    for (int i = 0; arr[i]; i++) free(arr[i]);
    free(arr);
}

/* ── resolve_per_model_reasoning_effort ────────────────────────────── */
/* PoP: reasoning_resolve_per_model @ hermes_constants.py:resolve_per_model_reasoning_effort */
json_t *reasoning_resolve_per_model(const char *model, const json_t *overrides) {
    if (!overrides || overrides->type != JSON_OBJECT || !model || !model[0])
        return NULL;

    int vc = 0;
    char **variants = reasoning_canonical_model_variants(model, &vc);
    if (!variants) return NULL;

    json_t *result = NULL;
    for (int i = 0; variants[i]; i++) {
        json_t *val = json_obj_get((json_t *)overrides, variants[i]);
        if (!val) continue;
        /* parse_reasoning_effort(overrides[variant]) — first non-None wins. */
        bool is_bool_false = (val->type == JSON_BOOL && val->bool_val == false);
        const char *sval = (val->type == JSON_STRING) ? val->str_val : NULL;
        json_t *parsed = reasoning_parse_effort(sval, is_bool_false);
        if (parsed) { result = parsed; break; }
    }
    free_variants(variants);
    return result;
}

/* ── resolve_reasoning_config ──────────────────────────────────────── */
/* Derive the model string from cfg's "model" section when caller passed "". */
static char *derive_model_from_cfg(const json_t *cfg) {
    json_t *mc = json_obj_get((json_t *)cfg, "model");
    if (!mc) return xstrdup2("");
    if (mc->type == JSON_STRING) {
        /* .strip() */
        const char *s = mc->str_val ? mc->str_val : "";
        while (*s && isspace((unsigned char)*s)) s++;
        const char *e = s + strlen(s);
        while (e > s && isspace((unsigned char)e[-1])) e--;
        size_t n = (size_t)(e - s);
        char *o = malloc(n + 1);
        memcpy(o, s, n); o[n] = '\0';
        return o;
    }
    if (mc->type == JSON_OBJECT) {
        json_t *d = json_obj_get(mc, "default");
        const char *pick = (d && d->type == JSON_STRING) ? d->str_val : NULL;
        if (!pick || !pick[0]) {
            json_t *m2 = json_obj_get(mc, "model");
            pick = (m2 && m2->type == JSON_STRING) ? m2->str_val : NULL;
        }
        if (!pick) pick = "";
        /* .strip() */
        while (*pick && isspace((unsigned char)*pick)) pick++;
        const char *e = pick + strlen(pick);
        while (e > pick && isspace((unsigned char)e[-1])) e--;
        size_t n = (size_t)(e - pick);
        char *o = malloc(n + 1);
        memcpy(o, pick, n); o[n] = '\0';
        return o;
    }
    return xstrdup2("");
}

/* PoP: reasoning_resolve_config @ hermes_constants.py:resolve_reasoning_config */
json_t *reasoning_resolve_config(const json_t *cfg, const char *model) {
    json_t empty_obj_storage;
    const json_t *use_cfg = (cfg && cfg->type == JSON_OBJECT) ? cfg : NULL;
    (void)empty_obj_storage;

    json_t *agent_cfg = use_cfg ? json_obj_get((json_t *)use_cfg, "agent") : NULL;
    if (agent_cfg && agent_cfg->type != JSON_OBJECT) agent_cfg = NULL;

    char *derived = NULL;
    const char *eff_model = model;
    if ((!model || !model[0]) && use_cfg) {
        derived = derive_model_from_cfg(use_cfg);
        eff_model = derived;
    } else if (!model) {
        eff_model = "";
    }

    /* 1. Per-model override. */
    json_t *overrides = agent_cfg ? json_obj_get(agent_cfg, "reasoning_overrides") : NULL;
    if (overrides && overrides->type == JSON_OBJECT) {
        json_t *per = reasoning_resolve_per_model(eff_model, overrides);
        if (per) { free(derived); return per; }
    }

    /* 2. Global agent.reasoning_effort — keep the raw value. */
    json_t *effort = agent_cfg ? json_obj_get(agent_cfg, "reasoning_effort") : NULL;
    free(derived);

    if (!effort) return NULL;   /* default "" -> parse -> None */
    if (effort->type == JSON_BOOL)
        return reasoning_parse_effort(NULL, effort->bool_val == false);
    if (effort->type == JSON_STRING)
        return reasoning_parse_effort(effort->str_val, false);
    /* Non-string/non-bool (e.g. null/number) -> parse(str(effort)) semantics.
     * YAML null -> None; numbers are unrecognized -> None. */
    return NULL;
}
