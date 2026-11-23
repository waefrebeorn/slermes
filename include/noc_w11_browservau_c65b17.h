#ifndef NOC_W11_BROWSERVAU_C65B17_H
#define NOC_W11_BROWSERVAU_C65B17_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_browservau_c65b17_parse_json_result @ tools/browser_vault_tool.py:_parse_json_result */
struct json_t *w11_browservau_c65b17_parse_json_result(const struct json_t *raw);

/* PoP: w11_browservau_c65b17_deep_probe @ tools/browser_vault_tool.py:_deep_probe */
struct json_t *w11_browservau_c65b17_deep_probe(const char *selector);

/* PoP: w11_browservau_c65b17_list @ tools/browser_vault_tool.py:browser_vault_list
 * Backends and prompt availability are injected as fixture data. */
struct json_t *w11_browservau_c65b17_list(const struct json_t *backends, int can_prompt);

/* PoP: w11_browservau_c65b17_unlock @ tools/browser_vault_tool.py:browser_vault_unlock */
struct json_t *w11_browservau_c65b17_unlock(const struct json_t *backends, const char *backend_name,
        int can_prompt, const char *prompt_master);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_BROWSERVAU_C65B17_H */
