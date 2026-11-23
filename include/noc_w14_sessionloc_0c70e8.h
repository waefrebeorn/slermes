#ifndef NOC_W14_SESSIONLOC_0C70E8_H
#define NOC_W14_SESSIONLOC_0C70E8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sessionloc_0c70e8_validate_title @ gateway/session_local_title.py:validate_title
 *
 * Faithful port of validate_title() together with the sanitize_title() core it
 * calls (persistence.conversation.sessions.sanitize_title). Returns a JSON
 * object: {"ok":true,"title":<cleaned>} or {"ok":false,"reason":"invalid_params"}.
 * Caller frees. Authority/store resolution is intentionally out of scope. */
struct json_t *w14_sessionloc_0c70e8_validate_title(const struct json_t *title);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONLOC_0C70E8_H */
