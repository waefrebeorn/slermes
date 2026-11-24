/*
 * profile_identity.c - C home for persistence/housekeeping/profile_identity.py.
 * Faithful port of the identity contract's pure helpers: the key namespace, the
 * count-dict projection (_present, fields in dataclass order) and the collision
 * exception's fields. The SQL verbs themselves are a store transaction and stay Python.
 * PoP: w19_profileide_c2f0d9_profile_namespace @ persistence/housekeeping/profile_identity.py:profile_namespace
 * PoP: w19_profileide_c2f0d9_rekey_as_dict @ persistence/housekeeping/profile_identity.py:ProfileRekeyCounts.as_dict
 * PoP: w19_profileide_c2f0d9_purge_as_dict @ persistence/housekeeping/profile_identity.py:ProfilePurgeCounts.as_dict
 * PoP: w19_profileide_c2f0d9_collision @ persistence/housekeeping/profile_identity.py:ProfileIdentityCollision.__init__
 */
#include "noc_w19_profileide_c2f0d9.h"
#include <stdlib.h>
#include <string.h>

/* Field order of @dataclass ProfileRekeyCounts / ProfilePurgeCounts. */
static const char *const REKEY_FIELDS[] = {
    "sessions_profile_name",
    "gateway_heartbeats_profile",
    "sessions_session_key",
    "sessions_origin_json",
    "delivery_obligations_adapter_profile",
    "delivery_obligations_session_key",
    "telegram_dm_topic_mode_profile_name",
    "telegram_dm_topic_bindings_profile_name",
    "telegram_dm_topic_bindings_session_key",
    "gateway_routing",
    "api_binding_receipts",
    "local_policy_receipts",
};
static const char *const PURGE_FIELDS[] = {
    "gateway_routing",
    "gateway_heartbeats",
    "delivery_obligations",
    "telegram_dm_topic_mode",
    "telegram_dm_topic_bindings",
};

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over the ASCII whitespace set. */
static char *py_strip_dup(const char *s) {
    const char *a;
    const char *b;
    size_t n;
    char *out;
    if (!s) return dupstr("");
    a = s;
    while (*a && py_ws((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && py_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* ---- profile_namespace(profile): f"agent:{(profile or '').strip()}:" ---- */
json_t *w19_profileide_c2f0d9_profile_namespace(const char *profile) {
    char *stripped = py_strip_dup(profile ? profile : "");
    json_t *node;
    char *buf;
    if (!stripped) return json_null();
    buf = (char *)malloc(strlen(stripped) + 8);
    if (!buf) { free(stripped); return json_null(); }
    strcpy(buf, "agent:");
    strcat(buf, stripped);
    strcat(buf, ":");
    node = json_string(buf);
    free(buf);
    free(stripped);
    return node;
}

/* _present(counts): {f.name: value for f in fields(counts) if value is not None} */
static json_t *present(json_t *counts, const char *const *names, size_t n) {
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < n; i++) {
        json_t *v = (counts && counts->type == JSON_OBJECT) ? json_obj_get(counts, names[i]) : NULL;
        if (v && v->type != JSON_NULL) json_set(out, names[i], json_copy(v));
    }
    return out;
}

json_t *w19_profileide_c2f0d9_rekey_as_dict(json_t *counts) {
    return present(counts, REKEY_FIELDS, sizeof(REKEY_FIELDS) / sizeof(REKEY_FIELDS[0]));
}

json_t *w19_profileide_c2f0d9_purge_as_dict(json_t *counts) {
    return present(counts, PURGE_FIELDS, sizeof(PURGE_FIELDS) / sizeof(PURGE_FIELDS[0]));
}

/* ---- ProfileIdentityCollision(new_name, message) ---- */
json_t *w19_profileide_c2f0d9_collision(const char *new_name, const char *message) {
    json_t *out = json_object();
    json_set(out, "str", json_string(message ? message : ""));
    json_set(out, "entity", json_string("profile"));
    json_set(out, "entity_id", json_string(new_name ? new_name : ""));
    return out;
}

