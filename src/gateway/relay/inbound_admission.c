/*
 * inbound_admission.c — C home for upstream Hermes gateway/relay/inbound_admission.py.
 * STATUS: BEHAVIOR PORT — value->value admission-decision parity, gate w25_inboundadm_e373aa.
 * PoP: gateway_relay_inbound_admission_pending @ gateway/relay/inbound_admission.py:<module>
 * PoP: w25_inboundadm_e373aa_lane_key @ gateway/relay/inbound_admission.py:admit_relay_delivery
 * PoP: w25_inboundadm_e373aa_admit_action @ gateway/relay/inbound_admission.py:_admit
 * PoP: w25_inboundadm_e373aa_constants @ gateway/relay/adapter.py:_PERMANENT_REFUSALS
 *
 * The transport is the seam: the ACK is sent when admission returns, the lane
 * dictionary only orders one chat's frames, and ``admit_producer`` is another
 * module's coroutine.  What is ported is the decision those values make: which
 * lane a frame belongs to, and which of the five exits admission takes --
 * refuse the parked route to the sender, ACK a permanent refusal, withhold the
 * ACK so the connector redelivers, hand a consumed frame's reply to the
 * deliverer, or commit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_inboundadm_e373aa.h"

/* Native admission refusals a redelivery cannot change (sender, route or payload). */
static const char *const IA_PERMANENT[W25_INBOUNDADM_E373AA_PERMANENT_REFUSALS] = {
    "admission_conflict", "invalid_params", "not_found", "permission_denied", "profile_mismatch",
};

/* Python truthiness of a JSON value (absent and null are both falsy). */
static int ia_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int ia_is_permanent(const char *reason) {
    size_t i;
    if (!reason) return 0;
    for (i = 0; i < W25_INBOUNDADM_E373AA_PERMANENT_REFUSALS; i++)
        if (!strcmp(reason, IA_PERMANENT[i])) return 1;
    return 0;
}

json_t *w25_inboundadm_e373aa_lane_key(const json_t *request) {
    const json_t *source = json_obj_get(request, "source");
    const json_t *platform = source && source->type == JSON_OBJECT ? json_obj_get(source, "platform") : NULL;
    const json_t *value = platform && platform->type == JSON_OBJECT ? json_obj_get(platform, "value") : NULL;
    const json_t *chat_id = source && source->type == JSON_OBJECT ? json_obj_get(source, "chat_id") : NULL;
    json_t *out = json_array();
    /* The lane key carries the platform value AS IS: a missing or non-string
     * platform is None, not "". */
    json_append(out, (value && value->type == JSON_STRING) ? json_copy(value)
                                                          : (value ? json_copy(value) : json_null()));
    json_append(out, chat_id ? json_copy(chat_id) : json_null());
    return out;
}

json_t *w25_inboundadm_e373aa_admit_action(const json_t *request) {
    const json_t *kind_value = json_obj_get(request, "kind");
    const char *kind = kind_value && kind_value->type == JSON_STRING ? kind_value->str_val : "";
    const json_t *reason_value = json_obj_get(request, "reason");
    const char *reason = reason_value && reason_value->type == JSON_STRING ? reason_value->str_val : NULL;

    if (!strcmp(kind, "parked")) {
        /* A parked target is refused to the sender and ACKed: a withheld frame
         * would stall the connector's ordered buffer drain for every profile. */
        return json_string("refuse_parked");
    }
    if (!strcmp(kind, "authority")) {
        /* Permanent refusals return (the transport ACKs); anything else raises
         * so the connector redelivers. */
        return json_string(ia_is_permanent(reason) ? "ack_refusal" : "raise");
    }
    /* admission_producer returned: a consumed frame's reply goes off this reader. */
    {
        const json_t *receipt = json_obj_get(request, "receipt");
        int receipt_none = (!receipt || receipt->type == JSON_NULL);
        if (receipt_none && ia_truthy(json_obj_get(request, "consumer_reply")))
            return json_string("deliver_consumer_reply");
    }
    return json_string("committed");
}

json_t *w25_inboundadm_e373aa_constants(void) {
    json_t *out = json_object();
    json_t *codes = json_array();
    size_t i;
    for (i = 0; i < W25_INBOUNDADM_E373AA_PERMANENT_REFUSALS; i++)
        json_append(codes, json_string(IA_PERMANENT[i]));
    json_set(out, "permanent_refusals", codes);
    return out;
}
