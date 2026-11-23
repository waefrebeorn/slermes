#ifndef NOC_SESSCOMPLETE_H
#define NOC_SESSCOMPLETE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: sesscomplete_complete @ gateway/session_complete_path.py:complete_path
 *
 * Faithful port of the validation and decision path of `complete_path`. The
 * runtime collaborators (connection.authority, session policy, the sidecar's
 * listing helper, authorize) are injected as data so the module's own logic is
 * fully reproduced:
 *   params           - JSON object; keys must be a subset of
 *                      {session_id, profile, word, cwd} and all values strings.
 *   session_id       - ref.session_id (empty => {"items": []}).
 *   sessions         - authority.sessions keys (JSON array of strings).
 *   policy_null      - policy_for_source(...) returned None.
 *   host_cwd         - terminal_uses_host_cwd(json.loads(policy.terminal_json)).
 *   items            - scripted server._path_completion_items(...) result.
 *   authorize_raise  - non-empty string => authorize() raises AuthorityRefusal(msg).
 *
 * Returns a NEWLY allocated json_t: {"items":[...]} on success, or
 * {"__raise__":"AuthorityRefusal","msg":<code>} for a refusal. Caller frees.
 */
struct json_t *sesscomplete_complete(const struct json_t *params, const char *session_id,
                                     const struct json_t *sessions, int policy_null,
                                     int host_cwd, const struct json_t *items,
                                     const char *authorize_raise);

#ifdef __cplusplus
}
#endif

#endif /* NOC_SESSCOMPLETE_H */
