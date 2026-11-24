#ifndef NOC_W16_RUNSTARTUP_6B6BFB_H
#define NOC_W16_RUNSTARTUP_6B6BFB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w16_runstartup_6b6bfb_liveness_plan @ gateway/run_startup_liveness.py:start_loop_liveness_guards */
/* Pure plan slice of the startup guard: given config + existing handles, decide whether the
 * guard is enabled, whether the floor timer and watchdog must be armed, and the clamped-free
 * parameters Python computes via float()/int(getattr(...)). Timer/watchdog construction is a seam. */
struct json_t *w16_runstartup_6b6bfb_liveness_plan(const struct json_t *config,
                                                  const struct json_t *floor_handle,
                                                  const struct json_t *watchdog);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_RUNSTARTUP_6B6BFB_H */
