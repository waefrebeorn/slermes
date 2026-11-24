/*
 * noc_w25_updatecmdp_22338d.h - C surface for upstream
 * hermes_cli/update_cmd_pause_gate.py (gate w25_updatecmdp_22338d).
 *
 * The pure decision core of the paused gateways' tree gate: what one
 * checkout-writing git step binds to (its targets, its in-place paths, the
 * baseline it shares or gets of its own), and how the step is judged the moment
 * git returns (HEAD moved and the tree torn, or the tree whole).  The pause
 * record (install root, tree read, torn-path read, write) and git are seams:
 * every one of them is a fixed input here.
 */
#ifndef NOC_W25_UPDATECMDP_22338D_H
#define NOC_W25_UPDATECMDP_22338D_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {token:{pause_id,baselines}, targets, paths, revert, found, now, torn,
 *       step_whole} -> {step, token, writes}.   (_checkout_move) */
json_t *w25_updatecmdp_22338d_checkout_move(const json_t *req);

/* req: {token, baseline_index, found, now, torn, step_whole} ->
 * {token, baseline, writes}.   (_settle_checkout_move) */
json_t *w25_updatecmdp_22338d_settle(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
