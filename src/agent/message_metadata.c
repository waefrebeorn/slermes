/*
 * message_metadata.c — faithful C11 port of agent/message_metadata.py
 * (Internal metadata attached to durable conversation messages.)
 */
#include "message_metadata.h"
#include <string.h>
#include <time.h>

/* PoP: PERSISTENCE_ONLY_MESSAGE_FIELDS @ agent/message_metadata.py
 * These fields describe Hermes' durable record, not provider-visible message
 * content. They must not influence context-pressure decisions. */
const char *const persistence_only_message_fields[] = {
    "timestamp",
    NULL,
};

int is_persistence_only_message_field(const char *key) {
    if (!key) return 0;
    for (const char *const *p = persistence_only_message_fields; *p; p++)
        if (strcmp(*p, key) == 0) return 1;
    return 0;
}

/* PoP: stamp_message_timestamp @ agent/message_metadata.py
 * Attach a creation timestamp without replacing source-provided time.
 * Works on a json object message mapping (libjson). Returns the same
 * message pointer, like the Python helper returns the same mapping. */
json_t *stamp_message_timestamp(json_t *message, double timestamp) {
    if (!message || message->type != JSON_OBJECT) return message;
    json_t *existing = json_object_get(message, "timestamp");
    if (existing && !(existing->type == JSON_NULL)) return message;
    double ts = (timestamp != 0.0) ? timestamp : (double)time(NULL);
    json_object_set(message, "timestamp", json_new_number(ts));
    return message;
}
