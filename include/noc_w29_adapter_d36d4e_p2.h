/*
 * noc_w29_adapter_d36d4e_p2.h - C surface for the inbound envelope decisions of
 * upstream plugins/platforms/signal/adapter.py (gate w29_adapter_d36d4e_p2).
 *
 * Sync-message unwrapping, group mention policy, group allowlisting, the
 * number↔UUID recipient caches, quote author extraction, own-quote detection,
 * the sent-timestamp caches and the whole _handle_envelope decision chain.
 * Attachment fetching, authorization and handle_message stay seams: the caller
 * passes the media types a successful fetch would have produced.
 */
#ifndef NOC_W29_ADAPTER_D36D4E_P2_H
#define NOC_W29_ADAPTER_D36D4E_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"envelope": <promoted dataMessage envelope or null>} */
json_t *w29_adapter_d36d4e_p2_unwrap_sync_message(const json_t *envelope_data, const json_t *account,
                                                  const json_t *recent_sent);

/* {"mentioned": bool, "text": str} */
json_t *w29_adapter_d36d4e_p2_apply_group_mention_rules(const json_t *text, const json_t *data_message,
                                                        const json_t *account, const json_t *require_mention,
                                                        const json_t *uuid_by_number);

json_t *w29_adapter_d36d4e_p2_group_allowed(const json_t *group_id, const json_t *allow);

/* {"number_to_uuid": {...}, "uuid_to_number": {...}} */
json_t *w29_adapter_d36d4e_p2_remember_recipient_identifiers(const json_t *number,
                                                             const json_t *service_id,
                                                             const json_t *maps);

json_t *w29_adapter_d36d4e_p2_extract_quote_author(const json_t *quote);

json_t *w29_adapter_d36d4e_p2_quote_references_own_message(const json_t *reply_to_id,
                                                           const json_t *reply_to_author,
                                                           const json_t *sent_timestamps,
                                                           const json_t *account,
                                                           const json_t *uuid_by_number,
                                                           const json_t *number_by_uuid);

/* bounded FIFO of outbound timestamps, oldest first */
json_t *w29_adapter_d36d4e_p2_remember_sent_message_timestamp(const json_t *cache,
                                                              const json_t *timestamp,
                                                              const json_t *max);
json_t *w29_adapter_d36d4e_p2_extract_contact_uuid(const json_t *contact, const json_t *phone);

/* {"echo": bool, "recent": [remaining keys]} */
json_t *w29_adapter_d36d4e_p2_consume_sent_timestamp(const json_t *recent, const json_t *ts);

/* The _handle_envelope decision chain:
 * {"dropped": bool, "chat_id", "text", "msg_type", "reply_to_id",
 *  "reply_to_author", "reply_to_name", "own"} */
json_t *w29_adapter_d36d4e_p2_envelope_decision(const json_t *envelope, const json_t *account,
                                                const json_t *allow, const json_t *ignore_stories,
                                                const json_t *require_mention, const json_t *recent_sent,
                                                const json_t *sent_cache, const json_t *media_types,
                                                const json_t *ignore_attachments,
                                                const json_t *uuid_by_number);

#ifdef __cplusplus
}
#endif
#endif
