#ifndef NOC_W17_RUNADAPTER_1B5147_H
#define NOC_W17_RUNADAPTER_1B5147_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_runadapter_1b5147_unavailable_message @ gateway/run_adapters_recovery.py:_adapter_unavailable_message */
struct json_t *w17_runadapter_1b5147_unavailable_message(const char *platform_value, int retrying);

/* PoP: w17_runadapter_1b5147_is_token_contention @ gateway/run_adapters_recovery.py:is_token_contention */
int w17_runadapter_1b5147_is_token_contention(const char *error_code);

/* PoP: w17_runadapter_1b5147_discount_suspended_wait @ gateway/run_adapters_recovery.py:discount_suspended_wait
 * Returns a NEW JSON array of the entries' queued_at values (number or null)
 * after the suspend-overrun shift; `now` is injected for time.monotonic(). */
struct json_t *w17_runadapter_1b5147_discount_suspended_wait(const struct json_t *entries,
                                                   double waited_from, double planned, double now);

/* PoP: w17_runadapter_1b5147_token_contention_reconnect_delay @ gateway/run_adapters_recovery.py:_token_contention_reconnect_delay
 * fixture: status_key, adapter{...}, strikes{...}, now. Returns a fresh
 * {"delay":number,"strikes":object,"fatal":bool,"fatal_message":string|null}. */
struct json_t *w17_runadapter_1b5147_token_contention_reconnect_delay(const struct json_t *fixture);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_RUNADAPTER_1B5147_H */
