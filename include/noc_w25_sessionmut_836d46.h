/*
 * noc_w25_sessionmut_836d46.h - C surface for upstream
 * gateway/session_mutation_policy.py (gate w25_sessionmut_836d46).
 *
 * The pure decision core of the policy-only session edits: the ACP edit-policy
 * gate, the Move-to-project cwd/TERMINAL_CWD merge, and whether /reasoning pins
 * a level or hands it back to the profile.  The authority, the policy blob, the
 * filesystem probe and the git probe are seams: every one of them is a fixed
 * input here, and only what those values decide is ported.
 */
#ifndef NOC_W25_SESSIONMUT_836D46_H
#define NOC_W25_SESSIONMUT_836D46_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {source, old_policy, payload_policy} ->
 * {status:"refused", reason, field, detail} | {status:"noop", edit_approval_policy}
 * | {status:"rebound", edit_approval_policy}   (prepare_edit_policy). */
json_t *w25_sessionmut_836d46_edit_policy(const json_t *req);

/* req: {cwd, isdir, branch, root, terminal_json} ->
 * {status:"refused", reason, field, detail} | {status:"ok", cwd, terminal, git}. */
json_t *w25_sessionmut_836d46_workspace(const json_t *req);

/* req: {effort, ignore_user_config, profile, frozen} ->
 * {calls:[{op:"pin",value}|{op:"unpin",profile}], frozen}   (prepare_reasoning). */
json_t *w25_sessionmut_836d46_reasoning(const json_t *req);

/* The PREPARE route table, as an object. */
json_t *w25_sessionmut_836d46_constants(void);

#ifdef __cplusplus
}
#endif
#endif
