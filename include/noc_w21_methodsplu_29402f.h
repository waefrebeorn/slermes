/*
 * noc_w21_methodsplu_29402f.h -- public C API for the port of
 *   tui_gateway/methods_plugin_catalog.py (wave 21 batch 5).
 *
 * `plugins.catalog.lookup`: one curated-catalog entry resolved by name, with
 * the same name rule `hermes plugins install <name>` applies, the same removal
 * kill list check (`hermes_cli.plugin_catalog.match_removed`, repo spellings
 * normalised), and the same status vocabulary:
 *   invalid_name | unavailable | unknown | found{entry:{name,repo,sha,subdir,tier}}
 *
 * The live catalog reader (`load_catalog_live`, network + cache + in-tree
 * fallback) and the kill-list resolver are seams: the caller passes the entries
 * and removed records they answered with.
 */
#ifndef NOC_W21_METHODSPLU_29402F_H
#define NOC_W21_METHODSPLU_29402F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w21_methodsplu_29402f_lookup @ tui_gateway/methods_plugin_catalog.py:_
 * params: {"name": <value>} (str(value or "").strip()); entries: the catalog
 * list [{"name","repo","sha","subdir","tier"}]; removed: the resolved kill list
 * [{"name","repo",...}]. Returns the payload the handler answers with. */
json_t *w21_methodsplu_29402f_lookup(const json_t *params, const json_t *entries,
                                     const json_t *removed);

/* PoP: w21_methodsplu_29402f_looks_like_catalog_name @ hermes_cli/plugins_cmd_catalog.py:looks_like_catalog_name
 * Bare [a-z0-9_-]{1,64} token: no "/" or "\", no URL scheme prefix. Returns a
 * fresh bool node. */
json_t *w21_methodsplu_29402f_looks_like_catalog_name(const json_t *identifier);

/* PoP: w21_methodsplu_29402f_normalize_repo @ hermes_cli/plugin_catalog.py:_normalize_repo
 * Canonical "host/path": scheme, user, "www.", ".git" and trailing slashes are
 * spelling, not identity. Caller frees. */
char *w21_methodsplu_29402f_normalize_repo(const char *url);

/* PoP: w21_methodsplu_29402f_match_removed @ hermes_cli/plugin_catalog.py:match_removed
 * Whether `candidate` (a name or repo URL) is on the kill list: exact name, or
 * a normalised repo match. Returns a fresh bool node. */
json_t *w21_methodsplu_29402f_match_removed(const json_t *candidate, const json_t *removed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_METHODSPLU_29402F_H */
