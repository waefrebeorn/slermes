#ifndef NOC_W12_SESSIONMUT_83F0C3_H
#define NOC_W12_SESSIONMUT_83F0C3_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionmut_83f0c3_runtime_owns_authority @ gateway/session_mutations.py:_runtime_owns_authority
 * Faithful port of:
 *   runner = getattr(authority, "runner", None)
 *   registry = getattr(runner, "session_authorities", None)
 *   if registry is not None: return any(a is authority for a in registry)
 *   return getattr(runner, "session_authority", None) is authority
 * has_registry models "registry is not None"; registry_contains_authority models the
 * identity scan; session_authority_is_authority models the direct identity check.
 * No runner => registry is None and session_authority is None => 0. */
int w12_sessionmut_83f0c3_runtime_owns_authority(int has_registry,
                                                  int registry_contains_authority,
                                                  int session_authority_is_authority);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONMUT_83F0C3_H */
