/*
 * noc_w25_apiservers_bf168e.h - C surface for upstream
 * gateway/platforms/api_server_store.py (gate w25_apiservers_bf168e).
 *
 * Which storage-interface handle an API/webhook adapter answers with.  The
 * stores themselves (SQLite handles, the profile cache, the reserving
 * authority's persistence) are seams: what arrives here is the *shape* of the
 * adapter's state -- whether a gateway runtime is bound, whether the profile
 * cache is closed, whether a ``_session_db`` override is present and whether it
 * is a handle, whether the requested home has a cached store, and whether the
 * authority for that home carries one.  What is ported is the decision those
 * values make, and the refusal an authority with no store produces.
 */
#ifndef NOC_W25_APISERVERS_BF168E_H
#define NOC_W25_APISERVERS_BF168E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* adapter_persistence(adapter, home):
 * request: {"runtime": <bool>, "cache_closed": <bool>,
 *           "override": null | "handle" | "other",
 *           "home_store": <bool>, "authority_present": <bool>,
 *           "authority_persistence": <bool>}
 * -> "none" | "authority_store" | "cache_store" | "override_store". */
json_t *w25_apiservers_bf168e_adapter_persistence(const json_t *request);

/* selected_session_db(adapter, home):
 * request: {"authority_present": <bool>, "cache_closed": <bool>}
 * -> "none" | "authority_store". */
json_t *w25_apiservers_bf168e_selected_session_db(const json_t *request);

/* authority_store(authority): request {"has_store": <bool>} ->
 * {"ok": true} or {"ok": false, "kind": "StoreUnavailable",
 *                  "error": "the authority has no storage interface handle"}. */
json_t *w25_apiservers_bf168e_authority_store(const json_t *request);

/* delete_session(store, session_id, sessions_dir) -> bool(removed):
 * request {"removed": <any>} -> true/false under Python truthiness. */
json_t *w25_apiservers_bf168e_delete_removed(const json_t *request);

#ifdef __cplusplus
}
#endif
#endif
