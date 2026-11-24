#ifndef NOC_W16_DELEGATETO_2D04D4_H
#define NOC_W16_DELEGATETO_2D04D4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_delegateto_2d04d4_resolve_wake @ tools/delegate_tool_dispatch.py:_resolve_async_wake_sid */
struct json_t *w16_delegateto_2d04d4_resolve_wake(const struct json_t *opts);
/* PoP: w16_delegateto_2d04d4_batch_progress @ tools/delegate_tool_dispatch.py:_batch_progress_token */
struct json_t *w16_delegateto_2d04d4_batch_progress(const struct json_t *child_agents);
/* PoP: w16_delegateto_2d04d4_sync_note @ tools/delegate_tool_dispatch.py:_SYNC_FALLBACK_NOTES */
struct json_t *w16_delegateto_2d04d4_sync_note(const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_DELEGATETO_2D04D4_H */
