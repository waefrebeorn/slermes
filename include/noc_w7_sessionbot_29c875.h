#ifndef NOC_W7_SESSIONBOT_29C875_H
#define NOC_W7_SESSIONBOT_29C875_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_sessionbot_29c875_canonical_record_error @ gateway/session_bot.py:_canonical_record_error
 * required is a JSON array of strings, or NULL for the upstream default.
 * Returns a JSON string with the failure reason, or JSON null when valid. */
struct json_t *w7_sessionbot_29c875_canonical_record_error(const char *stem,
        const struct json_t *record, const struct json_t *required);

/* PoP: w7_sessionbot_29c875_record_shape_error @ gateway/session_bot.py:_record_shape_error
 * 'owner' records delegate to _ticket_shape_error (stubbed to a fixed marker). */
struct json_t *w7_sessionbot_29c875_record_shape_error(const char *stem, const struct json_t *record);

/* PoP: w7_sessionbot_29c875_is_pre_admission_pin @ gateway/session_bot.py:_is_pre_admission_pin */
int w7_sessionbot_29c875_is_pre_admission_pin(const char *stem, const struct json_t *record);

/* PoP: w7_sessionbot_29c875_retry_identity @ gateway/session_bot.py:_retry_identity */
struct json_t *w7_sessionbot_29c875_retry_identity(const char *key);

/* PoP: w7_sessionbot_29c875_refused_answer @ gateway/session_bot.py:_refused_answer */
struct json_t *w7_sessionbot_29c875_refused_answer(const struct json_t *record);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SESSIONBOT_29C875_H */
