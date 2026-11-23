#ifndef NOC_W8_SESSIONWORK_E89180_H
#define NOC_W8_SESSIONWORK_E89180_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_sessionwork_e89180_scope_ok @ gateway/session_worker.py:worker_request
 * Mirrors `set(params) == _SCOPE | extra` for the given operation. */
int w8_sessionwork_e89180_scope_ok(const char *operation, const struct json_t *param_keys);

/* PoP: w8_sessionwork_e89180_claim_check @ gateway/session_worker.py:_claim
 * The validation prefix of _claim, returning one of:
 *   "profile_mismatch" | "permission_denied" | "invalid_params" | "ok".
 * Python's dynamic pid/birth/secret types are injected explicitly via
 * pid_type/birth_type/secret_type so the strict `type(x) is int` checks are
 * reproducible across the C/JSON boundary. */
const char *w8_sessionwork_e89180_claim_check(const struct json_t *ctx);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SESSIONWORK_E89180_H */
