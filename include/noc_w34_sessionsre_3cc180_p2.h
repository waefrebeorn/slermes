/*
 * noc_w34_sessionsre_3cc180_p2.h - C surface for upstream
 * hermes_cli/sessions_repair_profiles.py (gate w34_sessionsre_3cc180_p2).
 *
 * The verdict/report half: the json-file scans whose evidence is the file's own
 * content (voice modes, the sessions.json mirror), the batch ordering a move
 * uses, the two json rewrites an --apply performs, the move outcome table, and
 * the shape of the report --apply returns.
 *
 * File I/O, the state.db move primitives, snapshots and the atomic writer are
 * seams: the file's parsed content arrives in the request, the store's answers
 * arrive as values, and only what those values decide is ported here.
 *
 * Request objects:
 *
 *   voice_modes  {"store": {"profile": str}, "content": str|null,
 *                 "chats": {"<platform>:<chat>": [str]}}
 *       -> [finding]
 *
 *   sessions_json  {"store": {"profile": str}, "content": str|null,
 *                   "claimed": [str]}   -> [finding]
 *
 *   parents_first  {"payloads": [{"session": {"id": any,
 *                                             "parent_session_id": any}}]}
 *       -> [id, ...]  parents before children, insertion order for a cycle.
 *
 *   rekey_voice  {"content": str|null, "key": str, "owner": str}
 *       -> {"result": {...}, "data": <rewritten object>|null}
 *
 *   drop_entry  {"content": str|null, "key": str}
 *       -> {"result": {...}, "data": <rewritten object>|null}
 *
 *   move_batch  {"ids": [str], "payloads": {"<sid>": payload|null},
 *                "export_errors": [str], "import_errors": [str],
 *                "import_outcome": {"<sid>": str},
 *                "message_counts": {"<sid>": number},
 *                "delete_errors": [str], "delete_result": {"<sid>": bool}}
 *       -> {"<sid>": {"copied": 1} | {"missing": 1} | {"copied_incomplete": 1}
 *                    | {"<outcome>": 1, "moved": 1}, ...}
 *
 *   apply  {"findings": [{"kind": str, "subject": str,
 *                         "fix": {"counts": {...}} | {"error": {"message": str}}}],
 *           "stores": [{"profile": str, "db_exists": bool}],
 *           "snapshot_labels": {"<profile>": str|null}}
 *       -> {"snapshots": {...}, "totals": {...}, "failures": [...]}
 */
#ifndef NOC_W34_SESSIONSRE_3CC180_P2_H
#define NOC_W34_SESSIONSRE_3CC180_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Profile-less voice-mode entries owned by exactly one non-default bot. */
json_t *w34_sessionsre_3cc180_p2_voice_modes(const json_t *req);

/* sessions.json mirror entries for a namespace no live profile claims. */
json_t *w34_sessionsre_3cc180_p2_sessions_json(const json_t *req);

/* Move order: every parent in the batch precedes its children. */
json_t *w34_sessionsre_3cc180_p2_parents_first(const json_t *req);

/* Value half of _rekey_voice_mode_entry: the rewritten object and the counter,
 * with the atomic write left to the caller. */
json_t *w34_sessionsre_3cc180_p2_rekey_voice(const json_t *req);

/* Value half of _drop_sessions_json_entry. */
json_t *w34_sessionsre_3cc180_p2_drop_entry(const json_t *req);

/* The outcome table of one store-to-store move, parents-first, copy before
 * delete, with a failed row holding only its own lineage back. */
json_t *w34_sessionsre_3cc180_p2_move_batch(const json_t *req);

/* The report --apply returns: snapshots taken, counters accumulated, failures
 * recorded -- one bad row never abandoning the rest of the plan. */
json_t *w34_sessionsre_3cc180_p2_apply(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
