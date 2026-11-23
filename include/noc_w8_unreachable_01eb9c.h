#ifndef NOC_W8_UNREACHABLE_01EB9C_H
#define NOC_W8_UNREACHABLE_01EB9C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: unreachable_retry_enabled @ cron/unreachable_retry.py:retry_enabled
 *
 * When cfg is NULL the live function loads the profile config; the oracle
 * injects that configuration through the fixture's cfg field. */
struct json_t *unreachable_retry_enabled(struct json_t *cfg);

/* PoP: unreachable_is_retry_fire @ cron/unreachable_retry.py:is_retry_fire */
struct json_t *unreachable_is_retry_fire(struct json_t *job, const char *next_run);

/* PoP: unreachable_clear_state @ cron/unreachable_retry.py:clear_state */
struct json_t *unreachable_clear_state(struct json_t *job);

/* PoP: unreachable_ladder_applies @ cron/unreachable_retry.py:_ladder_applies
 *
 * is_recurring() and retry_enabled() are late imports; the oracle seams them
 * as the fixture's `recurring` flag and `cfg` object. */
struct json_t *unreachable_ladder_applies(struct json_t *job, int recurring,
                                          struct json_t *cfg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_UNREACHABLE_01EB9C_H */
