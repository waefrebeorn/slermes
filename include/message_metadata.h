/*
 * message_metadata.h — C11 port of agent/message_metadata.py
 * Internal metadata attached to durable conversation messages.
 */
#ifndef SLERMES_MESSAGE_METADATA_H
#define SLERMES_MESSAGE_METADATA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: PERSISTENCE_ONLY_MESSAGE_FIELDS @ agent/message_metadata.py
 * NULL-terminated; these fields describe Hermes' durable record, not
 * provider-visible message content. They must not influence context-pressure
 * decisions. */
extern const char *const persistence_only_message_fields[];

/* Membership test for the frozenset above. */
int is_persistence_only_message_field(const char *key);

/* PoP: stamp_message_timestamp @ agent/message_metadata.py
 * Attach a creation timestamp without replacing a source-provided time.
 * timestamp <= 0 means "use the local wall clock". Returns the same mapping. */
json_t *stamp_message_timestamp(json_t *message, double timestamp);

#ifdef __cplusplus
}
#endif
#endif /* SLERMES_MESSAGE_METADATA_H */
