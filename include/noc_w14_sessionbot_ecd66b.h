#ifndef NOC_W14_SESSIONBOT_ECD66B_H
#define NOC_W14_SESSIONBOT_ECD66B_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_sessionbot_ecd66b_write_receipt @ gateway/session_bot_mailbox.py:_write_receipt
 * current is the on-disk receipt (object or null); record the incoming write.
 * expect_present/expect seam the expect sentinel. Returns
 * {result: bool|null, error: str|null, wrote: object|null}. */
struct json_t *w14_sessionbot_ecd66b_write_receipt(const struct json_t *current, const struct json_t *record,
                                 const struct json_t *expect_present, const struct json_t *expect);
/* PoP: w14_sessionbot_ecd66b_withdraw_pin @ gateway/session_bot_mailbox.py:_withdraw_pin
 * True when current is an ambiguous pre-admission pin with no admission_id. */
struct json_t *w14_sessionbot_ecd66b_withdraw_pin(const struct json_t *current);
/* PoP: w14_sessionbot_ecd66b_requeue_pin @ gateway/session_bot_mailbox.py:_requeue_pin
 * The restored queued legacy ticket (pin fields stripped), or null. */
struct json_t *w14_sessionbot_ecd66b_requeue_pin(const struct json_t *current);
/* PoP: w14_sessionbot_ecd66b_refuse_pin @ gateway/session_bot_mailbox.py:_refuse_pin
 * The settled 'failed' receipt, or null when current is not a withdrawable pin. */
struct json_t *w14_sessionbot_ecd66b_refuse_pin(const struct json_t *current);
/* PoP: w14_sessionbot_ecd66b_remember_receipt @ gateway/session_bot_mailbox.py:remember_receipt
 * Update the delivery-index object given home+record; returns the new index. */
struct json_t *w14_sessionbot_ecd66b_remember_receipt(const struct json_t *index, const struct json_t *home,
                                     const struct json_t *record);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONBOT_ECD66B_H */
