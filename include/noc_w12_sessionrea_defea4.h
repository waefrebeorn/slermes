#ifndef NOC_W12_SESSIONREA_DEFEA4_H
#define NOC_W12_SESSIONREA_DEFEA4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionrea_defea4_guard @ gateway/session_readiness.py:readiness
 * Faithful port of readiness()'s guard clauses (the environment-independent part):
 *   'session:create' not in actor.capabilities or actor.profile_id != authority.profile_id
 *       -> "permission_denied"
 *   not isinstance(params, dict) / set(params) - allowed / non-str values
 *       -> "invalid_params"
 *   params.get('profile') truthy and _resolve_profile_dir(...).resolve() != Path(profile_id)
 *   (or the resolver raises HTTPException)
 *       -> "profile_mismatch"
 * Returns a malloc'd reason string on refusal, NULL when the guard allows the
 * runtime probe. Caller frees the returned string.
 * resolved_profile_dir / resolve_raises are the injected _resolve_profile_dir seam. */
char *w12_sessionrea_defea4_guard(const struct json_t *caps,
                                   const char *actor_profile_id,
                                   const char *authority_profile_id,
                                   const struct json_t *params,
                                   int runtime,
                                   const char *resolved_profile_dir,
                                   int resolve_raises);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONREA_DEFEA4_H */
