#ifndef NOC_W8_MCPTOOLCON_809435_H
#define NOC_W8_MCPTOOLCON_809435_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_mcptoolcon_809435_pathext_suffixes @ tools/mcp_tool_config.py:_pathext_suffixes
 * Executable suffixes in order. `windows` is an explicit seam (0 False, 1 True);
 * env PATHEXT (case-insensitive key) wins, then the OS default list.
 * Returns a JSON array of strings (caller frees). */
struct json_t *w8_mcptoolcon_809435_pathext_suffixes(const struct json_t *env, int windows);

/* PoP: w8_mcptoolcon_809435_warn_hidden_whitespace @ tools/mcp_tool_config.py:_warn_hidden_whitespace
 * Dotted paths of string values with leading/trailing whitespace. The warn-once
 * state only affects logging, not the returned list. */
struct json_t *w8_mcptoolcon_809435_warn_hidden_whitespace(const char *server_name,
                                                           const struct json_t *config);

/* PoP: w8_mcptoolcon_809435_require_rendered_remote @ tools/mcp_tool_config.py:_require_rendered_remote
 * {"ok": <config>} when no literal ${VAR} remains in url/headers, else
 * {"error": <message>}. Caller frees. */
struct json_t *w8_mcptoolcon_809435_require_rendered_remote(const char *server_name,
                                                            const struct json_t *config);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_MCPTOOLCON_809435_H */
