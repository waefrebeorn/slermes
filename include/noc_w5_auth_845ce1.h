#ifndef NOC_W5_AUTH_845CE1_H
#define NOC_W5_AUTH_845CE1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_auth_845ce1_terminal_setup_auth_method_id
       @ acp_adapter/auth.py:TERMINAL_SETUP_AUTH_METHOD_ID */
const char *w5_auth_845ce1_terminal_setup_auth_method_id(void);

/* PoP: w5_auth_845ce1_detect_provider @ acp_adapter/auth.py:detect_provider
 * Explicit seams: `runtime` stands in for resolve_runtime_provider(),
 * `api_key_callable` for callable(api_key), and `resolve_error` for the
 * resolve_runtime_provider() raising (swallowed -> None). Returns a malloc'd
 * lowercased, stripped provider name, or NULL for Python None. Caller frees. */
char *w5_auth_845ce1_detect_provider_ex(const struct json_t *runtime, int api_key_callable,
                                        int resolve_error);

/* PoP: w5_auth_845ce1_build_auth_methods @ acp_adapter/auth.py:build_auth_methods
 * Explicit seam: `provider` is the detect_provider() result (NULL == None).
 * Returns a NEW json array of method objects. Caller frees.
 * Each agent method is {id,name,description}; the terminal method additionally
 * carries type/args, mirroring the acp.schema constructor kwargs. */
struct json_t *w5_auth_845ce1_build_auth_methods_ex(const char *provider);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W5_AUTH_845CE1_H */
