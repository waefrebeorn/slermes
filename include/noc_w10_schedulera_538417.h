#ifndef NOC_W10_SCHEDULERA_538417_H
#define NOC_W10_SCHEDULERA_538417_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_schedulera_538417_pause_identity @ cron/scheduler_authority.py:_pause_identity
 * Input keys: "paused". Returns JSON {"paused_at":..,"reason":..} for a truthy
 * dict, else JSON true. Caller frees. */
struct json_t *w10_schedulera_538417_pause_identity(const struct json_t *in);

/* PoP: w10_schedulera_538417_replayable @ cron/scheduler_authority.py:replayable_completions
 * Input keys: "completions" (array), "existing" (array of journal-backed ids).
 * Returns the JSON array of completion ids whose admission journal exists. Caller frees. */
struct json_t *w10_schedulera_538417_replayable(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_SCHEDULERA_538417_H */
