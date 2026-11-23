#ifndef NOC_W10_RUNPROFILE_B87E2D_H
#define NOC_W10_RUNPROFILE_B87E2D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_runprofile_b87e2d_served_names @ gateway/run_profile_home.py:_served_names
 * Input keys: "served" (array of profile names from _multiplex_profile_homes).
 * Returns the JSON array of unique names, sorted (Python set -> sorted list). Caller frees. */
struct json_t *w10_runprofile_b87e2d_served_names(const struct json_t *in);

/* PoP: w10_runprofile_b87e2d_resolve_source @ gateway/run_profile_home.py:resolve_profile_home_for_source
 * All external dependencies are injected by the fixture: "identity_home",
 * "source_profile", "derived_name", "active_name", "multiplexed", "profiles",
 * "served", "home". Returns {"home": <path>} or {"reject": <profile>}. Caller frees. */
struct json_t *w10_runprofile_b87e2d_resolve_source(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_RUNPROFILE_B87E2D_H */
