/*
 * profile_identity.c - real C11 port of the pure rekey decision helpers in upstream Hermes
 * persistence/sqlite/housekeeping/profile_identity.py. The repository's SQL _do bodies
 * (sqlite_master probes, UPDATE/rowcount settlement) stay Python-side; every decision the
 * transaction makes about a key/entry is here.
 * PoP: w19_profileide_35a8d2_moved_key @ persistence/sqlite/housekeeping/profile_identity.py:_moved_key
 * PoP: w19_profileide_35a8d2_rekey_entry @ persistence/sqlite/housekeeping/profile_identity.py:_rekey_entry
 * PoP: w19_profileide_35a8d2_rekey_allowed @ persistence/sqlite/housekeeping/profile_identity.py:rekey_profile
 */
#include "noc_w19_profileide_35a8d2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int is_py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace. NULL stays NULL. */
static char *pystrip(const char *s) {
    const char *a;
    const char *b;
    char *out;
    size_t n;
    if (!s) return NULL;
    a = s;
    while (*a && is_py_space(*a)) a++;
    b = a + strlen(a);
    while (b > a && is_py_space(b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static char *ns_join(const char *name) {
    /* "agent:" + name + ":" */
    size_t n = strlen(name ? name : "") + 8;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    snprintf(out, n + 1, "agent:%s:", name ? name : "");
    return out;
}

char *w19_profileide_35a8d2_moved_key(const json_t *key, const char *old_name, const char *new_name) {
    char *old_ns;
    char *new_ns;
    char *out;
    size_t ns_len;
    const char *k;
    if (!key || key->type != JSON_STRING || !key->str_val) return NULL;
    k = key->str_val;
    old_ns = ns_join(old_name);
    if (!old_ns) return NULL;
    ns_len = strlen(old_ns);
    if (strncmp(k, old_ns, ns_len) != 0) { free(old_ns); return NULL; }
    new_ns = ns_join(new_name);
    free(old_ns);
    if (!new_ns) return NULL;
    out = (char *)malloc(strlen(new_ns) + strlen(k + ns_len) + 1);
    if (!out) { free(new_ns); return NULL; }
    strcpy(out, new_ns);
    strcat(out, k + ns_len);
    free(new_ns);
    return out;
}

int w19_profileide_35a8d2_rekey_allowed(const char *old_name, const char *new_name) {
    char *old_s = pystrip(old_name ? old_name : "");
    char *new_s = pystrip(new_name ? new_name : "");
    int allowed = 0;
    if (old_s && new_s && old_s[0] && new_s[0] && strcmp(old_s, new_s) != 0) allowed = 1;
    free(old_s);
    free(new_s);
    return allowed;
}

json_t *w19_profileide_35a8d2_rekey_entry(const json_t *entry, const char *old_name, const char *new_name) {
    json_t *out = json_object();
    json_t *copy = entry ? json_copy(entry) : NULL;
    int changed = 0;

    if (!out) { json_free(copy); return NULL; }

    if (copy && copy->type == JSON_OBJECT) {
        json_t *sk = json_obj_get(copy, "session_key");
        char *moved = w19_profileide_35a8d2_moved_key(sk, old_name, new_name);
        if (moved) {
            json_set(copy, "session_key", json_string(moved));
            free(moved);
            changed = 1;
        }
        {
            json_t *origin = json_obj_get(copy, "origin");
            if (origin && origin->type == JSON_OBJECT) {
                json_t *prof = json_obj_get(origin, "profile");
                if (prof && prof->type == JSON_STRING && prof->str_val
                    && !strcmp(prof->str_val, old_name ? old_name : "")) {
                    json_set(origin, "profile", json_string(new_name ? new_name : ""));
                    changed = 1;
                }
            }
        }
    }

    json_set(out, "changed", json_bool(changed ? 1 : 0));
    json_set(out, "entry", copy ? copy : json_null());
    return out;
}
