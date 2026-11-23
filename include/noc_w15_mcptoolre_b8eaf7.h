#ifndef NOC_W15_MCPTOOLRE_B8EAF7_H
#define NOC_W15_MCPTOOLRE_B8EAF7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_mcptoolre_b8eaf7_normalize_trust @ tools/mcp_tool_registration.py:_normalize_server_trust */
struct json_t *w15_mcptoolre_b8eaf7_normalize_trust(const struct json_t *value);
/* PoP: w15_mcptoolre_b8eaf7_auth_type @ tools/mcp_tool_registration.py:_auth_type */
struct json_t *w15_mcptoolre_b8eaf7_auth_type(const struct json_t *config);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_MCPTOOLRE_B8EAF7_H */
