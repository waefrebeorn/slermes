/*
 * noc_w31_gatewayses_1bd734_p3.h - C surface for the launch-policy half of
 * upstream hermes_cli/gateway_session_store.py (gate w31_gatewayses_1bd734_p3).
 *
 * The explicit launch flags as ``session.create`` policy -- identity-filtered
 * (0 is a value, False is not), toolsets joined then re-split, skills folded to
 * one deduplicated list, main's --run-budget / -v added, this shell's env
 * opt-ins applied last -- and the resume refusal when a classic-only flag
 * differs from the session's frozen launch.  The environment read and
 * gateway_chat.check_resume_policy stay in the caller.
 */
#ifndef NOC_W31_GATEWAYSES_1BD734_P3_H
#define NOC_W31_GATEWAYSES_1BD734_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _classic_only_policy(launch) -> {"run_budget": <float>, "verbose": true}
 * (each key only when the flag was given). */
json_t *w31_gatewayses_1bd734_p3_classic_only_policy(const json_t *launch);

/* _creation_policy(launch, env) -> the session.create policy object.  ``env``
 * holds this shell's HERMES_* launch opt-ins, its five values read with
 * utils.is_truthy_value. */
json_t *w31_gatewayses_1bd734_p3_creation_policy(const json_t *launch, const json_t *env);

/* _check_resume_launch's classic-only tail -> {"error": <exact
 * GatewayClientError text>} or {"error": null} when the flags agree. */
json_t *w31_gatewayses_1bd734_p3_resume_classic_differs(const json_t *launch,
                                                        const json_t *snapshot);

#ifdef __cplusplus
}
#endif
#endif
