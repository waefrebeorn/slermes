#ifndef NOC_W8_MCPTOOLCOM_F3C09B_H
#define NOC_W8_MCPTOOLCOM_F3C09B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_mcptoolcom_f3c09b_env_ref_name @ tools/mcp_tool_common.py:_env_ref_name
 * Bare env-var name from a ${...} body; strips a Cursor-style "env:" prefix.
 * Returns a malloc'd string; caller frees. */
char *w8_mcptoolcom_f3c09b_env_ref_name(const char *ref);

/* PoP: w8_mcptoolcom_f3c09b_safe_numeric @ tools/mcp_tool_common.py:_safe_numeric
 * Coerce `value` (JSON string/number/bool) to a number, clamped to `minimum`.
 * coerce_int != 0 -> int() semantics, else float() semantics. Returns a JSON
 * number (caller frees); `default` on failure / non-finite. */
struct json_t *w8_mcptoolcom_f3c09b_safe_numeric(const struct json_t *value, double def,
                               int coerce_int, double minimum);

/* PoP: w8_mcptoolcom_f3c09b_parse_boolish @ tools/mcp_tool_common.py:_parse_boolish
 * Bool-like config value with fallback. JSON null/other -> `def`. */
int w8_mcptoolcom_f3c09b_parse_boolish(const struct json_t *value, int def);

/* PoP: w8_mcptoolcom_f3c09b_mcp_server_enabled @ tools/mcp_tool_common.py:mcp_server_enabled
 * The ONE reader of enabled/disabled. */
int w8_mcptoolcom_f3c09b_mcp_server_enabled(const struct json_t *cfg);

/* PoP: w8_mcptoolcom_f3c09b_get_lifecycle_seconds @ tools/mcp_tool_common.py:_get_lifecycle_seconds
 * Optional positive lifecycle timeout (top-level then nested `lifecycle`).
 * Returns a JSON number or JSON null (caller frees). */
struct json_t *w8_mcptoolcom_f3c09b_get_lifecycle_seconds(const struct json_t *cfg, const char *key);

/* PoP: w8_mcptoolcom_f3c09b_prepend_path @ tools/mcp_tool_common.py:_prepend_path
 * Copy of `env` with `directory` prepended to PATH (os.pathsep=';' seam).
 * Returns a JSON object (caller frees). */
struct json_t *w8_mcptoolcom_f3c09b_prepend_path(const struct json_t *env, const char *directory);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_MCPTOOLCOM_F3C09B_H */
