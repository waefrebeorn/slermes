/*
 * noc_w23_updateadmi_517547.h - C surface for upstream
 * plugins/platforms/telegram/update_admission.py (gate w23_updateadmi_517547).
 *
 * The value-deciding half of Telegram update admission: the field-name log
 * line, the FIFO lane an update belongs to, and the concurrency limit a
 * malformed config value falls back to.
 *
 * Seams (never decided here): the PTB Update itself (its to_dict is the input),
 * a Message's attributes, the adapter's topic-adoption probe and its config.
 */
#ifndef NOC_W23_UPDATEADMI_517547_H
#define NOC_W23_UPDATEADMI_517547_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W23_UPDATEADMI_517547_DEFAULT_MAX_CONCURRENT 32
#define W23_UPDATEADMI_517547_SEEN_CAP 4096
#define W23_UPDATEADMI_517547_RECEIPT_TTL_SECONDS 86400

/* update.to_dict() -> "name, name(child,child), ..." (sorted keys, False values
 * treated as absent, one level of nesting). */
json_t *w23_updateadmi_517547_field_names(const json_t *data);

/* message -> its forum/DM topic id, or null when the message is not a real
 * topic message (or is not a message at all). */
json_t *w23_updateadmi_517547_topic_lane(const json_t *message);

/* chat.id, message, adopted-topic value (null = the adapter adopted nothing)
 * -> the FIFO lane keys, [[chat, thread], ...]. */
json_t *w23_updateadmi_517547_lane_keys(const json_t *chat_id, const json_t *message,
                                        const json_t *adopt);

/* config extra["max_concurrent_updates"] -> the limit used (default 32). */
json_t *w23_updateadmi_517547_update_limit(const json_t *raw);

/* The module constants, as an object. */
json_t *w23_updateadmi_517547_defaults(void);

#ifdef __cplusplus
}
#endif
#endif
