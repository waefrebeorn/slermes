/*
 * noc_w24_locallinea_6fdf26.h - C surface for upstream
 * persistence/sqlite/gateway/_local_lineage.py (gate w24_locallinea_6fdf26).
 *
 * The hand-off of a local conversation whose current target a messaging lane
 * took over: which receipts moved, the keys their release retires, the segments
 * of the conversation that follow the lane, the head of a /new segment and the
 * title that follows that head.  The connection is a seam -- every function here
 * takes the rows a query would have returned, so nothing is opened, read or
 * written; the row projections and the decisions over them are what is ported.
 */
#ifndef NOC_W24_LOCALLINEA_6FDF26_H
#define NOC_W24_LOCALLINEA_6FDF26_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_LOCALLINEA_6FDF26_POLICY_PREFIX "gateway.local_policy.v1:"
#define W24_LOCALLINEA_6FDF26_LEGACY_PREFIX "gateway.local_legacy.v1:"

/* _receipt(raw): the parsed receipt dict, or null for anything json.loads
 * refuses or that is not an object. */
json_t *w24_locallinea_6fdf26_receipt(const json_t *raw);

/* moved_receipts(conn, target_ids): rows are [owner, target, source] triples as
 * the join returned them -> {owner: [target, source]}, narrowed to *target_ids*
 * when that is a list. */
json_t *w24_locallinea_6fdf26_moved(const json_t *rows, const json_t *target_ids);

/* release_receipts(conn, owners): the (policy, legacy) key pairs the DELETE
 * retires, in owner order. */
json_t *w24_locallinea_6fdf26_release_keys(const json_t *owners);

/* hand_off_conversations(conn, moved): *moved* is {owner: [target, source]},
 * *receipts* maps owner -> the stored receipt text (null/absent when the row is
 * missing) ->
 *   {"handed": {owner: [target, source]}, "segments": {owner: [session ids]},
 *    "released": [[policy key, legacy key], ...]}
 * A receipt whose lineage is malformed (not a list, or not all strings) is not
 * handed: it stays as it is. */
json_t *w24_locallinea_6fdf26_handoff(const json_t *moved, const json_t *receipts);

/* _segment_root(conn, segments, target): the first row of the target's /new
 * segment, back through the compressions behind it; *end_reasons* maps a session
 * id -> its end_reason (absent for a row without one). */
json_t *w24_locallinea_6fdf26_segment_root(const json_t *segments,
                                           const json_t *target,
                                           const json_t *end_reasons);

/* _title_follows_head(conn, owner, head): the conversation's name moves to its
 * head -- 1 when the two UPDATEs would run.  *owner_row* is
 * [title, title_source, hidden], *head_row* [title, title_source], either null
 * when the row is missing.  A title of equal or higher provenance stays. */
int w24_locallinea_6fdf26_title_moves(const json_t *owner, const json_t *head,
                                      const json_t *owner_row,
                                      const json_t *head_row);

/* _title_rank(source): None ranks as user (2), llm 1, derived 0, anything else 0. */
int w24_locallinea_6fdf26_title_rank(const json_t *source);

/* POLICY_PREFIX, LEGACY_PREFIX and the _LANE_IDENTITY column list. */
json_t *w24_locallinea_6fdf26_constants(void);

#ifdef __cplusplus
}
#endif
#endif
