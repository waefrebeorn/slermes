/*
 * noc_w26_conversati_9bd9e4.h - C surface for upstream
 * agent/conversation_compression_reply_anchor.py (gate w26_conversati_9bd9e4).
 *
 * "Keep the just-delivered assistant reply live across a compaction commit":
 * which row is that reply, whether a compressed candidate already carries it,
 * where in the candidate its slot is, and the reinsertion itself with its
 * strict-alternation and no-growth guards.  The token estimator is a seam and
 * arrives as ``rough``; nothing here touches a store, a lock or a network.
 */
#ifndef NOC_W26_CONVERSATI_9BD9E4_H
#define NOC_W26_CONVERSATI_9BD9E4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {left, right, right_text?} -> visible-content equality (normalized text
 * when either side has any, else raw content). */
json_t *w26_conversati_9bd9e4_same_content(const json_t *req);

/* message -> sorted pairing ids of its tool calls. */
json_t *w26_conversati_9bd9e4_tool_ids(const json_t *message);

/* message -> the normalized tool_call_id of a tool result row ("" when none). */
json_t *w26_conversati_9bd9e4_result_id(const json_t *message);

/* messages -> sorted ids more than one assistant row uses. */
json_t *w26_conversati_9bd9e4_reused_ids(const json_t *messages);

/* req: {followers, compressed, reused_ids[]} -> the chronological slot of a
 * dropped reply inside ``compressed``, or null when it cannot be located. */
json_t *w26_conversati_9bd9e4_insertion_index(const json_t *req);

/* req: {original_messages, compressed, rough:{in,out,anchor}} ->
 * {anchor, compressed}: the row placed (marker stamped) and the candidate after
 * insertion, or {anchor: null, compressed: <unchanged>} when nothing was placed. */
json_t *w26_conversati_9bd9e4_anchor(const json_t *req);

/* The persistence marker and the summary/prefix tables the scan reads. */
json_t *w26_conversati_9bd9e4_constants(void);

#ifdef __cplusplus
}
#endif
#endif
