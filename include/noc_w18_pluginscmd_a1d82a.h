/*
 * noc_w18_pluginscmd_a1d82a.h -- public C API for the deterministic
 * known-issue warning builder of hermes_cli/plugins_cmd_install.py
 * (wave18 gate w18_pluginscmd_a1d82a).
 */
#ifndef NOC_W18_PLUGINSCMD_A1D82A_H
#define NOC_W18_PLUGINSCMD_A1D82A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_pluginscmd_a1d82a_known_issue_warnings @ hermes_cli/plugins_cmd_install.py:_known_issue_warnings
 * Faithful port of `[f"Known issue: {issue}" for issue in entry.known_issues]`.
 * `known_issues` is a JSON array of strings; returns a JSON array of strings.
 * Caller frees the returned json_t. */
json_t *w18_pluginscmd_a1d82a_known_issue_warnings(json_t *known_issues);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_PLUGINSCMD_A1D82A_H */
