#ifndef NOC_W8_BROWSERSESS_ACACF0_H
#define NOC_W8_BROWSERSESS_ACACF0_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_browsersess_acacf0_is_local_sidecar_key @ tools/browser_session.py:is_local_sidecar_key */
int w8_browsersess_acacf0_is_local_sidecar_key(const char *session_key);

/* PoP: w8_browsersess_acacf0_owner_task_id @ tools/browser_session.py:owner_task_id
 * Returns a malloc'd string; caller frees. */
char *w8_browsersess_acacf0_owner_task_id(const char *session_key);

/* PoP: w8_browsersess_acacf0_session_record_ex @ tools/browser_session.py:_session_record
 * hex10 is the injected uuid4().hex[:10] seam. Caller frees the result. */
struct json_t *w8_browsersess_acacf0_session_record_ex(const char *prefix, const struct json_t *cdp_url,
                                             const struct json_t *features, const char *hex10);

/* PoP: w8_browsersess_acacf0_publish @ tools/browser_session.py:_publish
 * setdefault of session_key / owner_task_id / home_key. Caller frees. */
struct json_t *w8_browsersess_acacf0_publish(const char *cache_key, const struct json_t *session_info,
                                   const char *home_key);

/* PoP: w8_browsersess_acacf0_shares_bot_desktop_ex @ tools/browser_session.py:shares_bot_desktop_browser
 * `display` (string or null) and `human_holds` are injected environment seams. */
int w8_browsersess_acacf0_shares_bot_desktop_ex(const struct json_t *session_info, const char *display,
                                      int human_holds);

/* PoP: w8_browsersess_acacf0_human_holds_shared_ex @ tools/browser_session.py:human_holds_shared_browser */
int w8_browsersess_acacf0_human_holds_shared_ex(const struct json_t *session_info, const char *display,
                                      int human_holds);

/* PoP: w8_browsersess_acacf0_constants @ tools/browser_session.py:LOCAL_SUFFIX/BROWSER_ORPHAN_REAP_INTERVAL/... */
struct json_t *w8_browsersess_acacf0_constants(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_BROWSERSESS_ACACF0_H */