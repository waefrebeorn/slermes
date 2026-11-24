/*
 * noc_w31_mcp_de9cb4_p2.h - C surface for upstream tools/connectors/mcp.py
 * (gate w31_mcp_de9cb4_p2, plus the one row shape it takes from
 * tools/connectors/operation.py).
 *
 * The other half of the same module: how a target is reported, which of the
 * declared credentials still block an install, what the caller without a card
 * gets back, which row the card's answer advances, and the two one-line
 * predicates the runner and the catalog backend decide with.  The operation
 * object, the catalog entry, the thread that runs an install and the tool
 * registry stay seams; what is ported is the decision each of them is asked for.
 */
#ifndef NOC_W31_MCP_DE9CB4_P2_H
#define NOC_W31_MCP_DE9CB4_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {declared: [{name, prompt, required, secret, default}], env: {}} ->
 * the declared specs that are required and still have no value. */
json_t *w31_mcp_de9cb4_p2_missing_required(const json_t *req);

/* req: {required: [...], home} -> the "set X in the environment or <home>/.env"
 * answer the no-card install gives. */
json_t *w31_mcp_de9cb4_p2_install_now_message(const json_t *req);

/* req: {profile_key, home_key, session_key} -> [profile_key or home_key, session]. */
json_t *w31_mcp_de9cb4_p2_late_key(const json_t *req);

/* req: {target: {name, kind, action, state, detail, instructions, connect_url,
 *                alias, phase, connection_id, attempt, required_env, launch,
 *                extra}, with_url} -> the target's snapshot as the card and the
 * model read it. */
json_t *w31_mcp_de9cb4_p2_target_snapshot(const json_t *req);

/* req: {op_id, deadline_at, targets: [{name, kind, action, state, ...}]} -> the
 * whole payload the no-card path returns: the operation's result, its status
 * and its note.  Identity fields are supplied, so the comparison is exact. */
json_t *w31_mcp_de9cb4_p2_no_card_result(const json_t *req);

/* req: {answer: "<the connection.respond payload>", targets: [{name, state}],
 *       runner} -> one decision per answer entry: {"name": <normalised>,
 *       "matched": <the operation had that target>, "action":
 *       "skip"|"approve"|"retry"|"none"} plus whether the answer settled it. */
json_t *w31_mcp_de9cb4_p2_answer_plan(const json_t *req);

/* req: {auth_type, auth_provider} -> the card must run the entry's own OAuth. */
json_t *w31_mcp_de9cb4_p2_installs_with_oauth(const json_t *req);

/* req: {state} -> who may cause the next move ("user" or "backend_watcher"). */
json_t *w31_mcp_de9cb4_p2_actor_for(const json_t *req);

/* req: {settled, state} -> whether a read may advance this target. */
json_t *w31_mcp_de9cb4_p2_observable(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
