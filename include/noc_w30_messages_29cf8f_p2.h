/*
 * noc_w30_messages_29cf8f_p2.h - C surface for upstream
 * persistence/sqlite/conversation/messages.py, gate p2 (row stamps).
 *
 * The write path's pure decisions: the typed MessageRecord projection, the
 * RowStamp a write leaves on a message dict, the restored-row predicate and
 * stamping a batch write does, _row_outcomes' fresh/adopted/repaired
 * disposition, append_batch's chunk slicing, replace_transcript's kept-row
 * predicate and stamps, the reaction and unseen-reaction row shaping, and the
 * compression-cooldown snapshot/expiry derivation.
 *
 * Connections, transactions and the store's writers stay in the caller.
 */
#ifndef NOC_W30_MESSAGES_29CF8F_P2_H
#define NOC_W30_MESSAGES_29CF8F_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"row": {stored row dict...}} -> the MessageRecord projection, reactions
 * dropped from display_metadata and the sidecars decoded (_message_record). */
json_t *w30_messages_29cf8f_p2_message_record(const json_t *in);

/* {"msg": {...written message dict...}, "stored": {id: {timestamp,
 * message_uid}}} -> the RowStamp the write left (_stamp). */
json_t *w30_messages_29cf8f_p2_stamp(const json_t *in);

/* {"session_id": str, "messages": [msg...], "active_row": {...}|null,
 * "inserted": int} -> {"inserted": n, "messages": [...]} after the batch write
 * stamps a restored-but-unwritten dict with its durable row (_append_chunk). */
json_t *w30_messages_29cf8f_p2_append_chunk(const json_t *in);

/* {"rows": [{"_row_id": n, "_canonical_row": {...}|null, ...}], "count": n} ->
 * one RowOutcome per row in batch order (_row_outcomes). */
json_t *w30_messages_29cf8f_p2_row_outcomes(const json_t *in);

/* {"messages": [{NewMessage}...], "chunk_rows": n|null} -> {"chunks":
 * [[index...]...]} the chunk boundaries append_batch writes (_append_batch). */
json_t *w30_messages_29cf8f_p2_batch_chunks(const json_t *in);

/* {"messages": [{NewMessage}...], "mode": "replace"|"preserve_existing_archive"|
 * "archive_dropped", "stored": [row...], "reject_active_turn_lease": bool} ->
 * {"rows": [RowStamp...], "active_only": b, "archive_dropped": b,
 * "loaded_stored": b} (_replace_transcript's kept-row rule). */
json_t *w30_messages_29cf8f_p2_replace_rows(const json_t *in);

/* {"reactions": [row...]|null} -> [Reaction...] | null (set_reaction). */
json_t *w30_messages_29cf8f_p2_reactions(const json_t *in);

/* {"pending": [row...]} -> [UnseenReaction...] (take_unseen_reactions). */
json_t *w30_messages_29cf8f_p2_unseen_reactions(const json_t *in);

/* {"row": {"session_exists": b, "cooldown_until": n|null, "error": s|null}} ->
 * CooldownSnapshot (read_compression_cooldown_snapshot). */
json_t *w30_messages_29cf8f_p2_cooldown_snapshot(const json_t *in);

/* {"snapshot": {present, until, error}, "at": n} -> CompressionCooldown|null
 * (read_compression_cooldown). */
json_t *w30_messages_29cf8f_p2_compression_cooldown(const json_t *in);

/* {"message_id": n|null, "row": {"session_id": s, "active": b, "compacted": b}
 * |null} -> {"session_id": s, "visibility": v} | null (message_storage_state). */
json_t *w30_messages_29cf8f_p2_storage_state(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
