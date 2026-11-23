#ifndef NOC_W10_GATEWAYQUE_6A8E6B_H
#define NOC_W10_GATEWAYQUE_6A8E6B_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_gatewayque_6a8e6b_published_queue @ hermes_cli/gateway_queue.py:published_queue
 * Pure over the state JSON {pending,queued_inputs,queued_at,pending_seq}; no RPC.
 * Returns the queued texts in publish order plus this viewer's fresh admissions. */
struct json_t *w10_gatewayque_6a8e6b_published_queue(const struct json_t *state);
/* PoP: w10_gatewayque_6a8e6b_own_head @ hermes_cli/gateway_queue.py:_own_head
 * Returns the admission id string at the head of the owner's queue, or NULL. */
struct json_t *w10_gatewayque_6a8e6b_own_head(const struct json_t *state);
/* PoP: w10_gatewayque_6a8e6b_queued_rows @ hermes_cli/gateway_queue.py:queued_rows
 * receipts maps admission -> {"receipt":{...}} or {"error":true} for a failed prompt.receipt.
 * Returns [[admission_id, text], ...] in run order. */
struct json_t *w10_gatewayque_6a8e6b_queued_rows(const struct json_t *state, const struct json_t *receipts);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_GATEWAYQUE_6A8E6B_H */
