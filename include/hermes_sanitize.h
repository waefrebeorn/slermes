/* Self-contained public API for UTF-8 surrogate + non-ASCII sanitization.
 * No god headers — opaque types via core_types only. C11 only.
 */
#ifndef SLERMES_SANITIZE_H
#define SLERMES_SANITIZE_H

#include "hermes_core_types.h"   /* message_t */
#include "hermes_json.h"         /* json_t, json_node_t */

/* Replace lone UTF-8 surrogate code points (U+D800-U+DFFF) with U+FFFD.
 * Returns a malloc'd string (caller frees); returns NULL on OOM/empty. */
char *sanitize_surrogates(const char *text);

/* Recursively walk a json_t tree and replace surrogate chars in all string
 * values. Returns true if any were replaced. Mutates in place. */
bool sanitize_json_surrogates(void *json_obj);

/* Sanitize surrogate / non-ASCII code points across an array of messages. */
bool sanitize_messages_surrogates(message_t *messages, int count);
bool sanitize_messages_non_ascii(message_t *messages, int count);

/* Recursively scrub surrogate code points from a json_t tree (string values +
 * key names), returning true if any were replaced. Caller owns the mutated node. */
bool agent_message_sanitize_structure_surrogates(json_t *node);

/* Return a copy of the tools JSON with non-ASCII stripped from string values.
 * Caller frees the returned node. */
json_node_t *sanitize_tools_non_ascii(json_node_t *tools);

/* Escape invalid chars in a raw JSON string; returns malloc'd string. */
char *escape_invalid_chars_in_json_strings(const char *raw);

#endif /* SLERMES_SANITIZE_H */
