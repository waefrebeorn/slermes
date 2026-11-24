#ifndef NOC_W16_SESSIONTER_E62531_H
#define NOC_W16_SESSIONTER_E62531_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionter_e62531_local_backend_ex @ gateway/session_terminal_cwd.py:_local_backend
 * policy_json is the policy.terminal_json text (NULL = None policy, then the
 * profile's live TERMINAL_ENV is injected via env_value, the _tenv seam). */
int w16_sessionter_e62531_local_backend_ex(const char *policy_json, const char *env_value);

/* PoP: w16_sessionter_e62531_should_persist @ gateway/session_terminal_cwd.py:persist_turn_cwd
 * Pure branch predicate: `if not cwd or metadata.get(key) == cwd: return`.
 * NULL saved means the key is absent (Python None). */
int w16_sessionter_e62531_should_persist(const char *saved, const char *cwd);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONTER_E62531_H */
