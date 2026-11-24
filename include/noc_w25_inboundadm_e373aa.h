/*
 * noc_w25_inboundadm_e373aa.h - C surface for upstream
 * gateway/relay/inbound_admission.py (gate w25_inboundadm_e373aa).
 *
 * Admission of a connector-delivered relay frame.  The transport's ACK, the
 * per-chat lane dictionary, the background-task set and ``admit_producer`` are
 * seams; what is ported is what the values decide: the lane key a frame is
 * ordered under, and what admission does with the producer's outcome -- refuse
 * a parked route to the sender, ACK a permanent refusal, withhold the ACK on a
 * transient one, hand a consumed frame's reply to the deliverer, or commit.
 */
#ifndef NOC_W25_INBOUNDADM_E373AA_H
#define NOC_W25_INBOUNDADM_E373AA_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of native refusals a redelivery cannot change. */
#define W25_INBOUNDADM_E373AA_PERMANENT_REFUSALS 5

/* lane_key(event): request {"source": {"platform": {"value": <str>},
 *                                      "chat_id": <any>}} -> [value, chat_id]. */
json_t *w25_inboundadm_e373aa_lane_key(const json_t *request);

/* admit_action(event, outcome):
 * request: {"kind": "ok" | "parked" | "authority", "reason": <str|null>,
 *           "receipt": <any>, "consumer_reply": <any>}
 * -> "refuse_parked" | "ack_refusal" | "raise" | "deliver_consumer_reply"
 *    | "committed". */
json_t *w25_inboundadm_e373aa_admit_action(const json_t *request);

/* The native refusal codes a redelivery cannot change, sorted. */
json_t *w25_inboundadm_e373aa_constants(void);

#ifdef __cplusplus
}
#endif
#endif
