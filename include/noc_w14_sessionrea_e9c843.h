#ifndef NOC_W14_SESSIONREA_E9C843_H
#define NOC_W14_SESSIONREA_E9C843_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sessionrea_e9c843_session_has_pending_flush @ tui_gateway/session_reaper.py:_session_has_pending_flush */
struct json_t *w14_sessionrea_e9c843_session_has_pending_flush(const struct json_t *session);
/* PoP: w14_sessionrea_e9c843_session_is_lru_evictable @ tui_gateway/session_reaper.py:_session_is_lru_evictable
 * flags carry the injected server-global predicates (see oracle). */
struct json_t *w14_sessionrea_e9c843_session_is_lru_evictable(const struct json_t *session, const struct json_t *flags);
/* PoP: w14_sessionrea_e9c843_session_is_reapable @ tui_gateway/session_reaper.py:_session_is_reapable */
struct json_t *w14_sessionrea_e9c843_session_is_reapable(const struct json_t *session, const struct json_t *flags);
/* PoP: w14_sessionrea_e9c843_session_is_evictable @ tui_gateway/session_reaper.py:_session_is_evictable
 * now/ttl are injected; TTL is server-global _SESSION_TTL_S. */
struct json_t *w14_sessionrea_e9c843_session_is_evictable(const struct json_t *session, const struct json_t *now,
                                        const struct json_t *ttl, const struct json_t *flags);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONREA_E9C843_H */
