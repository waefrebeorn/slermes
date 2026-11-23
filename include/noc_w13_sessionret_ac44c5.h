#ifndef NOC_W13_SESSIONRET_AC44C5_H
#define NOC_W13_SESSIONRET_AC44C5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_sessionret_ac44c5_reviewing @ gateway/session_retirement.py:_reviewing
 * ctx fields: review_run (bool), request_done (bool). Returns whether a cold
 * /refine review is still running on this live session. */
struct json_t *w13_sessionret_ac44c5_reviewing(const struct json_t *ctx);

/* PoP: w13_sessionret_ac44c5_pinned @ gateway/session_retirement.py:_pinned
 * ctx fields: session_id, mapped, source (null|"local"|"api_server"),
 * subscribers, observers, rescan, settling, detached, detaching,
 * history_mount, mutation_locked, claim_locked, task
 * ("none"|"done"|"running"|"current"), in_executing, in_pending_closes,
 * in_orphan_closes, review_run, request_done. Returns the in-memory pin
 * decision as a JSON bool. */
struct json_t *w13_sessionret_ac44c5_pinned(const struct json_t *ctx);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONRET_AC44C5_H */
