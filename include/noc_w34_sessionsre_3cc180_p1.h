/*
 * noc_w34_sessionsre_3cc180_p1.h - C surface for upstream
 * hermes_cli/sessions_repair_profiles.py (gate w34_sessionsre_3cc180_p1).
 *
 * The scan half of `hermes sessions repair-profiles`: which rows a store's
 * crossed-profile scan turns into which finding, and why.  Every store handle
 * (the repair API of a state.db) is a seam -- the answers it would give arrive
 * in the request object, and what is ported is the decision those answers
 * drive: what is repairable, what is report-only, what is skipped because a
 * move already owns the row.
 *
 * Request objects (all fields required unless noted):
 *
 *   ledgered_lineages  {"foreign": [<row>, ...]}
 *       -> {"<id>": "<reason>", ...}   rows a move must leave where they are.
 *
 *   scan_sessions  {"store": {"profile": str},
 *                   "claimed": [str], "routing_store": str|null,
 *                   "legacy_main": "report"|"rekey"|"move", "multiplexes": bool,
 *                   "crossed": {"mislabelled": [row], "foreign": [row],
 *                               "crossed_parents": [row]}}
 *       -> [{"kind","store","subject","detail","action","reason","repairable"}]
 *
 *   scan_topics  {"store": {"profile": str}, "rows": [row]}
 *       -> [finding]
 *
 *   scan_routing  {"store": {"profile": str, "routing": bool}, "rows": [row],
 *                  "claimed": [str], "routing_store": str|null,
 *                  "multiplexes": bool}
 *       -> [finding]
 *
 * A row is a JSON object; a field the upstream code would raise KeyError on is
 * read here as None ("None" where the f-string would print it).
 */
#ifndef NOC_W34_SESSIONSRE_3CC180_P1_H
#define NOC_W34_SESSIONSRE_3CC180_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rows whose lineage holds a gateway ledger: copying them would duplicate a
 * session the ledger still references, so they stay put. */
json_t *w34_sessionsre_3cc180_p1_ledgered_lineages(const json_t *foreign);

/* Findings of one store's session scan (foreign rows, mislabelled rows,
 * crossed parents), in the order the plan records them. */
json_t *w34_sessionsre_3cc180_p1_scan_sessions(const json_t *req);

/* Findings for profile-less telegram topic bindings. */
json_t *w34_sessionsre_3cc180_p1_scan_topics(const json_t *req);

/* Findings for gateway_routing rows in the wrong store (or in no live
 * namespace), given the routing index's store and the multiplex verdict. */
json_t *w34_sessionsre_3cc180_p1_scan_routing(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
