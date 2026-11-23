#ifndef NOC_W8_RUNIDLEEXI_E437AB_H
#define NOC_W8_RUNIDLEEXI_E437AB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_runidleexi_e437ab_idle_seconds @ gateway/run_idle_exit.py:idle_exit_seconds
 * Explicit seam: `value` is gateway.unmanaged_idle_exit_seconds from the launch config.
 * null/junk -> 600.0; negative -> 0.0. Returns a JSON number. Caller frees. */
struct json_t *w8_runidleexi_e437ab_idle_seconds(const struct json_t *value);

/* PoP: w8_runidleexi_e437ab_client_busy @ gateway/run_idle_exit.py:_client_busy
 * Explicit seams for clients, attaching, last activity and the current monotonic clock.
 * Returns a JSON string or null. Caller frees. */
struct json_t *w8_runidleexi_e437ab_client_busy(const struct json_t *clients,
    const struct json_t *attaching, const struct json_t *last_activity,
    const struct json_t *now, const struct json_t *window);

/* PoP: w8_runidleexi_e437ab_exit_reason @ gateway/run_idle_exit.py:_begin_idle_exit
 * Explicit seam for the exit_reason string built before stop(). `restart` is a string or null.
 * Returns a JSON string. Caller frees. */
struct json_t *w8_runidleexi_e437ab_exit_reason(const struct json_t *window,
    const struct json_t *restart);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_RUNIDLEEXI_E437AB_H */
