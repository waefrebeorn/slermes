#ifndef NOC_W7_REVIEWSIGNA_86E0A4_H
#define NOC_W7_REVIEWSIGNA_86E0A4_H

#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Integers cross the JSON boundary as decimal strings (libjson numbers are
 * doubles); each is parsed with strtoll. */

/* PoP: w7_reviewsigna_86e0a4_skill_review_due @ agent/review_signals.py:skill_review_due
 * interval > 0 and iters >= interval and "skill_manage" is a valid tool. */
bool w7_reviewsigna_86e0a4_skill_review_due(const char *iters, const char *interval,
                                            bool has_skill_manage);

/* PoP: w7_reviewsigna_86e0a4_background_review_enabled @ agent/review_signals.py:background_review_enabled
 * Seam: `configured` stands in for load_background_review_settings()[0]. */
bool w7_reviewsigna_86e0a4_background_review_enabled(bool skip, const char *delegate_depth,
                                                      bool configured);

/* PoP: w7_reviewsigna_86e0a4_build_review_signals @ agent/review_signals.py:build_review_signals
 * Returns the seven-field ReviewSignals dict. `background_enabled` is an
 * explicit seam for background_review_enabled(agent). Caller frees. */
json_t *w7_reviewsigna_86e0a4_build_review_signals(const char *iters, const char *turns,
                                                   bool memory_due, bool skill_due,
                                                   bool background_enabled,
                                                   const char *skill_interval,
                                                   const char *memory_interval);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_REVIEWSIGNA_86E0A4_H */
