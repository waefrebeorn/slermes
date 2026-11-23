/*
 * session_identity.c — faithful C11 port of gateway/session_identity.py.
 * Gate w10_sessionide_e2126e.
 *
 * PoP: w10_sessionide_e2126e_name @ gateway/session_identity.py:_name
 * PoP: w10_sessionide_e2126e_parse_authenticated_subject @ gateway/session_identity.py:parse_authenticated_subject
 * PoP: w10_sessionide_e2126e_authenticated_subject @ gateway/session_identity.py:authenticated_subject
 * PoP: w10_sessionide_e2126e_is_authenticated_identity @ gateway/session_identity.py:is_authenticated_identity
 */
#include "hermes_json.h"
#include "noc_w10_sessionide_e2126e.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_SUBJECT_PREFIX "auth:v1:"
#define INTERNAL_VALUE "server-internal"

static int ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() over ASCII whitespace; caller frees. */
static char *strip_dup(const char *s) {
    if (!s) return NULL;
    const char *a = s;
    while (*a && ws((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && ws((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

/* Python str() coercion for the JSON scalar values used in identity fields. */
static char *py_str(const json_t *v) {
    if (!v) return NULL;
    if (v->type == JSON_STRING) return v->str_val ? strdup(v->str_val) : strdup("");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        char buf[64];
        /* Integral values are fixture ints: Python str(int) has no decimal. */
        if (d == (double)(long long)d && d >= -9007199254740992.0 && d <= 9007199254740992.0)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
        return strdup(buf);
    }
    return strdup("");
}

json_t *w10_sessionide_e2126e_name(const struct json_t *value) {
    if (!value || value->type != JSON_STRING) return json_null();
    char *s = strip_dup(value->str_val);
    if (!s || !*s) { free(s); return json_null(); }
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w10_sessionide_e2126e_parse_authenticated_subject(const char *subject) {
    if (!subject || strncmp(subject, AUTH_SUBJECT_PREFIX, strlen(AUTH_SUBJECT_PREFIX)) != 0)
        return json_null();
    const char *rest = subject + strlen(AUTH_SUBJECT_PREFIX);
    json_t *parts = json_parse(rest, NULL);
    if (!parts || parts->type != JSON_ARRAY || parts->c.count != 3) {
        if (parts) json_free(parts);
        return json_null();
    }
    for (size_t i = 0; i < 3; i++) {
        if (!parts->c.items[i] || parts->c.items[i]->type != JSON_STRING) {
            json_free(parts);
            return json_null();
        }
    }
    return parts;
}

json_t *w10_sessionide_e2126e_authenticated_subject(const struct json_t *identity) {
    if (!identity || identity->type != JSON_OBJECT) identity = NULL;
    json_t *uid = identity ? json_obj_get(identity, "user_id") : NULL;
    char *subject = py_truthy(uid) ? py_str(uid) : strdup("unbound");
    if (!subject) subject = strdup("unbound");

    json_t *provider = identity ? json_obj_get(identity, "provider") : NULL;
    json_t *profile_id = identity ? json_obj_get(identity, "profile_id") : NULL;
    json_t *instance_id = identity ? json_obj_get(identity, "instance_id") : NULL;
    int local_branch = provider && provider->type == JSON_STRING && !strcmp(provider->str_val, "local")
        && py_truthy(profile_id) && py_truthy(instance_id)
        && json_obj_get(identity, "capabilities") != NULL;
    if (local_branch) {
        json_t *out = json_string(subject);
        free(subject);
        return out;
    }

    json_t *issuer = identity ? json_obj_get(identity, "issuer") : NULL;
    /* Python dumps the raw provider/issuer values (with `or ''` fallback); only
     * the subject is str()-coerced. Keep their JSON types rather than stringify. */
    json_t *arr = json_array();
    json_append(arr, py_truthy(provider) ? json_copy(provider) : json_string(""));
    json_append(arr, py_truthy(issuer) ? json_copy(issuer) : json_string(""));
    json_append(arr, json_string(subject));
    char *encoded = json_dumps(arr, 0);
    json_free(arr);
    free(subject);
    size_t n = strlen(AUTH_SUBJECT_PREFIX) + (encoded ? strlen(encoded) : 2) + 1;
    char *buf = (char *)malloc(n);
    if (buf) snprintf(buf, n, "%s%s", AUTH_SUBJECT_PREFIX, encoded ? encoded : "[]");
    free(encoded);
    json_t *out = json_string(buf ? buf : "");
    free(buf);
    return out;
}

json_t *w10_sessionide_e2126e_is_authenticated_identity(const struct json_t *identity) {
    if (!identity || identity->type != JSON_OBJECT) return json_bool(false);
    json_t *u = json_obj_get(identity, "user_id");
    json_t *p = json_obj_get(identity, "provider");
    if (!u || u->type != JSON_STRING || !u->str_val) return json_bool(false);
    if (!p || p->type != JSON_STRING || !p->str_val) return json_bool(false);
    char *us = strip_dup(u->str_val);
    char *ps = strip_dup(p->str_val);
    int ok = us && *us && ps && *ps
        && !(strcmp(u->str_val, INTERNAL_VALUE) == 0 && strcmp(p->str_val, INTERNAL_VALUE) == 0);
    free(us); free(ps);
    return json_bool(ok ? true : false);
}
