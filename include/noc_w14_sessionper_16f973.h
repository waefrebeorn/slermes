#ifndef NOC_W14_SESSIONPER_16F973_H
#define NOC_W14_SESSIONPER_16F973_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sessionper_16f973_chat_labels_meta_key @ gateway/session_persistence.py:_chat_labels_meta_key
 * Mirrors SessionPersistenceMixin._chat_labels_meta_key(platform, scope_id,
 * chat_id): "gateway_chat_labels:{getattr(platform,'value',platform)}:{scope_id or ''}:{chat_id}".
 * The fixture passes plain JSON values, so getattr(platform,'value',platform)
 * is just platform. Returns a malloc'd string (caller frees). */
char *w14_sessionper_16f973_chat_labels_meta_key(const struct json_t *platform,
                                                 const struct json_t *scope_id,
                                                 const struct json_t *chat_id);

/* PoP: w14_sessionper_16f973_is_live_system_guard @ gateway/session_persistence.py:_is_live_system_guard
 * Mirrors _is_live_system_guard(exc): isinstance(exc, RuntimeError) and
 * "live-system guard" in str(exc). exc_type/exc_message encode the exception
 * as fixture-injected facts. Returns 1/0. */
int w14_sessionper_16f973_is_live_system_guard(const char *exc_type,
                                               const char *exc_message);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONPER_16F973_H */
