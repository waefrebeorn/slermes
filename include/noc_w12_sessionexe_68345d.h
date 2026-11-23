#ifndef NOC_W12_SESSIONEXE_68345D_H
#define NOC_W12_SESSIONEXE_68345D_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w12_sessionexe_68345d_skip_context_files @ gateway/session_execution.py:GatewaySessionAgentMixin._skip_context_files
 * policy.ignore_rules when a policy exists; else gateway.platforms.<key>.skip_context_files. */
struct json_t *w12_sessionexe_68345d_skip_context_files(const struct json_t *policy,
                                                         const struct json_t *platforms_cfg,
                                                         const struct json_t *platform_key);
/* PoP: w12_sessionexe_68345d_current_message_count @ gateway/session_execution.py:GatewaySessionAgentMixin._current_message_count
 * DB message_count or null; has_store/session_id/row are fixture seams. */
struct json_t *w12_sessionexe_68345d_current_message_count(const struct json_t *has_store,
                                                            const struct json_t *session_id,
                                                            const struct json_t *row);
/* PoP: w12_sessionexe_68345d_cached_sid_is_dead @ gateway/session_execution.py:GatewaySessionAgentMixin._cached_sid_is_dead
 * (peeked cached session_id, is_dead) from a cache tuple; dead_check is a fixture seam.
 * Returns {"peek_sid":<str|null>, "dead":<bool>}. */
struct json_t *w12_sessionexe_68345d_cached_sid_is_dead(const struct json_t *has_lock,
                                                         const struct json_t *cache_entry,
                                                         const struct json_t *session_key,
                                                         const struct json_t *session_id,
                                                         const struct json_t *dead_check);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_SESSIONEXE_68345D_H */
