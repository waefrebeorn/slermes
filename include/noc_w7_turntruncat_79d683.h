#ifndef NOC_W7_TURNTRUNCAT_79D683_H
#define NOC_W7_TURNTRUNCAT_79D683_H
#include "hermes_json.h"
#include <stdbool.h>

/* C home for upstream Hermes agent/turn_truncation.py (gate w7_turntruncat_79d683). */

/* PoP: w7_turntruncat_repetition_copy @ agent/turn_truncation.py:repetition_copy
 * Returns the 3-tuple (log line, user copy, error) as a JSON array of strings. */
json_t *w7_turntruncat_repetition_copy(const char *stopping, const char *outcome,
                                       const char *refusal);

/* PoP: w7_turntruncat_partial_result @ agent/turn_truncation.py:partial_result
 * Numeric args are decimal STRINGS (json numbers are doubles). has_error=false means
 * error=None so the live default (`error = final_response`) applies. */
json_t *w7_turntruncat_partial_result(const char *final_response, const char *api_calls_str,
                                      bool has_error, const char *error, bool failed,
                                      bool compression_exhausted);

/* PoP: w7_turntruncat_boosted_output_cap @ agent/turn_truncation.py:boosted_output_cap
 * Mirrors: base = agent.max_tokens or requested_cap or 4096 (unless base given);
 *          anchor = requested_cap or base; limit = model output limit or 0;
 *          if limit and anchor >= limit: return anchor;
 *          boost = min(max(base*2**n, requested_cap or 0), max(32768, anchor*2));
 *          return min(boost, limit) if limit else boost.
 * "has_*" flags distinguish None from 0 as Python truthiness requires. */
long long w7_turntruncat_boosted_output_cap(const char *requested_cap_str, bool has_requested_cap,
                                            const char *n_str, const char *max_tokens_str,
                                            const char *api_mode, bool has_model_limit,
                                            const char *model_limit_str, bool has_base,
                                            const char *base_str);

#endif /* NOC_W7_TURNTRUNCAT_79D683_H */
