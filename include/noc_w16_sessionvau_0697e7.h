#ifndef NOC_W16_SESSIONVAU_0697E7_H
#define NOC_W16_SESSIONVAU_0697E7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionvau_0697e7_unlock_params @ gateway/session_vault_prompt.py:unlock_prompt */
struct json_t *w16_sessionvau_0697e7_unlock_params(const struct json_t *backend, const struct json_t *display_name);
/* PoP: w16_sessionvau_0697e7_save_login_params @ gateway/session_vault_prompt.py:save_login */
struct json_t *w16_sessionvau_0697e7_save_login_params(const struct json_t *origin, const struct json_t *site);
/* PoP: w16_sessionvau_0697e7_code_params @ gateway/session_vault_prompt.py:code_prompt */
struct json_t *w16_sessionvau_0697e7_code_params(const struct json_t *site, const struct json_t *hint);
/* PoP: w16_sessionvau_0697e7_save_login @ gateway/session_vault_prompt.py:save_login */
struct json_t *w16_sessionvau_0697e7_save_login(const char *raw);
/* PoP: w16_sessionvau_0697e7_timeouts @ gateway/session_vault_prompt.py:UNLOCK_TIMEOUT_S */
struct json_t *w16_sessionvau_0697e7_timeouts(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONVAU_0697E7_H */
