/*
 * session_client.c — C port of upstream Hermes gateway/session_client.py
 * (gate w8_sessionclie_c302cf).
 *
 * PoP: sessionclie_client_problem @ gateway/session_client.py:client_problem
 * PoP: sessionclie_policy_client @ gateway/session_client.py:policy_client
 */
#include "noc_w8_sessionclie_c302cf.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

#define ERR_SHAPE "must be an object with a name and an optional version"
#define ERR_NAME  "name must be 1-64 characters of a-z, 0-9, \".\", \"_\" or \"-\", starting with a letter or digit"
#define ERR_VER   "version must be 1-64 printable ASCII characters without spaces"

/* re.fullmatch(r'[a-z0-9][a-z0-9._-]{0,63}') */
static int valid_name(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    unsigned char c = (unsigned char)s[0];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    for (size_t i = 1; i < n; i++) {
        c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* re.fullmatch(r'[!-~]{1,64}') */
static int valid_version(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x21 || c > 0x7e) return 0;
    }
    return 1;
}

json_t *sessionclie_client_problem(const json_t *value) {
    if (!json_is_object(value)) return json_string(ERR_SHAPE);
    int has_name = 0;
    size_t n = json_object_size(value);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(value, i);
        if (!k) continue;
        if (!strcmp(k, "name")) has_name = 1;
        else if (strcmp(k, "version")) return json_string(ERR_SHAPE);
    }
    if (!has_name) return json_string(ERR_SHAPE);
    json_t *nv = json_obj_get(value, "name");
    if (!json_is_string(nv) || !valid_name(nv->str_val)) return json_string(ERR_NAME);
    json_t *vv = json_obj_get(value, "version");
    if (vv && (!json_is_string(vv) || !valid_version(vv->str_val)))
        return json_string(ERR_VER);
    return json_null();
}

json_t *sessionclie_policy_client(const json_t *policy) {
    if (!policy || policy->type == JSON_NULL || policy->type != JSON_STRING)
        return json_null();
    json_t *doc = json_parse(policy->str_val, NULL);
    if (!doc || !json_is_object(doc)) {
        json_free(doc);
        return json_null();
    }
    json_t *v = json_obj_get(doc, "client");
    json_t *out = NULL;
    if (v) {
        json_t *problem = sessionclie_client_problem(v);
        if (problem && problem->type == JSON_NULL) out = json_copy(v);
        json_free(problem);
    }
    json_free(doc);
    return out ? out : json_null();
}
