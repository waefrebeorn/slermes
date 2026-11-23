/*
 * auxiliary_wire.c — C port of upstream Hermes agent/auxiliary_wire.py.
 * PoP: auxwire_relay_chat_metadata @ agent/auxiliary_wire.py:relay_chat_metadata
 * PoP: auxwire_prepare_chat_messages @ agent/auxiliary_wire.py:prepare_chat_messages
 *
 * relay_chat_metadata is a full faithful port. prepare_chat_messages ports the
 * SDK-type guard and the {**kwargs, "messages": messages} rebuild; the carrier
 * shaping / conversion behind `messages` is a resolved-fact seam (header).
 */
#include "noc_auxwire.h"

#include <string.h>

json_t *auxwire_relay_chat_metadata(const json_t *metadata) {
    json_t *out = metadata && metadata->type == JSON_OBJECT
                ? json_copy(metadata) : json_object();
    /* json_set keeps an existing key's position, or appends - exactly Python
       {**metadata, "api_mode": "chat_completions"}. */
    json_set(out, "api_mode", json_string("chat_completions"));
    return out;
}

json_t *auxwire_prepare_chat_messages(const json_t *kwargs, int client_is_chat_sdk,
        const json_t *shaped_messages) {
    int has_messages = kwargs && json_obj_get(kwargs, "messages") != NULL;
    if (!client_is_chat_sdk || !has_messages) {
        /* live returns kwargs itself (unchanged) on the guard path */
        return kwargs && kwargs->type == JSON_OBJECT ? json_copy(kwargs) : json_object();
    }
    json_t *out = json_copy(kwargs);
    json_set(out, "messages", shaped_messages ? json_copy(shaped_messages) : json_null());
    return out;
}
