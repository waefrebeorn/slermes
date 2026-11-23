/*
 * cli_commands_worktree.c — C port of the decision in upstream Hermes
 * hermes_cli/cli_commands_worktree.py:enter_new_worktree.
 *
 * PoP: cliworktree_enter_new_worktree @ hermes_cli/cli_commands_worktree.py:enter_new_worktree
 *
 * move_cli_workspace's outcome and gateway presence are injected inputs (the
 * real work happens in gateway_session_store / gateway_automation); this is the
 * function's own control flow: a gateway refusal removes the fresh tree,
 * restores the earlier active tree and returns False; an OSError is a local
 * no-op (False) or a reported cwd-entry failure (True) depending on whether a
 * gateway connection owns the session.
 */
#include "noc_cliworktree.h"
#include "hermes_json.h"
#include <string.h>

json_t *cliworktree_enter_new_worktree(const char *move_outcome, int gateway_conn_is_none) {
    int ok = 1, cleanup = 0, prints = 0;
    if (move_outcome && !strcmp(move_outcome, "gateway_error")) {
        ok = 0; cleanup = 1; prints = 1;
    } else if (move_outcome && !strcmp(move_outcome, "os_error")) {
        ok = gateway_conn_is_none ? 0 : 1;
        prints = 1;
    }
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? true : false));
    json_set(o, "cleanup", json_bool(cleanup ? true : false));
    json_set(o, "prints", json_number((double)prints));
    return o;
}
