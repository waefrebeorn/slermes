#ifndef NOC_W14_HOSTEDROOM_3724B3_H
#define NOC_W14_HOSTEDROOM_3724B3_H

#include "hermes_json.h"

/* C port of gateway/hosted_room_context.py (gate w14_hostedroom_3724b3).
 *
 * The two actor-id prefixes are module constants in the live module;
 * scheduled_post_kind is the pure, side-effect-free classifier used to tell a
 * scheduler-authored message.user actor from a human one. The caller passes
 * the actor Mapping as a json_t* (borrowed; never freed by the callee).
 *
 * Returns a new json_t*: json_string("post"), json_string("notice"), or
 * json_null() for a human / malformed actor.
 */
json_t *w14_hostedroom_3724b3_scheduled_post_kind(json_t *actor);

#endif /* NOC_W14_HOSTEDROOM_3724B3_H */
