#ifndef NOC_W5_BOTCHAIN_6AA11F_H
#define NOC_W5_BOTCHAIN_6AA11F_H

/*
 * Public C API for the port of agent/bot_chain.py (gate w5_botchain_6aa11f).
 * All exported symbols are botchain_-prefixed to avoid collisions.
 * The stateful admit_send core is driven with resolved state/settings facts
 * from the caller (see botchain_admit); the sha256 ask digest is replaced by
 * the normalized body itself (an injective identity over the ask domain),
 * which preserves the exact duplicate-detection decision of the live code.
 */
#include "hermes_json.h"

/* Port of _hops(author): the envelope's positive int hops, else 0.
 * Returns a json number (caller frees). */
json_t *botchain_hops(const json_t *author);

/* Port of Admission.envelope(): {"hops": hop} when hop > 1, else {}.
 * Returns a json object (caller frees). */
json_t *botchain_envelope(double hop);

/* Port of the carry_chain(old,new) guard: true iff the live function would
 * proceed past its emptiness/equality guard (and move a present chain).
 * Returns a json bool (caller frees). */
json_t *botchain_carry(const char *old_id, const char *new_id);

/*
 * Port of the decision core of admit_send(agent, recipients, body).
 * `facts` supplies the resolved inputs the live function reads from its
 * runtime: enabled, max_hops, bot_replies_expect_reply, author (dict),
 * recipients (array of str), body (str), key (str), hops (int), sent (int),
 * no_reply_from (str|null) and asks (array of [recipient, normalized_body]).
 * Returns the Admission verdict as
 *   {"refusal":str|null, "refused":{recipient:msg},
 *    "notify_only":[recipient] (sorted), "hop":int,
 *    "asks":[[recipient, normalized_body]] (sorted), "prior_sent":int}
 * (caller frees).
 */
json_t *botchain_admit(const json_t *facts);

#endif /* NOC_W5_BOTCHAIN_6AA11F_H */
