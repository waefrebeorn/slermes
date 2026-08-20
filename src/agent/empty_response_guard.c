/*
 * empty_response_guard.c — Deterministic-empty detection + cost-aware retry
 * budgets (NS-503). Faithful port of agent/empty_response_guard.py.
 */

#include "empty_response_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "usage_pricing.h"

static void set_str(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) return;
    if (src) {
        strncpy(dst, src, n - 1);
        dst[n - 1] = '\0';
    } else {
        dst[0] = '\0';
    }
}

void empty_guard_init(empty_guard_t *g) {
    if (!g) return;
    free(g->attempts);  /* safe re-init (no leak on repeated init) */
    memset(g, 0, sizeof(*g));
    g->enabled = EMPTY_GUARD_DEFAULT_ENABLED;
    g->threshold_usd = EMPTY_GUARD_DEFAULT_COST_THRESHOLD;
    g->attempts = NULL;
    g->attempts_len = 0;
    g->attempts_cap = 0;
    g->streak_cost_usd = 0.0;
}

void empty_guard_free(empty_guard_t *g) {
    if (!g) return;
    free(g->attempts);
    g->attempts = NULL;
    g->attempts_len = 0;
    g->attempts_cap = 0;
}

bool resolve_guard_settings(const json_t *section, bool *out_enabled, double *out_threshold) {
    bool enabled = EMPTY_GUARD_DEFAULT_ENABLED;
    double threshold = EMPTY_GUARD_DEFAULT_COST_THRESHOLD;

    if (section && section->type == JSON_OBJECT) {
        /* enabled */
        const json_t *e = json_obj_get(section, "enabled");  /* may be bool/string/number */
        if (e) {
            if (e->type == JSON_BOOL) {
                enabled = e->bool_val;
            } else if (e->type == JSON_STRING) {
                /* YAML quoting can turn true/false into strings. */
                const char *s = e->str_val;
                enabled = !(s && (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 ||
                                  strcmp(s, "no") == 0 || strcmp(s, "off") == 0));
            } else if (e->type == JSON_NUMBER) {
                enabled = e->num_val != 0.0;
            }
            /* any other type -> schema default */
        }
        /* cost_threshold_usd */
        const json_t *t = json_obj_get(section, "cost_threshold_usd");
        if (t && t->type == JSON_NUMBER && t->num_val > 0.0) {
            threshold = t->num_val;
        }
        /* malformed (string etc.) falls back to default */
    }
    if (out_enabled) *out_enabled = enabled;
    if (out_threshold) *out_threshold = threshold;
    return true;
}

bool guard_enabled(const empty_guard_t *g) {
    return g ? g->enabled : EMPTY_GUARD_DEFAULT_ENABLED;
}

/* PoP: _zero_output @ agent/empty_response_guard.py:_zero_output */
/* Return (usage_present, zero_output) for a response, failing open. */
static void zero_output(const json_t *usage, const char *provider, const char *api_mode,
                         bool *out_usage_present, bool *out_zero_output) {
    *out_usage_present = false;
    *out_zero_output = false;
    if (!usage) return;
    usage_counts_t canonical = normalize_usage(provider ? provider : "",
                                               api_mode ? api_mode : "", usage);
    if (canonical.input_tokens <= 0) {
        /* A present-but-empty usage object, or one with no input tokens, is
         * not evidence — fail open (matches Python). */
        return;
    }
    long long output = canonical.output_tokens;
    long long reasoning = canonical.reasoning_tokens;  /* 0 if absent */
    *out_usage_present = true;
    *out_zero_output = ((output + reasoning) == 0);
}

/* PoP: _estimate_attempt_cost @ agent/empty_response_guard.py:_estimate_attempt_cost */
/* Best-effort USD estimate for one attempt. -1.0 when unknown. */
static double estimate_attempt_cost(const char *model, const char *provider,
                                     const json_t *usage, const char *api_mode,
                                     const char *base_url) {
    if (!usage) return -1.0;
    usage_counts_t canonical = normalize_usage(provider ? provider : "",
                                               api_mode ? api_mode : "", usage);
    if (canonical.input_tokens <= 0) return -1.0;
    usage_cost_t cost = usage_pricing_estimate(model ? model : "", &canonical);
    if (!usage_pricing_known(model ? model : "")) {
        /* included / subscription / unknown pricing: leave budget untouched */
        return -1.0;
    }
    return cost.total_cost;
}

static void attempts_push(empty_guard_t *g, const empty_attempt_t *a) {
    if (g->attempts_len == g->attempts_cap) {
        size_t nc = g->attempts_cap ? g->attempts_cap * 2 : 8;
        empty_attempt_t *na = realloc(g->attempts, nc * sizeof(*na));
        if (!na) return;  /* OOM: skip recording, fail open */
        g->attempts = na;
        g->attempts_cap = nc;
    }
    g->attempts[g->attempts_len++] = *a;
}

void record_empty_attempt(empty_guard_t *g,
                           size_t retry_count,
                           const char *model,
                           const char *provider,
                           const char *finish_reason,
                           const json_t *usage,
                           const char *api_mode,
                           const char *base_url) {
    if (!g) return;
    if (retry_count == 0) {
        g->attempts_len = 0;
        g->streak_cost_usd = 0.0;
    }
    bool usage_present = false, zero = false;
    zero_output(usage, provider, api_mode, &usage_present, &zero);

    empty_attempt_t a;
    memset(&a, 0, sizeof(a));
    set_str(a.model, sizeof(a.model), model);
    set_str(a.provider, sizeof(a.provider), provider);
    set_str(a.finish_reason, sizeof(a.finish_reason), finish_reason);
    a.usage_present = usage_present;
    a.zero_output = zero;
    attempts_push(g, &a);

    /* keep identity for later budget checks */
    set_str(g->model, sizeof(g->model), model);
    set_str(g->provider, sizeof(g->provider), provider);
    set_str(g->base_url, sizeof(g->base_url), base_url);
    set_str(g->api_mode, sizeof(g->api_mode), api_mode);

    double cost = estimate_attempt_cost(model, provider, usage, api_mode, base_url);
    if (cost > 0.0) g->streak_cost_usd += cost;
}

/* PoP: deterministic_empty @ agent/empty_response_guard.py:_attempts */
/* PoP: deterministic_empty @ agent/empty_response_guard.py:signature */
/* True when the current streak looks deterministic. */
bool deterministic_empty(const empty_guard_t *g) {
    if (!g || !g->enabled) return false;
    if (g->attempts_len < 2) return false;
    const empty_attempt_t *first = &g->attempts[0];
    for (size_t i = 0; i < g->attempts_len; i++) {
        const empty_attempt_t *a = &g->attempts[i];
        if (!a->usage_present) return false;
        if (!a->zero_output) return false;
        if (strcmp(a->model, first->model) != 0) return false;
        if (strcmp(a->provider, first->provider) != 0) return false;
        if (strcmp(a->finish_reason, first->finish_reason) != 0) return false;
    }
    return true;
}

/* PoP: empty_retry_budget @ agent/empty_response_guard.py:_cost_threshold_usd */
/* Empty-retry budget for the current streak (3, or 1 when a single attempt is
 * estimated to cost more than the configured threshold). */
int empty_retry_budget(const empty_guard_t *g,
                        const char *model,
                        const char *provider,
                        const json_t *usage,
                        const char *api_mode,
                        const char *base_url) {
    if (!g || !g->enabled) return EMPTY_GUARD_DEFAULT_RETRY_BUDGET;
    double cost = estimate_attempt_cost(model, provider, usage, api_mode, base_url);
    if (cost < 0.0) return EMPTY_GUARD_DEFAULT_RETRY_BUDGET;
    if (cost >= g->threshold_usd) return EMPTY_GUARD_REDUCED_RETRY_BUDGET;
    return EMPTY_GUARD_DEFAULT_RETRY_BUDGET;
}

double streak_cost_usd(const empty_guard_t *g) {
    if (!g) return -1.0;
    if (g->streak_cost_usd <= 0.0) return -1.0;
    return g->streak_cost_usd;
}
