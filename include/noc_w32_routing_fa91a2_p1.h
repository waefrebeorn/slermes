/*
 * noc_w32_routing_fa91a2_p1.h - C surface for the routing statements and the
 * coercions that build them, upstream persistence/sqlite/gateway/routing.py
 * (gate w32_routing_fa91a2_p1).
 *
 * Ported: the topic-profile name normaliser and the routing-key namespace
 * reader (persistence/gateway/routing.py leaves this module imports), the
 * gateway_routing upsert statement with its named parameters, the
 * session_for_origin / never-active / peer lookups and the routing columns
 * write of record_session_peer, plus the telegram_dm_topic_mode enable
 * statement.  Every statement is returned byte-for-byte as the verb sends it,
 * with the parameter sequence it binds.
 *
 * The SQLite holder (writer connection, read pool, schema migration, clock,
 * the store's own profile) is a seam: it is fed as fixture values, so nothing
 * here opens a file or reads a row.
 */
#ifndef NOC_W32_ROUTING_FA91A2_P1_H
#define NOC_W32_ROUTING_FA91A2_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {value} -> the stripped profile name; empty or missing reads "default". */
json_t *w32_routing_fa91a2_normalize_profile(const json_t *in);

/* in: {session_key} -> the "<ns>" slot of an "agent:<ns>:..." key, else "main". */
json_t *w32_routing_fa91a2_route_namespace(const json_t *in);

/* in: {session_key, entry_json, routing_scope?, now?} ->
 * {calls: [{sql, params}]}; an empty key or entry writes nothing. */
json_t *w32_routing_fa91a2_upsert_plan(const json_t *in);

/* in: {platform, chat_id, thread_id?, user_id?} -> {calls: [{sql, params}]};
 * no platform or no chat id reads nothing. */
json_t *w32_routing_fa91a2_origin_query(const json_t *in);

/* in: {older_than_days, now?} -> {calls: [{sql, params}]} carrying the cutoff,
 * or {error} for a negative retention (the ValueError text). */
json_t *w32_routing_fa91a2_never_active_query(const json_t *in);

/* in: {source, user_id?, session_key, chat_id?, chat_type?, thread_id?,
 *      owner?} -> {calls: [...]}: the keyed peer lookup, then the fenced peer
 * tuple lookup only when both chat id and chat type are given. */
json_t *w32_routing_fa91a2_peer_plan(const json_t *in);

/* in: {session_id, source, session_key, user_id?, chat_id?, chat_type?,
 *      thread_id?, display_name?, origin_json?, transport_profile?,
 *      include_compression_ancestors?, existing_row?, owner?, now?} ->
 * {calls: [...]}: the UPDATE (lineage CTE when ancestors are included), the
 * existence probe and, when the row is missing, the full-identity INSERT. */
json_t *w32_routing_fa91a2_record_peer_plan(const json_t *in);

/* in: {chat_id, user_id, profile_name?, has_topics_enabled?,
 *      allows_users_to_create_topics?, now?} -> {calls: [{sql, params}]}. */
json_t *w32_routing_fa91a2_enable_topic_plan(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
