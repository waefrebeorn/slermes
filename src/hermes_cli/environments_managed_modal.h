/*
 * port_tools_environments_managed_modal.h — Slermes C11 port of the
 * managed-environment modal execution tool (tools/environments/...).
 *
 * Public surface consumed by the CLI command handlers. Faithful
 * extraction from the god header so callers no longer include hermes.h
 * transitively.
 */

#ifndef PORT_TOOLS_ENVIRONMENTS_MANAGED_MODAL_H
#define PORT_TOOLS_ENVIRONMENTS_MANAGED_MODAL_H

#include "hermes_json.h"   /* json_node_t */

#ifdef __cplusplus
extern "C" {
#endif

json_node_t *cli_tools_environments_managed_modal__start_modal_exec(const char *sandbox_id,
                                                                     json_node_t *prepared);
json_node_t *cli_tools_environments_managed_modal__poll_modal_exec(const char *sandbox_id,
                                                                    const char *exec_id);
/* Cancel a running modal exec by exec id. */
void cli_tools_environments_managed_modal__cancel_modal_exec(const char *sandbox_id,
                                                              const char *exec_id);
/* Cancel an exec by id (no sandbox required). */
void cli_tools_environments_managed_modal__cancel_exec(const char *exec_id);
json_node_t *cli_tools_environments_managed_modal__timeout_result_for_modal(int timeout);
json_node_t *cli_tools_environments_managed_modal__create_sandbox(const char *image,
                                                                   const char *cwd,
                                                                   int timeout);
json_node_t *cli_tools_environments_managed_modal__format_error(const char *prefix,
                                                                int status_code,
                                                                const char *body);
int cli_tools_environments_managed_modal__guard_unsupported_credential_passthrough(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_ENVIRONMENTS_MANAGED_MODAL_H */
