/*
 * noc_w24_authextern_606ea5.h - C surface for upstream
 * hermes_cli/auth_external_process.py (gate w24_authextern_606ea5).
 *
 * Executable lookup and launch-env policy for external_process (ACP) providers:
 * which command a GUI/service launch runs, what lookup path a configured
 * command resolves to, and the env names an external-process provider owns (so
 * the generic env writers must refuse them).  The filesystem walk
 * (locate_command), the Claude Code prefix search and the provider registry are
 * seams; the value decisions are here.
 */
#ifndef W24_AUTHEXTERN_606EA5_H
#define W24_AUTHEXTERN_606EA5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (env_vars, env, process_command) -> the command a launch uses: the first set
 * process_command_env_vars override (stripped), else the profile's
 * process_command.  profile is the (env_vars, process_command) pair. */
json_t *w24_authextern_606ea5_command(const json_t *env_vars, const json_t *env,
                                      const json_t *process_command);

/* (command, cwd, home, env) -> the string handed to the resolver: unchanged for
 * a bare name, else the abspath of the expanded (``~``/``$VAR``/``%VAR%``)
 * command.  null when the command is empty/falsy or not text. */
json_t *w24_authextern_606ea5_lookup(const json_t *command, const json_t *cwd,
                                     const json_t *home, const json_t *env);

/* (profiles) -> sorted names from every external_process profile's
 * process_command_env_vars and process_args_env_var. */
json_t *w24_authextern_606ea5_launch_env_names(const json_t *profiles);

/* (profiles, launch_env) -> the union of the above with the left-core
 * launch_env names: what the generic env writers must refuse. */
json_t *w24_authextern_606ea5_denylist(const json_t *profiles, const json_t *launch_env);

#ifdef __cplusplus
}
#endif
#endif
