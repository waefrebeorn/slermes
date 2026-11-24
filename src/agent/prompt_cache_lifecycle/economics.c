/*
 * economics.c -- real C11 port of the prompt-cache warm-or-stop economics of
 * upstream Hermes agent/prompt_cache_lifecycle/economics.py.
 *
 * PoP: w20_economics_90c7dd_evaluate @ agent/prompt_cache_lifecycle/economics.py:evaluate_economics
 * PoP: w20_economics_90c7dd_stop_reason @ agent/prompt_cache_lifecycle/economics.py:CacheWarmingDecision.stop_reason
 * PoP: w20_economics_90c7dd_per_token @ agent/prompt_cache_lifecycle/economics.py:_per_token
 * PoP: w20_economics_90c7dd_decision_json @ agent/prompt_cache_lifecycle/economics.py:CacheWarmingDecision
 *
 * The module holds no state: a pricing entry in, one decision out. Rates reach
 * the live module as Decimal; float(Decimal) and strtod() both round to the
 * nearest double, so the two pipelines agree bit for bit.
 */
#include "noc_w20_economics_90c7dd.h"

#include <stdio.h>
#include <string.h>

/* json value -> the optional Decimal attribute (None/absent reads as 0.0). */
double w20_economics_90c7dd_per_token(const json_t *rate) {
    if (!rate || rate->type != JSON_NUMBER) return 0.0;
    return rate->num_val / 1000000.0;
}

/* getattr(pricing, name, None): missing, non-object and null all read as None. */
static const json_t *econ_attr(const json_t *pricing, const char *name) {
    if (!pricing || pricing->type != JSON_OBJECT) return NULL;
    return json_obj_get(pricing, name);
}

static double econ_rate(const json_t *pricing, const char *name) {
    return w20_economics_90c7dd_per_token(econ_attr(pricing, name));
}

int w20_economics_90c7dd_evaluate(const json_t *pricing,
                                  int64_t cached_tokens,
                                  int64_t uncached_tokens,
                                  double spent_usd,
                                  double observed_warm_usd,
                                  int64_t output_tokens,
                                  w20_economics_90c7dd_decision *out) {
    double read, write, inp, out_rate, fee;
    double tail_cost, hit_cost, miss_full, miss_cost, warm_cost, expected;
    int available, within_miss;
    const json_t *fee_node;

    if (!out) return 0;

    read = econ_rate(pricing, "cache_read_cost_per_million");
    write = econ_rate(pricing, "cache_write_cost_per_million");
    inp = econ_rate(pricing, "input_cost_per_million");
    out_rate = econ_rate(pricing, "output_cost_per_million");
    /* float(getattr(pricing, 'request_cost', None) or 0) */
    fee_node = econ_attr(pricing, "request_cost");
    fee = (fee_node && fee_node->type == JSON_NUMBER) ? fee_node->num_val : 0.0;

    tail_cost = (double)uncached_tokens * inp;
    hit_cost = (double)cached_tokens * read;
    miss_full = (double)cached_tokens * (write > 0.0 ? write : inp);
    warm_cost = hit_cost + tail_cost + (double)output_tokens * out_rate + fee;
    if (observed_warm_usd > warm_cost) warm_cost = observed_warm_usd; /* max(..., observed) */
    miss_cost = miss_full - hit_cost;
    if (miss_cost < 0.0) miss_cost = 0.0;                             /* max(0.0, ...) */
    available = cached_tokens > 0 && (hit_cost > 0.0 || miss_full > 0.0);
    expected = miss_cost - warm_cost;
    within_miss = (spent_usd + warm_cost <= miss_cost) ? 1 : 0;

    if (cached_tokens < W20_ECONOMICS_90C7DD_MIN_CACHEABLE_PREFIX_TOKENS) available = 0;

    out->warm_cost = warm_cost;
    out->miss_cost = miss_cost;
    out->expected_savings = expected;
    out->economics_available = available ? 1 : 0;
    out->within_miss = within_miss;
    snprintf(out->action, sizeof(out->action), "%s",
             (available && within_miss && expected > 0.0) ? "warm" : "stop");
    return 1;
}

const char *w20_economics_90c7dd_stop_reason(const w20_economics_90c7dd_decision *d) {
    if (!d) return "cache economics unavailable";
    if (!d->economics_available) return "cache economics unavailable";
    if (!d->within_miss) return "refreshes would cost more than the cache write they prevent";
    return "a refresh would cost more than the cache write it prevents";
}

json_t *w20_economics_90c7dd_decision_json(const w20_economics_90c7dd_decision *d) {
    json_t *obj = json_object();
    char buf[64];
    if (!d) return obj;
    snprintf(buf, sizeof(buf), "%.17g", d->warm_cost);
    json_set(obj, "warm_cost", json_string(buf));
    snprintf(buf, sizeof(buf), "%.17g", d->miss_cost);
    json_set(obj, "miss_cost", json_string(buf));
    snprintf(buf, sizeof(buf), "%.17g", d->expected_savings);
    json_set(obj, "expected_savings", json_string(buf));
    json_set(obj, "economics_available", json_bool(d->economics_available ? 1 : 0));
    json_set(obj, "action", json_string(d->action));
    json_set(obj, "within_miss", json_bool(d->within_miss ? 1 : 0));
    json_set(obj, "stop_reason", json_string(w20_economics_90c7dd_stop_reason(d)));
    return obj;
}
