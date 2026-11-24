/*
 * processes.c - C home for hermes_cli/local_runtime/processes.py.
 * Faithful port of the module's pure helper: server_child_env. The Windows job-object and
 * Popen wrappers stay Python (live OS handles, no pure semantics to port).
 * PoP: w19_processes_bd7796_server_child_env @ hermes_cli/local_runtime/processes.py:server_child_env
 */
#include "noc_w19_processes_bd7796.h"
#include <ctype.h>
#include <string.h>

/* _CREDENTIAL_ENV_MARKERS = ("_API_KEY", "_TOKEN", "_SECRET", "PASSWORD", "_CREDENTIALS") */
static const char *const CREDENTIAL_ENV_MARKERS[] = {
    "_API_KEY", "_TOKEN", "_SECRET", "PASSWORD", "_CREDENTIALS", NULL
};

/* Python str.upper() for the ASCII keys an environment holds. */
static char *ascii_upper(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c < 0x80) ? (char)toupper(c) : (char)c;
    }
    out[n] = '\0';
    return out;
}

static int has_credential_marker(const char *key) {
    char *up = ascii_upper(key);
    int found = 0;
    if (!up) return 0;
    for (int i = 0; CREDENTIAL_ENV_MARKERS[i]; i++) {
        if (strstr(up, CREDENTIAL_ENV_MARKERS[i])) { found = 1; break; }
    }
    free(up);
    return found;
}

json_t *w19_processes_bd7796_server_child_env(json_t *base_env) {
    json_t *out = json_object();
    if (!base_env || base_env->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(base_env); i++) {
        const char *key = json_object_get_key_at(base_env, i);
        if (!key || has_credential_marker(key)) continue;
        json_set(out, key, json_copy(json_object_get_at(base_env, i)));
    }
    return out;
}
