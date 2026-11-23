#ifndef NOC_W13_SESSIONDM_83038A_H
#define NOC_W13_SESSIONDM_83038A_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_sessiondm_83038a_declared_topic @ gateway/session_dm_threads.py:_declared_topic_thread
 * True when extra.dm_topics declares (chat_id, thread_id) as an operator topic. */
struct json_t *w13_sessiondm_83038a_declared_topic(const struct json_t *extra,
                                                    const struct json_t *chat_id,
                                                    const struct json_t *thread_id);
/* PoP: w13_sessiondm_83038a_key_source @ gateway/session_dm_threads.py:dm_thread_key_source
 * Returns the resulting source's thread_id: null when a shared-mode DM thread is
 * dropped from the session key, else the original thread_id. */
struct json_t *w13_sessiondm_83038a_key_source(const struct json_t *source,
                                                const struct json_t *extra,
                                                const struct json_t *db);
/* PoP: w13_sessiondm_83038a_batch_suffix @ gateway/session_dm_threads.py:dm_thread_batch_suffix
 * ":thread:<id>" when the shared-mode source loses its thread from the key, else "". */
struct json_t *w13_sessiondm_83038a_batch_suffix(const struct json_t *source,
                                                  const struct json_t *extra);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONDM_83038A_H */
