#ifndef NOC_W21_SEARCH_EFF38D_H
#define NOC_W21_SEARCH_EFF38D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/connectors/search.py (gate w21_search_eff38d).
 *
 * The connector leg of tool search: whether connectors are in scope at all, the
 * "hosted connectors are unavailable" field with its fix-up hint, the per-query
 * result merge from the gateway's `connector_search` leg (echo filtering,
 * composed-name collision resolution, per-query ordering), the schema
 * transform that turns file inputs into path strings, and the `remote_schemas`
 * leg for `connectors__*` names.
 *
 * Not ported (bound to the gateway bridge): the `connector_search` /
 * `connector_describe` calls themselves -- the fixtures are the leg's
 * (failure, payload) -- and `_tokens` in the catalog entry, which is the
 * Snowball-stemmed BM25 index (tools.tool_search_catalog._tokenize).
 */

/* PoP: w21_search_eff38d_format_name @ tools/connectors/gateway/names.py:format_connector_name
 * `connectors__<connector>__<tool>` with the conventional `<CONNECTOR>_` prefix
 * removed from the tool slug. Caller frees. */
char *w21_search_eff38d_format_name(const char *connector, const char *tool);

/* PoP: w21_search_eff38d_vendor_candidates @ tools/connectors/gateway/names.py:vendor_slug_candidates
 * `("<CONNECTOR>_<tool>", "<tool>")`. */
json_t *w21_search_eff38d_vendor_candidates(const char *connector, const char *tool);

/* PoP: w21_search_eff38d_is_connector_name @ tools/connectors/gateway/names.py:is_connector_name */
int w21_search_eff38d_is_connector_name(const json_t *name);

/* PoP: w21_search_eff38d_model_parameters @ tools/connectors/gateway/files.py:model_parameters
 * The model-facing parameter schema: `file_uploadable` subschemas become
 * `{"type":"string","description":"<desc> Pass a local file path."}`. A
 * non-object schema is `{}`. */
json_t *w21_search_eff38d_model_parameters(const json_t *input_schema);

/* PoP: w21_search_eff38d_connections_in_scope @ tools/connectors/search.py:connections_in_scope
 * `any(_fn(td).get("name") == "manage_connections" for td in tool_defs)` */
int w21_search_eff38d_connections_in_scope(const json_t *tool_defs);

/* PoP: w21_search_eff38d_unavailable @ tools/connectors/search.py:connectors_unavailable
 * req: failure, verb, names[], last_failure (what guest_setup_failure() answers).
 *   {"status":"not_set_up","hint":"...","names":[...]}
 *   {"status":"unavailable","reason":"<failure>","hint":"...","names":[...]} */
json_t *w21_search_eff38d_unavailable(const json_t *req);

/* PoP: w21_search_eff38d_entries_by_group @ tools/connectors/search.py:connector_entries_by_group
 * req: queries[], leg{failure, payload{failure,payload...}} -> the bridge answer
 *   {"per_query":[["<entry>",...],...],"failure":"..."|null}
 * An entry is {"name","description","schema","source","source_name"}. */
json_t *w21_search_eff38d_entries_by_group(const json_t *req);

/* PoP: w21_search_eff38d_remote_schemas @ tools/connectors/search.py:remote_schemas_for
 * req: names[], tool_defs[], leg{failure,payload} (the describe answer).
 *   {"tools":{...},"failure":"unreachable"|"<leg failure>"|null} */
json_t *w21_search_eff38d_remote_schemas(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_SEARCH_EFF38D_H */
