#ifndef NOC_PLANLIMITS_H
#define NOC_PLANLIMITS_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: planlimits_model_label @ agent/plan_limits.py:model_label
 * Returns a malloc'd copy of the trimmed catalog name, or of `model`. */
char *planlimits_model_label(const char *model, const struct json_t *pricing);

/* PoP: planlimits_credits_action @ agent/plan_limits.py:credits_action
 * keep_going_false != 0 models Python `keep_going_on_credits is False`.
 * purchased_micros is a decimal string (64-bit safe at the gate). Returns a
 * static string: use_credits | add_credits | add_credits_and_keep_going. */
const char *planlimits_credits_action(int keep_going_false, const char *purchased_micros_decimal);

/* PoP: planlimits_portal_wording @ agent/plan_limits.py:portal_wording
 * Returns a malloc'd string. */
char *planlimits_portal_wording(const char *text);

/* PoP: planlimits_next_steps @ agent/plan_limits.py:limit_next_steps_from_payload
 * Returns a malloc'd string (next_steps_text of the payload). topup_is_null
 * models Python topup_command=None; markup wraps commands in [bold]..[/]. */
char *planlimits_next_steps(const struct json_t *limit, const char *model_command,
                            const char *topup_command, int topup_is_null, int portal, int markup);

#ifdef __cplusplus
}
#endif
#endif /* NOC_PLANLIMITS_H */
