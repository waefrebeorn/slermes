#ifndef NOC_AUXWIRE_H
#define NOC_AUXWIRE_H
/*
 * Public C API for the port of agent/auxiliary_wire.py (gate auxwire).
 * All exported symbols are module-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/* Faithful port of relay_chat_metadata: {**metadata, "api_mode": "chat_completions"}.
 * Returns a json object (caller frees). */
json_t *auxwire_relay_chat_metadata(const json_t *metadata);

/*
 * Decision core of prepare_chat_messages. LIMITATION: the live function tests
 * isinstance(client, (OpenAI, AsyncOpenAI)) then runs shape_wire_carriers and
 * ChatCompletionsTransport.convert_messages, which reach into the provider /
 * plugin registry that a pure C port cannot query. The caller supplies the
 * ALREADY-RESOLVED facts: client_is_chat_sdk = that isinstance; the presence of
 * "messages" in kwargs is derived from kwargs itself; shaped_messages is the
 * final converted wire messages. When the guard fails the live function
 * returns kwargs unchanged. Returns a json object (caller frees).
 */
json_t *auxwire_prepare_chat_messages(const json_t *kwargs, int client_is_chat_sdk,
        const json_t *shaped_messages);

#endif /* NOC_AUXWIRE_H */
