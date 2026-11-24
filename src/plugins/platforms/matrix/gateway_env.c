/*
 * gateway_env.c -- real C11 port of upstream Hermes
 *   plugins/platforms/matrix/gateway_env.py.
 *
 * PoP: w22_gatewayenv_6bc7a3_e2ee @ plugins/platforms/matrix/gateway_env.py:_e2ee
 * PoP: w22_gatewayenv_6bc7a3_authz @ plugins/platforms/matrix/gateway_env.py:AUTHZ
 * PoP: w22_gatewayenv_6bc7a3_enable_credentials @ plugins/platforms/matrix/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * PoP: w22_gatewayenv_6bc7a3_env_overrides @ plugins/platforms/matrix/gateway_env.py:ENV_OVERRIDES
 *
 * The declaration (EnvCred / home_step) is carried as data; applying a step to
 * a live GatewayConfig (enable_from_env, env_home_channel) stays upstream
 * because it mutates a loaded config. _e2ee is ported end to end with the
 * environment injected instead of read from the process.
 */
#include "hermes_json.h"
#include "noc_w22_gatewayenv_6bc7a3.h"

#include <stdlib.h>
#include <string.h>

static char *ge6bc7a3_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* `getenv(name)` of the injected mapping: NULL when unset (Python's ""). */
static const char *ge6bc7a3_get(const json_t *env, const char *name) {
    const json_t *v = json_obj_get(env, name);
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

static int ge6bc7a3_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip().lower() for the ASCII env alphabet; malloc'd, never NULL on success. */
static char *ge6bc7a3_normalize(const char *s) {
    size_t n, i;
    char *out;
    if (!s) return ge6bc7a3_dup("");
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    {
        size_t start = 0, end = n;
        while (start < end && ge6bc7a3_is_space((unsigned char)out[start])) start++;
        while (end > start && ge6bc7a3_is_space((unsigned char)out[end - 1])) end--;
        if (start) memmove(out, out + start, end - start);
        out[end - start] = '\0';
        n = end - start;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)out[i];
        if (c >= 'A' && c <= 'Z') out[i] = (char)(c - 'A' + 'a');
    }
    return out;
}

/* is_truthy_value(getenv(...)) -- the shared {"1","true","yes","on"} set. */
static int ge6bc7a3_env_truthy(const json_t *env, const char *name) {
    char *norm = ge6bc7a3_normalize(ge6bc7a3_get(env, name));
    int hit;
    if (!norm) return 0;
    hit = !strcmp(norm, "1") || !strcmp(norm, "true") ||
          !strcmp(norm, "yes") || !strcmp(norm, "on");
    free(norm);
    return hit;
}

json_t *w22_gatewayenv_6bc7a3_e2ee(const json_t *env) {
    static const char *const E2EE_MODES[] = {
        "required", "require", "optional", "prefer", "preferred", NULL
    };
    json_t *extra = json_object();
    char *mode = ge6bc7a3_normalize(ge6bc7a3_get(env, "MATRIX_E2EE_MODE"));
    const char *device;
    int encryption = 0;
    int i;

    /* mode in ("required", "require", "optional", "prefer", "preferred") */
    for (i = 0; mode && E2EE_MODES[i]; i++) {
        if (!strcmp(mode, E2EE_MODES[i])) { encryption = 1; break; }
    }
    if (!encryption && ge6bc7a3_env_truthy(env, "MATRIX_ENCRYPTION")) encryption = 1;
    json_set(extra, "encryption", json_bool(encryption));

    /* if mode: extra["e2ee_mode"] = mode */
    if (mode && mode[0]) json_set(extra, "e2ee_mode", json_string(mode));
    free(mode);

    /* env_extras(extra, (("device_id", "MATRIX_DEVICE_ID"),)) -- truthy value only. */
    device = ge6bc7a3_get(env, "MATRIX_DEVICE_ID");
    if (device && device[0]) json_set(extra, "device_id", json_string(device));
    return extra;
}

json_t *w22_gatewayenv_6bc7a3_authz(void) {
    json_t *out = json_object();
    json_set(out, "allowed_users_env", json_string("MATRIX_ALLOWED_USERS"));
    json_set(out, "allow_all_env", json_string("MATRIX_ALLOW_ALL_USERS"));
    json_set(out, "allowlist_suppresses_pairing", json_bool(1));
    return out;
}

json_t *w22_gatewayenv_6bc7a3_enable_credentials(void) {
    json_t *out = json_array();
    json_append(out, json_string("MATRIX_ACCESS_TOKEN"));
    json_append(out, json_string("MATRIX_PASSWORD"));
    return out;
}

/* [[a, b], ...] from parallel string arrays. */
static json_t *ge6bc7a3_pairs(const char *const *flat, size_t rows) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < rows; i++) {
        json_t *row = json_array();
        json_append(row, json_string(flat[i * 2]));
        json_append(row, json_string(flat[i * 2 + 1]));
        json_append(out, row);
    }
    return out;
}

json_t *w22_gatewayenv_6bc7a3_env_overrides(void) {
    static const char *const GE6BC7A3_CREDS[] = {"MATRIX_ACCESS_TOKEN", "MATRIX_PASSWORD"};
    static const char *const GE6BC7A3_FIXED[] = {"homeserver", "MATRIX_HOMESERVER"};
    static const char *const GE6BC7A3_OPTIONAL[] = {
        "user_id", "MATRIX_USER_ID",
        "password", "MATRIX_PASSWORD"
    };
    static const char *const GE6BC7A3_WARN[] = {
        "MATRIX_HOMESERVER",
        "MATRIX_ACCESS_TOKEN/MATRIX_PASSWORD set but MATRIX_HOMESERVER is missing"
    };
    json_t *out = json_array();
    json_t *cred = json_object();
    json_t *home = json_object();
    json_t *creds_row = json_array();
    json_t *warn = json_array();

    json_append(creds_row, json_string(GE6BC7A3_CREDS[0]));
    json_append(creds_row, json_string(GE6BC7A3_CREDS[1]));

    json_set(cred, "kind", json_string("EnvCred"));
    json_set(cred, "platform", json_string("matrix"));
    {
        json_t *rows = json_array();
        json_append(rows, creds_row);
        json_set(cred, "creds", rows);
    }
    json_set(cred, "token", json_string("MATRIX_ACCESS_TOKEN"));
    json_set(cred, "fixed", ge6bc7a3_pairs(GE6BC7A3_FIXED, 1));
    json_set(cred, "optional", ge6bc7a3_pairs(GE6BC7A3_OPTIONAL, 2));
    json_set(cred, "optional_stripped", json_array());
    json_append(warn, json_string(GE6BC7A3_WARN[0]));
    json_append(warn, json_string(GE6BC7A3_WARN[1]));
    json_set(cred, "warn_missing", warn);
    json_set(cred, "then", json_string("_e2ee"));
    json_set(cred, "home", json_null());
    json_set(cred, "home_strip", json_bool(0));
    json_append(out, cred);

    json_set(home, "kind", json_string("home_step"));
    json_set(home, "platform", json_string("matrix"));
    json_set(home, "env_base", json_string("MATRIX_HOME_ROOM"));
    json_set(home, "strip", json_bool(0));
    json_append(out, home);
    return out;
}
