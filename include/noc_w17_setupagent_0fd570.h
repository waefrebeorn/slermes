#ifndef NOC_W17_SETUPAGENT_0FD570_H
#define NOC_W17_SETUPAGENT_0FD570_H

#include "hermes_json.h"

/* C port of hermes_cli/setup_agent_import.py (gate w17_setupagent_0fd570).
 * All exports carry the verbatim gate-key prefix. Returned char* values are
 * malloc'd/caller-owned; NULL = Python None. */

/* Module constants: PLUGIN and the full IMPORT_SOURCES table. */
json_t *w17_setupagent_0fd570_constants(void);

/* The child argv built by _import_child(). `executable`, `source`, `selector`
 * (array of str) and `extra_args` (array of str) are injected; `from_setup`
 * selects source.extra_args and the trailing --from-setup flag. */
json_t *w17_setupagent_0fd570_import_argv(const char *executable, const char *source,
                                          json_t *extra_args, json_t *selector, bool from_setup);

/* The no-source-name hint line printed by migrate_from_agents(). */
char *w17_setupagent_0fd570_migrate_hint(json_t *found_labels, bool show_found);

/* The "Found on this computer:" context line from offer_agent_import().
 * Paths are Windows Path str() (backslash separated); the first occurrence of
 * user_home is replaced with '~'. */
char *w17_setupagent_0fd570_found_context(const char *user_home, json_t *found);

#endif
