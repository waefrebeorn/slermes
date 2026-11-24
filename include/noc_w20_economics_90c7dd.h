/*
 * noc_w20_economics_90c7dd.h -- public C API for the port of
 *   agent/prompt_cache_lifecycle/economics.py (wave 20 batch 9).
 *
 * Every exported symbol is prefixed w20_economics_90c7dd_ so it cannot collide
 * with the rest of the tree. The decision record mirrors the frozen dataclass
 * CacheWarmingDecision field for field.
 */
#ifndef NOC_W20_ECONOMICS_90C7DD_H
#define NOC_W20_ECONOMICS_90C7DD_H

#include <stdint.h>

#include "hermes_json.h"

/* Smallest documented cacheable prefix (Anthropic/OpenAI/Qwen all 1,024). */
#define W20_ECONOMICS_90C7DD_MIN_CACHEABLE_PREFIX_TOKENS 1024

typedef struct {
    double warm_cost;
    double miss_cost;
    double expected_savings;
    int economics_available;   /* Python bool */
    char action[8];            /* "warm" | "stop" */
    int within_miss;           /* Python bool (default True -> 1) */
} w20_economics_90c7dd_decision;

/* float(rate) / 1_000_000, or 0.0 when the rate is None/absent. `rate` may be
 * NULL (missing attribute), JSON_NULL (None) or JSON_NUMBER. */
double w20_economics_90c7dd_per_token(const json_t *rate);

/* One warm-or-stop decision. `pricing` is a PricingEntry-shaped object with
 * input/output/cache_read/cache_write_cost_per_million and optional
 * request_cost. Returns 0 when `out` was left untouched (cannot happen today;
 * reserved for a NULL `out`). */
int w20_economics_90c7dd_evaluate(const json_t *pricing,
                                  int64_t cached_tokens,
                                  int64_t uncached_tokens,
                                  double spent_usd,
                                  double observed_warm_usd,
                                  int64_t output_tokens,
                                  w20_economics_90c7dd_decision *out);

/* CacheWarmingDecision.stop_reason property. */
const char *w20_economics_90c7dd_stop_reason(const w20_economics_90c7dd_decision *d);

/* Wire shape of one decision: costs as shortest-exact decimal strings
 * ("%.17g", the same printf format the oracle applies to Python's float). */
json_t *w20_economics_90c7dd_decision_json(const w20_economics_90c7dd_decision *d);

#endif /* NOC_W20_ECONOMICS_90C7DD_H */
