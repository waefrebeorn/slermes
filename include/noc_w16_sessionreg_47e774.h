#ifndef NOC_W16_SESSIONREG_47E774_H
#define NOC_W16_SESSIONREG_47E774_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionreg_47e774_minted_key @ tui_gateway/session_registry.py:_is_server_minted_key */
struct json_t *w16_sessionreg_47e774_minted_key(const struct json_t *value);
/* PoP: w16_sessionreg_47e774_lookup_key @ tui_gateway/session_registry.py:_session_lookup_key */
struct json_t *w16_sessionreg_47e774_lookup_key(const struct json_t *session,
                                                 const char *fallback);
/* PoP: w16_sessionreg_47e774_build_constants @ tui_gateway/session_registry.py:_AGENT_BUILD_WAIT_SLICE/_AGENT_BUILD_SLOW_NOTICE_AFTER/_AGENT_BUILD_SLOW_NOTICE_KEY */
struct json_t *w16_sessionreg_47e774_build_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONREG_47E774_H */
