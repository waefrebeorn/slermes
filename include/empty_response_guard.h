/*
 * empty_response_guard.h — Deterministic-empty detection + cost-aware retry
 * budgets (NS-503). Faithful port of agent/empty_response_guard.py.
 *
 * Two independent guards, both failing OPEN to legacy behaviour:
 *   1. deterministic_empty  — two consecutive empties (usage present,
 *      output_tokens==0, same model/provider/finish_reason) => skip remaining
 *      retries, walk the fallback chain.
 *   2. empty_retry_budget   — when one empty attempt's cost exceeds the
 *      threshold (default $0.25), the budget drops 3 -> 1.
 *
 * State is scoped to one consecutive empty streak and lives in empty_guard_t,
 * which the caller owns (attach a pointer on the agent struct). The streak is
 * cleared whenever retries==0 at record time, transparently honouring every
 * existing reset site (turn start, compaction, tool success, fallback).
 */

#ifndef EMPTY_RESPONSE_GUARD_H
#define EMPTY_RESPONSE_GUARD_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMPTY_GUARD_DEFAULT_RETRY_BUDGET      3
#define EMPTY_GUARD_REDUCED_RETRY_BUDGET      1
#define EMPTY_GUARD_DEFAULT_COST_THRESHOLD    0.25  /* USD per attempt */
#define EMPTY_GUARD_DEFAULT_ENABLED           true

/* One observed empty completion within the current streak. */
typedef struct {
    char  model[128];
    char  provider[64];
    char  finish_reason[64];
    bool  usage_present;
    bool  zero_output;
} empty_attempt_t;

/* Mutable guard state, owned by the caller (e.g. an agent struct field). */
typedef struct {
    bool   enabled;
    double threshold_usd;       /* per-attempt cost that halves the budget */
    empty_attempt_t *attempts;  /* dynamic ring of attempts in the streak */
    size_t attempts_len;
    size_t attempts_cap;
    double streak_cost_usd;     /* accumulated estimated cost of the streak */
    /* identity used for cost estimation */
    char   model[128];
    char   provider[64];
    char   base_url[256];
    char   api_mode[32];
} empty_guard_t;

/* Initialize a guard with schema defaults (enabled, $0.25). */
void empty_guard_init(empty_guard_t *g);

/* Free dynamic buffers owned by the guard (call at agent teardown). */
void empty_guard_free(empty_guard_t *g);

/* Resolve the agent.empty_response_guard config section into (enabled,
 * threshold). Tolerant of malformed input (falls back to schema defaults).
 * Returns true on success. */
bool resolve_guard_settings(const json_t *section, bool *out_enabled, double *out_threshold);

/* Whether the guard is enabled (config-resolved; default true). */
bool guard_enabled(const empty_guard_t *g);

/* Record one empty completion in the current streak.
 *   retry_count  — the agent's _empty_content_retries BEFORE incrementing for
 *                  this attempt; a value of 0 starts a new streak (clears
 *                  prior history).
 *   model/provider/finish_reason/usage/api_mode/base_url — the completion.
 * usage may be NULL (unknown usage => fails open, never classified empty). */
void record_empty_attempt(empty_guard_t *g,
                           size_t retry_count,
                           const char *model,
                           const char *provider,
                           const char *finish_reason,
                           const json_t *usage,
                           const char *api_mode,
                           const char *base_url);

/* True when the current streak looks deterministic (>=2 consecutive attempts,
 * all usage-present, zero output, identical signature). Fails open when the
 * guard is disabled. */
bool deterministic_empty(const empty_guard_t *g);

/* Empty-retry budget for the current streak: 3, or 1 when a single attempt is
 * estimated to cost >= the configured threshold. 3 when disabled or unknown
 * cost. */
int empty_retry_budget(const empty_guard_t *g,
                        const char *model,
                        const char *provider,
                        const json_t *usage,
                        const char *api_mode,
                        const char *base_url);

/* Accumulated estimated cost of the current empty streak, or -1.0 when none
 * is known (never accumulated a positive estimate). */
double streak_cost_usd(const empty_guard_t *g);

#ifdef __cplusplus
}
#endif

#endif /* EMPTY_RESPONSE_GUARD_H */
