#ifndef NOC_W8_MCPTOOLERR_C679C5_H
#define NOC_W8_MCPTOOLERR_C679C5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_mcptoolerr_c679c5_resolve_identity_header @ tools/mcp_tool_errors.py:_resolve_identity_header
 * Static identity_header as a JSON [name, value] array (caller frees), or null
 * when absent/invalid. The profile value_from path needs a live profile seam
 * and is not exercised by this pure gate. */
struct json_t *w8_mcptoolerr_c679c5_resolve_identity_header(const char *server_name,
                                                            const struct json_t *config);

/* PoP: w8_mcptoolerr_c679c5_apply_identity_header @ tools/mcp_tool_errors.py:_apply_identity_header
 * Merge the resolved identity header into a copy of `headers`; an explicit
 * entry of the same name (any casing) wins. Returns an object (caller frees). */
struct json_t *w8_mcptoolerr_c679c5_apply_identity_header(const char *server_name,
        const struct json_t *config, const struct json_t *headers);

/* PoP: w8_mcptoolerr_c679c5_validate_remote_mcp_url @ tools/mcp_tool_errors.py:_validate_remote_mcp_url
 * {"ok": <stripped url>} for a valid http(s) URL, else {"error": <message>}.
 * Redaction is a no-op for the plain (credential-free) inputs here. Caller frees. */
struct json_t *w8_mcptoolerr_c679c5_validate_remote_mcp_url(const char *server_name,
        const struct json_t *url);

/* PoP: w8_mcptoolerr_c679c5_shown_url @ tools/mcp_tool_errors.py:_shown_url
 * URL with credential-named query values and URL userinfo masked with ***
 * (the redact_sensitive_text(url, redact_url_credentials=True) seam). Caller frees. */
char *w8_mcptoolerr_c679c5_shown_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_MCPTOOLERR_C679C5_H */
