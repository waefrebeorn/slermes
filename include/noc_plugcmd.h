#ifndef NOC_PLUGCMD_H
#define NOC_PLUGCMD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: plugcmd_invoke_plugin_command @ hermes_cli/plugin_command_dispatch.py:invoke_plugin_command
 *
 * `params` is the handler's inspect.Parameter list, each
 *   {"name": <str>, "kind": "POSITIONAL_ONLY"|"POSITIONAL_OR_KEYWORD"|
 *                          "VAR_POSITIONAL"|"KEYWORD_ONLY"|"VAR_KEYWORD"|"NOSIG"}
 * ("NOSIG" means signature() raises, so the legacy one-arg call is used).
 * `context` is the **context dict. Returns a record of the resulting call:
 *   {"raw_args": <value>, "kwargs": {...}, "legacy": <bool>}
 */
struct json_t *plugcmd_invoke(const struct json_t *params,
                              const struct json_t *raw_args,
                              const struct json_t *context);

#ifdef __cplusplus
}
#endif

#endif /* NOC_PLUGCMD_H */
