/*
 * noc_w25_detachedre_4e5201.h - C surface for upstream
 * persistence/sqlite/conversation/_store/detached_results.py (gate w25_detachedre_4e5201).
 *
 * Idempotent detached-result appends: the value-only decision in front of the
 * writer.  Which display_kind a delegation delivery becomes, the exact lineage
 * match SQL each append looks for (so a replayed delivery returns the existing
 * row), the match parameters, and the completion note's metadata merge.  The
 * writer transaction, the message row builder, the uid minting and the
 * active-turn lease guard stay a seam: the append is a fixed result here.
 */
#ifndef NOC_W25_DETACHEDRE_4E5201_H
#define NOC_W25_DETACHEDRE_4E5201_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The compressed-lineage ancestor walk every detached append matches through. */
#define W25_DETACHEDRE_4E5201_LINEAGE_MATCH_SQL \
    "WITH RECURSIVE lineage(id) AS (SELECT ? UNION\n    SELECT s.parent_session_id FROM sessions s JOIN lineage l ON s.id = l.id\n    JOIN sessions p ON p.id = s.parent_session_id WHERE p.end_reason = 'compression'\n) SELECT m.id FROM messages m JOIN lineage l ON m.session_id = l.id WHERE "

/* The delegation delivery's own row condition (display_kind + delegation_id +
 * delivery_notice). */
#define W25_DETACHEDRE_4E5201_DELEGATION_MATCH_SQL \
    "m.display_kind IN ('async_delegation_complete', 'hidden') AND json_extract(m.display_metadata, '$.delegation_id') = ? AND coalesce(json_extract(m.display_metadata, '$.delivery_notice'), '') = ?"

/* The completion note's own row condition. */
#define W25_DETACHEDRE_4E5201_COMPLETION_MATCH_SQL \
    "json_extract(m.display_metadata, '$.completion_admission') = ?"

/* {"session_id": str, "content": str, "metadata": {...}} ->
 * {"ok": true, "kind": str, "match_sql": str, "params": [delegation_id,
 *  delivery_notice], "metadata": {...}}
 * or {"ok": false, "error": "ValueError", "message": str} when the metadata
 * carries no stable delegation_id (Python-falsy rejected: None/''/0/[]/{}). */
json_t *w25_detachedre_4e5201_delegation_delivery(const json_t *req);

/* {"admission_id": <any>, "content": str, "display_kind": str,
 *  "metadata": {...}} -> {"ok": true, "kind": display_kind,
 *  "match_sql": str, "params": [admission_id], "metadata": {... with
 *  completion_admission}} */
json_t *w25_detachedre_4e5201_completion_note(const json_t *req);

/* The match SQL constants above, as an object. */
json_t *w25_detachedre_4e5201_constants(void);

#ifdef __cplusplus
}
#endif
#endif
