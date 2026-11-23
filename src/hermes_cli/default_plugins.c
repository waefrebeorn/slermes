/*
 * default_plugins.c — C port of upstream Hermes hermes_cli/default_plugins.py.
 *
 * PoP: w13_defaultplu_f7fec3_attempt @ hermes_cli/default_plugins.py:_attempt
 * PoP: w13_defaultplu_f7fec3_fetch_policy @ hermes_cli/default_plugins.py:fetch_policy
 * PoP: w13_defaultplu_f7fec3_fetch_allowed @ hermes_cli/default_plugins.py:fetch_allowed
 *
 * Pure decision helpers: process env and YAML config are replaced by explicit
 * `env_value`, `security` and `readable` seams so no filesystem/process state is
 * touched. The `is True` identity check on allow_lazy_installs is mirrored.
 */
#include "noc_w13_defaultplu_f7fec3.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lower_strip(const char *s, char *out, size_t cap) {
    size_t n = s ? strlen(s) : 0, a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    size_t k = 0;
    for (size_t i = a; i < b && k + 1 < cap; i++)
        out[k++] = (char)tolower((unsigned char)s[i]);
    out[k] = '\0';
}

static const char *policy(const json_t *env_value, const json_t *security, int readable) {
    char env[512];
    lower_strip((env_value && env_value->type == JSON_STRING) ? env_value->str_val : "",
                env, sizeof env);
    if (!strcmp(env, "1") || !strcmp(env, "true") || !strcmp(env, "yes")) return "denied";
    if (!readable) return "unreadable";
    int allowed = 1; /* dict.get("allow_lazy_installs", True) */
    if (json_is_object(security)) {
        const json_t *v = json_obj_get(security, "allow_lazy_installs");
        if (v) allowed = (v->type == JSON_BOOL && v->bool_val) ? 1 : 0; /* `is True` */
    }
    return allowed ? "allowed" : "denied";
}

json_t *w13_defaultplu_f7fec3_attempt(const struct json_t *name) {
    const char *n = (name && name->type == JSON_STRING) ? name->str_val : "";
    size_t len = strlen(n ? n : "");
    char *buf = (char *)malloc(len + 16);
    if (!buf) return json_string("default-");
    memcpy(buf, "default-", 8);
    memcpy(buf + 8, n ? n : "", len);
    buf[8 + len] = '\0';
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *w13_defaultplu_f7fec3_fetch_policy(const struct json_t *env_value,
        const struct json_t *security, int readable) {
    return json_string(policy(env_value, security, readable));
}

json_t *w13_defaultplu_f7fec3_fetch_allowed(const struct json_t *env_value,
        const struct json_t *security, int readable) {
    return json_bool(!strcmp(policy(env_value, security, readable), "allowed"));
}
