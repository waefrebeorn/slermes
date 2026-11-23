/*
 * session_worker.c — C port of upstream Hermes gateway/session_worker.py.
 *
 * PoP: w8_sessionwork_e89180_scope_ok @ gateway/session_worker.py:worker_request
 * PoP: w8_sessionwork_e89180_claim_check @ gateway/session_worker.py:_claim
 *
 * Faithful port of the two pure decision points: the exact _SCOPE|extra param
 * key-set comparison and the admission validation prefix (profile match,
 * capabilities, strict Python types for pid/birth/secret, pid>0, pid !=
 * os.getpid(), secret length 16..256). The `ok` path is where _claim proceeds
 * to the psutil/persistence/fingerprint machinery, which the oracle stubs.
 */
#include "noc_w8_sessionwork_e89180.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static int in_list(const char *key, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (list[i] && key && !strcmp(key, list[i])) return 1;
    return 0;
}

int w8_sessionwork_e89180_scope_ok(const char *operation, const struct json_t *param_keys) {
    static const char *const base[] = {
        "profile_id", "session_id", "execution_id", "generation", "pid", "birth", "secret"
    };
    static const char *const reg[] = {"kind"};
    static const char *const persist[] = {"epoch", "sequence", "operation", "payload"};
    const char *const *extra = NULL;
    size_t extra_n = 0;
    if (!strcmp(operation, "register")) { extra = reg; extra_n = 1; }
    else if (!strcmp(operation, "adopt")) { extra = NULL; extra_n = 0; }
    else if (!strcmp(operation, "persist")) { extra = persist; extra_n = 4; }
    else return 0; /* Python: KeyError on an unknown operation */

    size_t want = sizeof(base) / sizeof(base[0]) + extra_n;
    size_t got = (param_keys && param_keys->type == JSON_ARRAY) ? json_len(param_keys) : 0;
    if (got != want) return 0;
    for (size_t i = 0; i < got; i++) {
        const char *k = json_string_value(json_get(param_keys, i));
        if (!k) return 0;
        if (!in_list(k, base, sizeof(base) / sizeof(base[0])) && !in_list(k, extra, extra_n)) return 0;
    }
    return 1;
}

const char *w8_sessionwork_e89180_claim_check(const struct json_t *ctx) {
    const char *profile = json_get_str(ctx, "profile_id", "");
    const char *authz_profile = json_get_str(ctx, "authority_profile_id", "");
    const char *actor_profile = json_get_str(ctx, "actor_profile_id", "");
    if (strcmp(profile, authz_profile) || strcmp(actor_profile, authz_profile))
        return "profile_mismatch";
    if (!json_get_bool(ctx, "has_capabilities", false))
        return "permission_denied";

    const char *pid_type = json_get_str(ctx, "pid_type", "int");
    const char *birth_type = json_get_str(ctx, "birth_type", "int");
    const char *secret_type = json_get_str(ctx, "secret_type", "str");
    double pid = json_get_num(ctx, "pid", 0.0);
    double own_pid = json_get_num(ctx, "own_pid", 0.0);
    const char *secret = json_get_str(ctx, "secret", "");
    size_t slen = secret ? strlen(secret) : 0;

    /* type(pid) is not int  or  pid <= 0  or  pid == os.getpid() */
    if (strcmp(pid_type, "int") || pid <= 0.0 || pid == own_pid)
        return "invalid_params";
    /* type(birth) not in (int, float) */
    if (strcmp(birth_type, "int") && strcmp(birth_type, "float"))
        return "invalid_params";
    /* not isinstance(secret, str)  or  not 16 <= len(secret) <= 256 */
    if (strcmp(secret_type, "str") || slen < 16 || slen > 256)
        return "invalid_params";
    return "ok";
}
