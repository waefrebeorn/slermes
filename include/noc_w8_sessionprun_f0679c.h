#ifndef NOC_W8_SESSIONPRUN_F0679C_H
#define NOC_W8_SESSIONPRUN_F0679C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_sessionprun_f0679c_filters @ gateway/session_prune.py:_filters
 * Validates the raw filter mapping against PRUNE_FILTER_NAMES | {'older_than_days'}
 * and scalar values. Returns {"ok":bool,"value":<dict|null>}. Caller frees. */
struct json_t *w8_sessionprun_f0679c_filters(const struct json_t *raw);

/* PoP: w8_sessionprun_f0679c_prune_guard @ gateway/session_prune.py:prune
 * Explicit seam for the pre-commit admission checks (native_owner, capabilities,
 * profile match, filters/never_active exclusivity, filter validation).
 * Returns a JSON string: "ok" | "permission_denied" | "profile_mismatch" | "invalid_params".
 * Caller frees. */
struct json_t *w8_sessionprun_f0679c_prune_guard(const struct json_t *native_owner,
    const struct json_t *capabilities, const struct json_t *actor_profile,
    const struct json_t *authority_profile, const struct json_t *params);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_SESSIONPRUN_F0679C_H */
