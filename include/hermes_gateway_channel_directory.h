/**
 * @file hermes_gateway_channel_directory.h
 * @brief Channel directory API (port of Python gateway/channel_directory.py).
 */
#ifndef HERMES_GATEWAY_CHANNEL_DIRECTORY_H
#define HERMES_GATEWAY_CHANNEL_DIRECTORY_H

#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Channel Directory
 * ================================================================ */

/* Human-facing target label for a channel entry.
 * Returns malloc'd string. Caller must free. */
char *channel_target_name(const char *platform_name, json_node_t *channel);

/* Human-friendly session entry name from origin metadata.
 * Returns malloc'd string. Caller must free. */
char *session_entry_name(json_node_t *origin);

/* Build session-based channel entries from sessions.json for a platform.
 * Returns JSON array. Caller must json_free. */
json_node_t *build_from_sessions(const char *platform_name);

/* Load the cached channel directory from disk.
 * Returns json_node_t: {updated_at, platforms}. Caller must json_free. */
json_node_t *load_directory(void);

/* Look up channel type string for a chat_id.
 * Returns malloc'd string or NULL. Caller must free. */
char *lookup_channel_type(const char *platform_name, const char *chat_id);

/* Resolve a human-friendly channel name to a numeric ID.
 * Returns malloc'd string or NULL. Caller must free. */
char *resolve_channel_name(const char *platform_name, const char *name);

/* Format the channel directory as a human-readable list.
 * Returns malloc'd string (caller must free). */
char *format_directory_for_display(void);

/* Normalize a channel query (strip leading #, lowercase).
 * Returns malloc'd string. Caller must free. */
char *normalize_channel_query(const char *value);

/* Build session entry ID from chat_id + optional thread_id.
 * Returns malloc'd string ("chat_id:thread_id" or "chat_id"). Caller must free. */
char *session_entry_id(const char *chat_id, const char *thread_id);

/* PoP: _normalize_adapter_channels @ gateway/channel_directory.py:_normalize_adapter_channels */
/* Validate and dedupe channel entries from adapter list_channels() JSON.
 * Returns malloc'd JSON array of {id, name, type, [thread_id], [guild]}. */
json_t *normalize_adapter_channels(const char *raw_json);

#endif /* HERMES_GATEWAY_CHANNEL_DIRECTORY_H */