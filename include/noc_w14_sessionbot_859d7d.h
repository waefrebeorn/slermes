#ifndef NOC_W14_SESSIONBOT_859D7D_H
#define NOC_W14_SESSIONBOT_859D7D_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_sessionbot_859d7d_owner_stopped @ gateway/session_bot_outcome.py:owner_stopped
 * True when result.interrupt_message is the gateway restart/shutdown reason. */
struct json_t *w14_sessionbot_859d7d_owner_stopped(const struct json_t *result);
/* PoP: w14_sessionbot_859d7d_reply_attempt @ gateway/session_bot_outcome.py:_reply_attempt
 * Body of a message_agent call back to sender, or null (JSON string of arguments). */
struct json_t *w14_sessionbot_859d7d_reply_attempt(const struct json_t *call, const struct json_t *sender);
/* PoP: w14_sessionbot_859d7d_last_words @ gateway/session_bot_outcome.py:_last_words
 * Last visible assistant words this turn (tool-call reply first, else content), or null. */
struct json_t *w14_sessionbot_859d7d_last_words(const struct json_t *messages, const struct json_t *sender);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONBOT_859D7D_H */
