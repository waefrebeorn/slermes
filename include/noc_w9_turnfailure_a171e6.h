#ifndef NOC_W9_TURNFAILURE_A171E6_H
#define NOC_W9_TURNFAILURE_A171E6_H

#include "hermes_json.h"

/* agent/turn_failure_copy.py — user-facing failure copy + verdict stamping. */
json_t *w9_turnfailure_a171e6_stamp_failure(json_t *result, const char *reason, int retryable);
json_t *w9_turnfailure_a171e6_untyped_failed_turn_display_kind(const char *role, json_t *content);
json_t *w9_turnfailure_a171e6_failed_turn_notice(json_t *turn_messages);
json_t *w9_turnfailure_a171e6_exit_reason_failure(const char *reason);
int     w9_turnfailure_a171e6_is_max_iteration_handoff(json_t *result);
json_t *w9_turnfailure_a171e6_failure_cause_gloss(const char *reason,
                                                  const char *subject, const char *possessive);
json_t *w9_turnfailure_a171e6_short_detail(const char *text, int limit);
json_t *w9_turnfailure_a171e6_invalid_response_failure_reason(json_t *error);

#endif /* NOC_W9_TURNFAILURE_A171E6_H */
