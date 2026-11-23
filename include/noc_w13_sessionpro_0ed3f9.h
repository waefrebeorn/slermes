#ifndef NOC_W13_SESSIONPRO_0ED3F9_H
#define NOC_W13_SESSIONPRO_0ED3F9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_sessionpro_0ed3f9_sanitize @ gateway/session_prompt_pin.py:sanitize_prompt_pin
 * Validates the durable prompt-pin snapshot; returns the canonical six-field
 * object or JSON null when refused. */
struct json_t *w13_sessionpro_0ed3f9_sanitize(const struct json_t *pin);

/* PoP: w13_sessionpro_0ed3f9_get_pin @ gateway/session_prompt_pin.py:SessionPromptPinMixin.get_prompt_pin
 * Returns the entry's prompt pin only while the route still owns the caller's
 * session (expected_session_id null skips the guard); else JSON null. */
struct json_t *w13_sessionpro_0ed3f9_get_pin(const struct json_t *entry,
                                              const struct json_t *expected_session_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONPRO_0ED3F9_H */
