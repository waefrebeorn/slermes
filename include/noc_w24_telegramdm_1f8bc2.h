/*
 * noc_w24_telegramdm_1f8bc2.h - C surface for upstream
 * plugins/platforms/telegram/telegram_dm_topic_send.py
 * (gate w24_telegramdm_1f8bc2).
 *
 * The pure decision ladder that runs after the Bot API answers "message
 * thread not found" on a private-chat topic: is the chat private at all, does
 * this error wording mean the topic is gone, may the send be rerouted to the
 * chat root, and which kwargs survive the retry (the dead route and, in a
 * private chat, the reply anchor are dropped).  The Bot API call, the routing
 * repository and the binding prune are seams: they are not called here.
 */
#ifndef NOC_W24_TELEGRAMDM_1F8BC2_H
#define NOC_W24_TELEGRAMDM_1F8BC2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of "the topic is gone" wordings a private chat matches. */
#define W24_TELEGRAMDM_1F8BC2_PRIVATE_MARKERS 4
#define W24_TELEGRAMDM_1F8BC2_PUBLIC_MARKERS 1

/* chat_id: <any> -> bool.  int(str(chat_id).strip()) > 0; a value Python
 * cannot parse as an int is False (the TypeError/ValueError branch). */
json_t *w24_telegramdm_1f8bc2_private_chat(const json_t *chat_id);

/* chat_id, error: <any> -> bool.  any(marker in str(error).lower() for marker
 * in markers), markers chosen by private_chat(chat_id). */
json_t *w24_telegramdm_1f8bc2_topic_gone(const json_t *chat_id, const json_t *error);

/* chat_id, metadata: <any> -> bool.  A private dead topic may be rerouted
 * unless metadata["telegram_dm_topic_created_for_send"] is truthy. */
json_t *w24_telegramdm_1f8bc2_dead_topic_may_reroute(const json_t *chat_id,
                                                     const json_t *metadata);

/* kwargs: {...} -> {...}: the same mapping minus message_thread_id and
 * direct_messages_topic_id, and minus reply_to_message_id in a private chat.
 * Insertion order of the surviving keys is preserved. */
json_t *w24_telegramdm_1f8bc2_dead_topic_retry_kwargs(const json_t *kwargs);

#ifdef __cplusplus
}
#endif
#endif
