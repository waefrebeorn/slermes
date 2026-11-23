/*
 * review_signals.c — C11 port of the pure cadence predicates in upstream
 * Hermes agent/review_signals.py.
 *
 * PoP: w7_reviewsigna_86e0a4_skill_review_due @ agent/review_signals.py:skill_review_due
 * PoP: w7_reviewsigna_86e0a4_background_review_enabled @ agent/review_signals.py:background_review_enabled
 * PoP: w7_reviewsigna_86e0a4_build_review_signals @ agent/review_signals.py:build_review_signals
 */
#include "noc_w7_reviewsigna_86e0a4.h"
#include <stdlib.h>

static long long to_i64(const char *s) {
    if (!s || !*s) return 0;
    return strtoll(s, NULL, 10);
}

bool w7_reviewsigna_86e0a4_skill_review_due(const char *iters, const char *interval,
                                            bool has_skill_manage) {
    long long iv = to_i64(interval);
    return iv > 0 && to_i64(iters) >= iv && has_skill_manage;
}

bool w7_reviewsigna_86e0a4_background_review_enabled(bool skip, const char *delegate_depth,
                                                     bool configured) {
    if (skip || to_i64(delegate_depth) > 0) return false;
    return configured;
}

json_t *w7_reviewsigna_86e0a4_build_review_signals(const char *iters, const char *turns,
                                                   bool memory_due, bool skill_due,
                                                   bool background_enabled,
                                                   const char *skill_interval,
                                                   const char *memory_interval) {
    json_t *out = json_object();
    json_set(out, "tool_iterations_since_skill_review", json_number((double)to_i64(iters)));
    json_set(out, "user_turns_since_memory_review", json_number((double)to_i64(turns)));
    json_set(out, "skill_review_due", json_bool(skill_due));
    json_set(out, "memory_review_due", json_bool(memory_due));
    json_set(out, "background_review_enabled", json_bool(background_enabled));
    json_set(out, "skill_nudge_interval", json_number((double)to_i64(skill_interval)));
    json_set(out, "memory_nudge_interval", json_number((double)to_i64(memory_interval)));
    return out;
}

