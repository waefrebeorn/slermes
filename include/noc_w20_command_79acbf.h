#ifndef NOC_W20_COMMAND_79ACBF_H
#define NOC_W20_COMMAND_79ACBF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/secret_sources/command.py (gate w20_command_79acbf).
 *
 * The module is the user-configured ``secrets.command`` helper source.  The
 * spawn itself (/bin/sh -c, hard timeout, killpg) needs a process table, so
 * the port exposes the pure layers: dotenv unquoting/parsing, the config
 * schema, and fetch()'s decision shaping with the helper's stdout injected as
 * a seam (``NULL`` == "the helper failed to run / returned no value"). */

/* PoP: w20_command_79acbf_unquote_dotenv_value @ agent/secret_sources/command.py:unquote_dotenv_value
 * One layer of matching surrounding quotes after a strip; ``""``/``''``
 * collapse to empty and a lone quote survives.  Caller frees. */
char *w20_command_79acbf_unquote_dotenv_value(const char *raw);

/* PoP: w20_command_79acbf_parse_dotenv_map @ agent/secret_sources/command.py:_parse_dotenv_map
 * KEY=VALUE blob -> {KEY: unquoted VALUE}; comments, blank and non-env-shaped
 * lines are skipped, CRLF is normalised.  Later duplicates win. */
json_t *w20_command_79acbf_parse_dotenv_map(const char *stdout_text);

/* PoP: w20_command_79acbf_fetch_plan @ agent/secret_sources/command.py:CommandSource.fetch
 * Shape of one fetch with the subprocess replaced by ``helper_stdout``:
 *   {"error": str|null, "error_kind": str|null, "warnings": [str],
 *    "secrets": {..}, "timeout": number}
 * ``cfg`` that is not a dict reads as {}, ``is_windows`` mirrors
 * ``_is_windows()``, and ``helper_stdout`` of NULL is a helper failure. */
json_t *w20_command_79acbf_fetch_plan(const json_t *cfg, int is_windows,
                                      const json_t *helper_stdout);

/* PoP: w20_command_79acbf_config_schema @ agent/secret_sources/command.py:CommandSource.config_schema */
json_t *w20_command_79acbf_config_schema(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_COMMAND_79ACBF_H */
