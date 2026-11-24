/*
 * noc_w18_pluginstra_7bc196.h -- public C API for the deterministic
 * update-dependency refusal message in hermes_cli/plugins_transaction.py
 * (wave18 gate w18_pluginstra_7bc196).
 */
#ifndef NOC_W18_PLUGINSTRA_7BC196_H
#define NOC_W18_PLUGINSTRA_7BC196_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_pluginstra_7bc196_update_deps_message @ hermes_cli/plugins_transaction.py:_refresh_declared_dependencies
 * The non-interactive refusal text of _refresh_declared_dependencies:
 *   added = tuple(s for s in after.reqs if s not in before.reqs)
 *   if not added and not (after.installs_packages and not before.installs_packages):
 *       return None
 *   "The update declares new Python dependencies (" + (", ".join(added) or
 *   "in its pyproject.toml") + "); run `hermes plugins update {name}` in a
 *   terminal to review them."
 * Returns JSON null when there is no message, else a JSON string. Caller frees. */
json_t *w18_pluginstra_7bc196_update_deps_message(json_t *before_reqs, json_t *after_reqs,
                                                  bool before_installs, bool after_installs,
                                                  const char *target_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_PLUGINSTRA_7BC196_H */
