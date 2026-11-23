#ifndef NOC_CLIWORKTREE_H
#define NOC_CLIWORKTREE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: cliworktree_enter_new_worktree @ hermes_cli/cli_commands_worktree.py:enter_new_worktree
 *
 * The observable decision of move_cli_workspace being accepted ("ok"), refused
 * by the gateway ("gateway_error"), or failing to chdir ("os_error"). For the
 * latter, gateway_conn_is_none models gateway_connection(cli) is None.
 * Returns {"ok": bool, "cleanup": bool, "prints": int} where `cleanup` says
 * _cleanup_worktree was called (and the earlier active tree restored) and
 * `prints` is the number of status lines emitted. */
struct json_t *cliworktree_enter_new_worktree(const char *move_outcome,
                                              int gateway_conn_is_none);

#ifdef __cplusplus
}
#endif
#endif /* NOC_CLIWORKTREE_H */
