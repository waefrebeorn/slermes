#ifndef NOC_W14_TERMINALTO_FF2244_H
#define NOC_W14_TERMINALTO_FF2244_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes tools/terminal_tool_result.py (gate w14_terminalto_ff2244).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w14_terminalto_ff2244_interpret_signal_exit @ tools/terminal_tool_result.py:_interpret_signal_exit
 * exit_code is a JSON number (int64 decimal string also accepted). */
struct json_t *w14_terminalto_ff2244_interpret_signal_exit(const struct json_t *exit_code);

/* PoP: w14_terminalto_ff2244_interpret_exit_code @ tools/terminal_tool_result.py:_interpret_exit_code */
struct json_t *w14_terminalto_ff2244_interpret_exit_code(const struct json_t *command,
                                                        const struct json_t *exit_code);

/* PoP: w14_terminalto_ff2244_cwd_fallback_note @ tools/terminal_tool_result.py:_cwd_fallback_note */
struct json_t *w14_terminalto_ff2244_cwd_fallback_note(const struct json_t *fallback);

/* PoP: w14_terminalto_ff2244_env_recreated_note @ tools/terminal_tool_result.py:_ENV_RECREATED_NOTE */
struct json_t *w14_terminalto_ff2244_env_recreated_note(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TERMINALTO_FF2244_H */
