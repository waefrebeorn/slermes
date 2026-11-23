#ifndef NOC_W7_RUNRUNTIME_BAA593_H
#define NOC_W7_RUNRUNTIME_BAA593_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_runruntime_baa593_profile_parked @ gateway/run_runtime.py:profile_parked
 * bool(name) and name in (descriptor.get('parked_profiles') or {}). */
int w7_runruntime_baa593_profile_parked(const struct json_t *descriptor, const char *name);

/* PoP: w7_runruntime_baa593_parked_profile_map @ gateway/run_runtime.py:parked_profile_map
 * descriptor.setdefault('parked_profiles', {}); returns a copy of that map. */
struct json_t *w7_runruntime_baa593_parked_profile_map(const struct json_t *descriptor);

/* PoP: w7_runruntime_baa593_park_profile @ gateway/run_runtime.py:park_profile
 * parked[name] = str(reason)[:500]; returns the resulting parked map. */
struct json_t *w7_runruntime_baa593_park_profile(const struct json_t *descriptor,
        const char *name, const struct json_t *reason);

/* PoP: w7_runruntime_baa593_unpark_profile @ gateway/run_runtime.py:unpark_profile
 * parked.pop(name, None); returns the resulting parked map. */
struct json_t *w7_runruntime_baa593_unpark_profile(const struct json_t *descriptor, const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_RUNRUNTIME_BAA593_H */
