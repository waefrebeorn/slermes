#ifndef NOC_W13_CLI_MODALM_3A2DEE_H
#define NOC_W13_CLI_MODALM_3A2DEE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_climodalm_3a2dee_approval_choices @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._approval_choices */
struct json_t *w13_climodalm_3a2dee_approval_choices(const struct json_t *command,
        int allow_permanent, int allow_session, int smart_denied);

/* PoP: w13_climodalm_3a2dee_normalize_choice @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._normalize_slash_confirm_choice
 * choices: array of [value, label, detail] rows; uses row[0] as the allowed value. */
struct json_t *w13_climodalm_3a2dee_normalize_choice(const struct json_t *raw,
        const struct json_t *choices);

/* PoP: w13_climodalm_3a2dee_connection_opening_phase @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._connection_opening_phase */
struct json_t *w13_climodalm_3a2dee_connection_opening_phase(const struct json_t *target);

/* PoP: w13_climodalm_3a2dee_split_destructive_skip @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._split_destructive_skip
 * Returns [remainder, skip_bool]; skip tokens now/--yes/-y (case-insensitive). */
struct json_t *w13_climodalm_3a2dee_split_destructive_skip(const struct json_t *cmd_text);

/* PoP: w13_climodalm_3a2dee_connection_fields @ hermes_cli/cli_modal_mixin.py:CLIModalMixin._connection_fields */
struct json_t *w13_climodalm_3a2dee_connection_fields(const struct json_t *target);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_CLI_MODALM_3A2DEE_H */
