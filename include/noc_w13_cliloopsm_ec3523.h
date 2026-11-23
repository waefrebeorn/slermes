#ifndef NOC_W13_CLILOOPSM_EC3523_H
#define NOC_W13_CLILOOPSM_EC3523_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_cliloopsm_ec3523_preview @ hermes_cli/cli_loops_mixin.py:_preview
 * f"{payload[:80]}{'...' if len(payload) > 80 else ''}" */
struct json_t *w13_cliloopsm_ec3523_preview(const struct json_t *payload);

/* PoP: w13_cliloopsm_ec3523_parse_queue_index @ hermes_cli/cli_loops_mixin.py:_parse_queue_index
 * 1-based queue index for an all-ASCII-digit token (leading zeros allowed), else NULL.
 * Tokens longer than Python's 4300-digit int() conversion limit return NULL. */
struct json_t *w13_cliloopsm_ec3523_parse_queue_index(const struct json_t *token);

/* PoP: w13_cliloopsm_ec3523_last_assistant_response_text @ hermes_cli/cli_loops_mixin.py:CLILoopsMixin._last_assistant_response_text
 * First assistant message in reversed history; list content is flattened to "\n"-joined
 * text/output_text parts. */
struct json_t *w13_cliloopsm_ec3523_last_assistant_response_text(const struct json_t *history);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_CLILOOPSM_EC3523_H */
