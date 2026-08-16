/*
 * port_tools_environments_modal_utils.h — Slermes C11 port of the
 * managed-environment modal execution helpers (tools/environments/...).
 *
 * Public surface consumed by port_tools_environments_managed_modal.c and
 * other environment tool modules. Faithful extraction from the god header
 * so callers no longer include hermes.h transitively.
 */

#ifndef PORT_TOOLS_ENVIRONMENTS_MODAL_UTILS_H
#define PORT_TOOLS_ENVIRONMENTS_MODAL_UTILS_H

#include "hermes_json.h"   /* json_node_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Wrap a command so its stdin is fed from a heredoc. */
char *cli_tools_environments_modal_utils_wrap_modal_stdin_heredoc(const char *command,
                                                                  const char *stdin_data);
/* Wrap a command to pipe a sudo password. */
char *cli_tools_environments_modal_utils_wrap_modal_sudo_pipe(const char *command,
                                                             const char *password);
/* Execute a prepared modal command (returns raw result node). */
json_node_t *cli_tools_environments_modal_utils_execute(const char *sandbox_id,
                                                         const char *command,
                                                         const char *cwd, int timeout,
                                                         const char *stdin_data);
/* Prepare a modal exec (build the prepared-state node). */
json_node_t *cli_tools_environments_modal_utils__prepare_modal_exec(const char *command,
                                                                    const char *cwd,
                                                                    int timeout,
                                                                    const char *stdin_data);
/* Build a success result node from command output + return code. */
json_node_t *cli_tools_environments_modal_utils__result(const char *output, int returncode);
/* Build an error result node. */
json_node_t *cli_tools_environments_modal_utils__error_result(const char *error);
/* Build a timeout result node for a modal exec. */
json_node_t *cli_tools_environments_modal_utils__timeout_result_for_modal(int timeout);
/* Start a prepared modal exec. */
json_node_t *cli_tools_environments_modal_utils__start_modal_exec(const char *sandbox_id,
                                                                   json_node_t *prepared);
/* Poll a running modal exec. */
json_node_t *cli_tools_environments_modal_utils__poll_modal_exec(const char *sandbox_id,
                                                                  const char *exec_id);
/* Cancel a running modal exec. */
void cli_tools_environments_modal_utils__cancel_modal_exec(const char *sandbox_id,
                                                           const char *exec_id);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_ENVIRONMENTS_MODAL_UTILS_H */
