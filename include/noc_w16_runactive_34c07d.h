#ifndef NOC_W16_RUNACTIVE_34C07D_H
#define NOC_W16_RUNACTIVE_34C07D_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w16_runactive_34c07d_profile_units @ gateway/run_active_work_verb.py:_profile_units */
/* Pure slice of _profile_units: helper results (managed units, side count, cron count, pid)
 * are injected from the fixture, while the chat-namespace filtering and assembly are ported. */
struct json_t *w16_runactive_34c07d_profile_units(const struct json_t *units, const struct json_t *profile,
                                                 const struct json_t *home, const struct json_t *authority,
                                                 const struct json_t *pid);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_RUNACTIVE_34C07D_H */
