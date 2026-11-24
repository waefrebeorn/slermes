/*
 * noc_w33_sandboxcon_be5b73_p1.h - C surface for upstream
 * tools/environments/sandbox_conformance.py, part 1 (gate
 * w33_sandboxcon_be5b73_p1).
 *
 * The identities and the plan of a hosted-scope conformance run: which scope
 * an agent gets, the deterministic marker a check plants in that agent's data,
 * the sandbox cache key an operation is routed by, the ordered table of checks
 * that will be run, and the literal capability table the module is tuned with.
 *
 * Every seam (the recording provider and its environments, the tool registry,
 * bind_state, subprocess, the filesystem) stays in the caller; this file owns
 * only what the values decide.
 */
#ifndef NOC_W33_SANDBOXCON_BE5B73_P1_H
#define NOC_W33_SANDBOXCON_BE5B73_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of entries in the ordered check table. */
#define W33_SANDBOXCON_BE5B73_P1_CHECK_COUNT 10

/* req: {"agent": str, "tenant": str = "acme", "user": str = "u1"} ->
 * {"tenant", "agent", "actor_id", "owner_id", "can_administer": true}
 * (upstream hosted_scope_for: one Principal, actor and owner). */
json_t *w33_sandboxcon_be5b73_p1_hosted_scope(const json_t *req);

/* scope: {"tenant", "agent"} -> "mark" + first 12 hex of
 * uuid5(NAMESPACE_URL, "<tenant>/<agent>") (upstream _marker). */
json_t *w33_sandboxcon_be5b73_p1_marker(const json_t *scope);

/* scope: {"tenant", "agent"} -> "sandbox:<quote(tenant)>/<quote(agent)>"
 * with urllib's safe='' quoting (SandboxIdentity.key). */
json_t *w33_sandboxcon_be5b73_p1_identity_key(const json_t *scope);

/* The ordered conformance-check table: [{"index", "name", "params": [...],
 * "arity"}, ...] (upstream checks()). */
json_t *w33_sandboxcon_be5b73_p1_checks_table(void);

/* {"stub_backend", "persisted_mark", "metadata_calls": [...]} - the module
 * literals STUB_BACKEND, PERSISTED_MARK and _METADATA_CALLS. */
json_t *w33_sandboxcon_be5b73_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
