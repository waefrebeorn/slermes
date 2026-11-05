#ifndef PORT_HOSTED_ROOM_EXECUTION_POLICY_H
#define PORT_HOSTED_ROOM_EXECUTION_POLICY_H

#include "hermes_json.h"

/* Port of Python: gateway/hosted_room_execution_policy.py
 *
 * The immutable target policy a RoomLink member turn runs under: validation,
 * canonical digest, and the as_mapping round trip.
 *
 * Not ported here: _served_profile_scope / execution_policy_mapping, which
 * read the served profile's own gateway config and approval mode through
 * hermes_cli.config / tools_config / gateway.run. That is live runtime state,
 * not a pure transform.
 *
 * Limitation: libjson stores every number as a double, so a document written
 * with `"max_iterations": 1.0` is indistinguishable from `1`. Python's
 * bounded_int rejects the float form; this port accepts it.
 */

#define HEP_POLICY_VERSION   1
#define HEP_MAX_TOOLSETS     128
#define HEP_MAX_ITERATIONS   9007199254740991LL   /* 2^53 - 1 */

typedef struct {
    int         version;
    char       *target_profile;
    char      **enabled_toolsets;   /* sorted, unique, includes "bot_room" */
    int         n_toolsets;
    char       *approval_mode;      /* manual | smart | off */
    long long   max_iterations;
    char       *policy_digest;      /* lowercase hex sha256 */
} hep_policy_t;

/* sha256 over Python's compact_json(unsigned policy) — sorted keys, no
 * separators, ASCII. Returns malloc'd 64-hex string. */
char *hep_policy_digest(const char *target_profile,
                        const char *const *toolsets, int n_toolsets,
                        const char *approval_mode, long long max_iterations);

/* Validate a policy document. Returns 0 and fills *out on success; returns -1
 * and stores a malloc'd message in *err on failure. */
int hep_policy_from_mapping(const char *json, hep_policy_t *out, char **err);

/* Round trip: the same mapping the Python dataclass produces. */
json_t *hep_policy_as_mapping(const hep_policy_t *policy);

void hep_policy_free(hep_policy_t *policy);

#endif /* PORT_HOSTED_ROOM_EXECUTION_POLICY_H */
