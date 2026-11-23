#ifndef NOC_CLARIFYLOCK_H
#define NOC_CLARIFYLOCK_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: clarifylock_lock          @ gateway/session_clarify_lock.py:clarify_lock
 * PoP: clarifylock_coerce_answer @ gateway/session_clarify_lock.py:clarify_lock
 *
 * Faithful ports of the owner-side clarify.lock verb. The runtime collaborators
 * (connection, session_server_requests.lock_answer/may_settle) are injected as
 * data:
 *   params       - JSON object; request_id and question_id must be non-empty strings.
 *   remaining    - scripted lock_answer() result (JSON null => the wait already
 *                  ended -> status 'expired'; otherwise an int -> status 'ok').
 *   lock_raises  - inject lock_answer()'s ValueError -> AuthorityRefusal('invalid_params').
 *   answer       - for coerce_answer: absent => '' (the Python default), JSON null
 *                  => None, string => itself, other => json.dumps(..., ensure_ascii=False).
 *
 * lock() returns a NEWLY allocated json_t ({"status":...} or a {"__raise__":...}
 * refusal). coerce_answer() returns a NEWLY allocated json_t (string or null).
 * Callers own and must json_free() the result.
 */
struct json_t *clarifylock_lock(const struct json_t *params, const struct json_t *remaining,
                                int lock_raises);

struct json_t *clarifylock_coerce_answer(const struct json_t *answer);

#ifdef __cplusplus
}
#endif

#endif /* NOC_CLARIFYLOCK_H */
