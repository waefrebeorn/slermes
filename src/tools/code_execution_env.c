/*
 * code_execution_env.c — C port of upstream Hermes tools/code_execution_env.py.
 *
 * PoP: w11_codeexecut_132ad4_is_secret @ tools/code_execution_env.py:is_secret_env_name
 * PoP: w11_codeexecut_132ad4_cache_probe @ tools/code_execution_env.py:_cache_probe_result
 * PoP: w11_codeexecut_132ad4_scrub @ tools/code_execution_env.py:_scrub_child_env
 *
 * The env-passthrough and delegated-child collaborators (heavy quarry imports)
 * are injected as data seams from the fixture instead of mutating the process
 * environment: Windows CRT setenv(name,"",1) UNSETS a variable while Python
 * os.environ[name]="" keeps it.
 */
#include "noc_w11_codeexecut_132ad4.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_CACHE_MAX 32
#define ENV_MARKER "HERMES_DELEGATED_CHILD_CONTEXT"

static const char *const SECRET_SUBSTRINGS[] = {
    "KEY", "TOKEN", "SECRET", "PASSWORD", "CREDENTIAL", "PASSWD", "AUTH", "DSN",
    "WEBHOOK", "CREDS", "BEARER", "APIKEY", "COOKIE",
};
static const size_t N_SECRETS = sizeof(SECRET_SUBSTRINGS) / sizeof(SECRET_SUBSTRINGS[0]);

static const char *const SAFE_PREFIXES[] = {
    "PATH", "HOME", "USER", "LANG", "LC_", "TERM", "TMPDIR", "TMP", "TEMP", "SHELL",
    "LOGNAME", "XDG_", "PYTHONPATH", "VIRTUAL_ENV", "CONDA",
};
static const size_t N_SAFE = sizeof(SAFE_PREFIXES) / sizeof(SAFE_PREFIXES[0]);

static const char *const HERMES_ALLOWED[] = {
    "HERMES_HOME", "HERMES_PROFILE", "HERMES_CONFIG", "HERMES_ENV", ENV_MARKER,
};
static const size_t N_HALLOWED = sizeof(HERMES_ALLOWED) / sizeof(HERMES_ALLOWED[0]);

static const char *const WIN_ESSENTIAL[] = {
    "SYSTEMROOT", "SYSTEMDRIVE", "WINDIR", "COMSPEC", "PATHEXT", "OS",
    "PROCESSOR_ARCHITECTURE", "NUMBER_OF_PROCESSORS", "PUBLIC", "ALLUSERSPROFILE",
    "PROGRAMDATA", "PROGRAMFILES", "PROGRAMFILES(X86)", "PROGRAMW6432",
    "APPDATA", "LOCALAPPDATA", "USERPROFILE", "USERDOMAIN", "USERNAME",
    "HOMEDRIVE", "HOMEPATH", "COMPUTERNAME",
};
static const size_t N_WIN = sizeof(WIN_ESSENTIAL) / sizeof(WIN_ESSENTIAL[0]);

static const char *const KANBAN_KEYS[] = {
    "HERMES_KANBAN_TASK", "HERMES_KANBAN_RUN_ID", "HERMES_KANBAN_CLAIM_LOCK",
    "HERMES_KANBAN_GOAL_MODE", "HERMES_KANBAN_GOAL_MAX_TURNS",
};
static const size_t N_KANBAN = sizeof(KANBAN_KEYS) / sizeof(KANBAN_KEYS[0]);

static void upper_copy(const char *s, char *out, size_t sz) {
    size_t i = 0;
    if (!s) { out[0] = '\0'; return; }
    for (; s[i] && i + 1 < sz; i++) out[i] = (char)toupper((unsigned char)s[i]);
    out[i] = '\0';
}

static int in_list(const char *const *list, size_t n, const char *s) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(list[i], s)) return 1;
    return 0;
}

static int starts_with_any(const char *s, const char *const *prefs, size_t n) {
    if (!s) return 0;
    for (size_t i = 0; i < n; i++) if (!strncmp(s, prefs[i], strlen(prefs[i]))) return 1;
    return 0;
}

static int array_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

int w11_codeexecut_132ad4_is_secret(const char *name) {
    if (!name) return 0;
    char up[4096];
    upper_copy(name, up, sizeof(up));
    for (size_t i = 0; i < N_SECRETS; i++)
        if (strstr(up, SECRET_SUBSTRINGS[i])) return 1;
    return 0;
}

json_t *w11_codeexecut_132ad4_cache_probe(const json_t *cache, const char *key, json_t *value) {
    json_t *out = json_object();
    if (cache && cache->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(cache); i++) {
            const char *k = json_object_get_key_at(cache, i);
            json_set(out, k, json_copy(json_object_get_at(cache, i)));
        }
    }
    /* Python: if len(cache) >= MAX: pop first (FIFO), then cache[key] = value.
     * json_set replaces an existing key in place (no reorder), matching dict. */
    if (json_object_size(out) >= PROBE_CACHE_MAX && json_object_size(out) > 0) {
        const char *first = json_object_get_key_at(out, 0);
        if (first) {
            char *copy = strdup(first); /* copy BEFORE json_obj_del */
            json_obj_del(out, copy);
            free(copy);
        }
    }
    json_set(out, key ? key : "", value ? value : json_null());
    return out;
}

/* Faithful seam of agent.delegation_context.delegated_child_subprocess_env,
 * with the process-context / fence-root inputs injected from the fixture. */
static json_t *delegated_transform(const json_t *env, int delegated, const char *os_task,
                                   const char *fence_root) {
    if (!env || env->type != JSON_OBJECT) return json_object();
    int forced = delegated || (os_task && *os_task);
    if (!forced) {
        for (size_t i = 0; i < json_object_size(env); i++) {
            const char *k = json_object_get_key_at(env, i);
            json_t *v = json_object_get_at(env, i);
            if (v && v->type == JSON_STRING && v->str_val && *v->str_val &&
                (!strcmp(k, "HERMES_KANBAN_TASK") || !strcmp(k, ENV_MARKER))) {
                forced = 1;
                break;
            }
        }
    }
    if (!forced) return json_copy(env);

    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(env); i++) {
        const char *k = json_object_get_key_at(env, i);
        json_t *v = json_object_get_at(env, i);
        if (!k) continue;
        if (in_list(KANBAN_KEYS, N_KANBAN, k)) continue;
        if (!strcmp(k, "HERMES_SESSION_SOURCE") && v && v->type == JSON_STRING &&
            v->str_val && !strcmp(v->str_val, "kanban")) continue;
        json_set(out, k, json_copy(v));
    }
    const char *inherited = "";
    json_t *mk = json_obj_get(env, ENV_MARKER);
    if (mk && mk->type == JSON_STRING && mk->str_val) inherited = mk->str_val;
    char *chosen = (inherited[0] && strcmp(inherited, "1")) ? strdup(inherited)
                                                            : strdup(fence_root ? fence_root : "1");
    json_set(out, ENV_MARKER, json_string(chosen)); /* json_set copies the value */
    free(chosen);
    return out;
}

json_t *w11_codeexecut_132ad4_scrub(const json_t *source_env, const json_t *passthrough,
                                    const json_t *resolved, const json_t *scoped_additions,
                                    const json_t *toolchain, int is_windows, int delegated,
                                    const char *os_task, const char *fence_root) {
    json_t *scrubbed = json_object();

    if (source_env && source_env->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(source_env); i++) {
            const char *k = json_object_get_key_at(source_env, i);
            json_t *v = json_object_get_at(source_env, i);
            if (!k) continue;
            if (array_has(passthrough, k)) {
                json_t *rv = json_obj_get(resolved, k);
                if (rv && rv->type != JSON_NULL) json_set(scrubbed, k, json_copy(rv));
                continue;
            }
            char up[4096];
            upper_copy(k, up, sizeof(up));
            if (strstr(up, "KEY") || strstr(up, "TOKEN") || strstr(up, "SECRET") ||
                strstr(up, "PASSWORD") || strstr(up, "CREDENTIAL") || strstr(up, "PASSWD") ||
                strstr(up, "AUTH") || strstr(up, "DSN") || strstr(up, "WEBHOOK") ||
                strstr(up, "CREDS") || strstr(up, "BEARER") || strstr(up, "APIKEY") ||
                strstr(up, "COOKIE"))
                continue; /* is_secret_env_name */
            int safe = starts_with_any(k, SAFE_PREFIXES, N_SAFE) ||
                       array_has(toolchain, k) || in_list(HERMES_ALLOWED, N_HALLOWED, k) ||
                       (is_windows && in_list(WIN_ESSENTIAL, N_WIN, up));
            if (safe) json_set(scrubbed, k, json_copy(v));
            /* else if k.startswith("HERMES_"): logged drop (no output effect) */
        }
    }

    if (scoped_additions && scoped_additions->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(scoped_additions); i++) {
            const char *k = json_object_get_key_at(scoped_additions, i);
            if (k && array_has(passthrough, k))
                json_set(scrubbed, k, json_copy(json_object_get_at(scoped_additions, i)));
        }
    }

    json_t *scoped = delegated_transform(source_env, delegated, os_task, fence_root);
    json_t *mk = json_obj_get(scoped, ENV_MARKER);
    if (mk && mk->type == JSON_STRING && mk->str_val && *mk->str_val) {
        const char *keys[3] = {ENV_MARKER, "HERMES_KANBAN_DB", "HERMES_KANBAN_BOARD"};
        for (int i = 0; i < 3; i++) {
            json_t *sv = json_obj_get(scoped, keys[i]);
            if (sv) {
                char *vbuf = (sv->type == JSON_STRING && sv->str_val) ? strdup(sv->str_val) : NULL;
                if (vbuf) { json_set(scrubbed, keys[i], json_string(vbuf)); free(vbuf); }
                else json_set(scrubbed, keys[i], json_copy(sv));
            }
        }
    }
    json_free(scoped);

    json_t *result = delegated_transform(scrubbed, delegated, os_task, fence_root);
    json_free(scrubbed);
    return result;
}
