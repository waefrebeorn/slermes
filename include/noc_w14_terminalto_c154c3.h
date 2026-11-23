#ifndef NOC_W14_TERMINALTO_C154C3_H
#define NOC_W14_TERMINALTO_C154C3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes tools/terminal_tool_guards.py (gate w14_terminalto_c154c3).
 * Every exported symbol is prefixed with the gate name. All results are freshly
 * allocated json_t values that the caller must free with json_free(). */

/* PoP: w14_terminalto_c154c3_validate_workdir @ tools/terminal_tool_guards.py:_validate_workdir */
struct json_t *w14_terminalto_c154c3_validate_workdir(const struct json_t *workdir);

/* PoP: w14_terminalto_c154c3_safe_command_preview @ tools/terminal_tool_guards.py:_safe_command_preview
 * Only str/None inputs are exposed; repr() of non-str values is not modelled. */
struct json_t *w14_terminalto_c154c3_safe_command_preview(const struct json_t *command, int limit);

/* PoP: w14_terminalto_c154c3_blocked_json @ tools/terminal_tool_guards.py:_blocked_json
 * Returns the exact json.dumps(..., ensure_ascii=False) envelope string. */
struct json_t *w14_terminalto_c154c3_blocked_json(const struct json_t *error, const struct json_t *status);

/* PoP: w14_terminalto_c154c3_looks_like_help_or_version_command @ tools/terminal_tool_guards.py:_looks_like_help_or_version_command */
struct json_t *w14_terminalto_c154c3_looks_like_help_or_version_command(const struct json_t *command);

/* PoP: w14_terminalto_c154c3_strip_quotes @ tools/terminal_tool_guards.py:_strip_quotes
 * strip_inert_heredoc_bodies() is stubbed to identity in the oracle (fixtures
 * avoid inert heredoc bodies), so this port does the three quote-mask passes. */
struct json_t *w14_terminalto_c154c3_strip_quotes(const struct json_t *command);

/* PoP: w14_terminalto_c154c3_strip_trailing_amp @ tools/terminal_tool_guards.py:_strip_trailing_amp */
struct json_t *w14_terminalto_c154c3_strip_trailing_amp(const struct json_t *command);

/* PoP: w14_terminalto_c154c3_has_flag @ tools/terminal_tool_guards.py:_has_flag
 * `short` is a string of flag letters; `long` is a JSON array of long option names. */
struct json_t *w14_terminalto_c154c3_has_flag(const struct json_t *args, const char *short_flags,
                                              const struct json_t *long_flags);

/* PoP: w14_terminalto_c154c3_is_data_command @ tools/terminal_tool_guards.py:_is_data_command */
struct json_t *w14_terminalto_c154c3_is_data_command(const struct json_t *word, const struct json_t *args);

/* PoP: w14_terminalto_c154c3_returns_promptly @ tools/terminal_tool_guards.py:_returns_promptly */
struct json_t *w14_terminalto_c154c3_returns_promptly(const struct json_t *word, const struct json_t *args);

/* PoP: w14_terminalto_c154c3_command_word @ tools/terminal_tool_guards.py:_command_word
 * Returns [word, args] (the live tuple is converted to a list in the oracle). */
struct json_t *w14_terminalto_c154c3_command_word(const struct json_t *tokens);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TERMINALTO_C154C3_H */
