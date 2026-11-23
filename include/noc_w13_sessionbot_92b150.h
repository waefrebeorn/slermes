#ifndef NOC_W13_SESSIONBOT_92B150_H
#define NOC_W13_SESSIONBOT_92B150_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_sessionbot_92b150_cancel @ gateway/session_bot_cancel.py:_cancel
 * Scripted-seam port of a bot_relay.cancel: validates params/ownership, selects
 * the current admission, and computes the cancellation status.
 * Inputs: params, record, actor_subject, retry_id, row_status, retry_eligible,
 * answer_status, answer_outcome, receipt_status.
 * Returns {"delivery_id","status","receipt_status"} or {"error":<reason>}. */
struct json_t *w13_sessionbot_92b150_cancel(const struct json_t *params,
                                            const struct json_t *record,
                                            const struct json_t *actor_subject,
                                            const struct json_t *retry_id,
                                            const struct json_t *row_status,
                                            const struct json_t *retry_eligible,
                                            const struct json_t *answer_status,
                                            const struct json_t *answer_outcome,
                                            const struct json_t *receipt_status);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONBOT_92B150_H */
