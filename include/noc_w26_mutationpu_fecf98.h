/*
 * noc_w26_mutationpu_fecf98.h - C surface for upstream
 * persistence/sqlite/authority/_mutation_publish.py (gate w26_mutationpu_fecf98).
 *
 * The two value decisions a publish makes before it touches a connection: the
 * profile namespace encoded in a gateway session key, and the frozen policy a
 * branch child inherits (the parent's, with editor MCP servers replaced by the
 * child's own prepared selection, or refused).  The connections, the prepared
 * snapshot, the lineage rows, the archive/publish writers and the receipts stay
 * in the caller.
 *
 * A refusal is returned as {"__refusal": <wire code>} -- the header's stand-in
 * for raising AuthorityRefusal.
 */
#ifndef NOC_W26_MUTATIONPU_FECF98_H
#define NOC_W26_MUTATIONPU_FECF98_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* session_key: str|null -> the profile id, "default" for main, null when the
 * key is not an agent:<ns>: key. */
json_t *w26_mutationpu_fecf98_profile_from_session_key(const json_t *session_key);

/* policy, child, prepared -> the child's policy, or {"__refusal": code}. */
json_t *w26_mutationpu_fecf98_child_policy(const json_t *policy, const json_t *child,
                                           const json_t *prepared);

/* TITLE_SOURCE_USER / TITLE_SOURCE_DERIVED, as an object. */
json_t *w26_mutationpu_fecf98_constants(void);

#ifdef __cplusplus
}
#endif
#endif
