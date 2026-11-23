#ifndef NOC_W13_CLICONFIG_2DCA4F_H
#define NOC_W13_CLICONFIG_2DCA4F_H

#include "hermes_json.h"

/* C port of two pure helpers in hermes_cli/cli_config_load.py
 * (gate w13_cliconfig_2dca4f). */

/* Overlay a parsed config file onto built-in defaults, in place (deep copied).
 * Returns a new json_t object (the merged defaults). */
json_t *w13_cliconfig_2dca4f_merge_file_config(const json_t *defaults,
                                               const json_t *file_config);

/* env -> top-level prefill_messages_file -> legacy agent.prefill_messages_file.
 * env_value is the already-fetched HERMES_PREFILL_MESSAGES_FILE carrier.
 * Returns a malloc'd string; caller frees. */
char *w13_cliconfig_2dca4f_resolve_prefill_messages_file(const char *env_value,
                                                         const json_t *config);

#endif /* NOC_W13_CLICONFIG_2DCA4F_H */
