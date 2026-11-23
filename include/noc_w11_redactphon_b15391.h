#ifndef NOC_W11_REDPHON_B15391_H
#define NOC_W11_REDPHON_B15391_H

#include <hermes_json.h>

/* C11 port of agent/redact_phone_policy.py.  The config read and the two
 * downstream redactors are injected through the args object, matching the
 * oracle stubs; the policy decision (fails closed, bool-only opt-in) is ported
 * exactly.  Each function takes a borrowed json_t *args and returns a NEW
 * json_t * the caller frees. */
json_t *w11_redactphon_b15391_chat_keeps_phone_numbers(json_t *a);
json_t *w11_redactphon_b15391_redact_for_chat_egress(json_t *a);
json_t *w11_redactphon_b15391_redact_assistant_reply(json_t *a);

#endif /* NOC_W11_REDPHON_B15391_H */
