/*
 * scheduler_worker_env.c — C home for upstream Hermes cron/scheduler_worker_env.py.
 *
 * PoP: w11_schedulerw_1873fb_pin_pythonpath @ cron/scheduler_worker_env.py:pin_hermes_tree_on_pythonpath
 *
 * The two environment probes (sysconfig purelib, the committed dependency
 * site-packages) are injected as explicit seams so the gate never mutates the
 * process environment or touches the filesystem.
 */
#include "noc_w11_schedulerw_1873fb.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

#define PATHSEP ';'

static void add_unique(char ***list, size_t *n, size_t *cap, const char *s) {
    for (size_t i = 0; i < *n; ++i)
        if (!strcmp((*list)[i], s)) return;
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *list = (char **)realloc(*list, *cap * sizeof(char *));
    }
    (*list)[(*n)++] = strdup(s);
}

struct json_t *w11_schedulerw_1873fb_pin_pythonpath(const struct json_t *env,
                                                     const char *root,
                                                     const char *installed_purelib,
                                                     const char *dependency) {
    json_t *out = json_object();

    /* worker_env is returned untouched when repo_root is the interpreter purelib. */
    if (env && env->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(env); ++i) {
            const char *k = json_object_get_key_at(env, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(env, i)));
        }
    }
    const char *r = root ? root : "";
    if (installed_purelib && !strcmp(installed_purelib, r))
        return out;

    char **parts = NULL;
    size_t n = 0, cap = 0;
    add_unique(&parts, &n, &cap, r);                 /* pinned checkout */
    if (dependency) add_unique(&parts, &n, &cap, dependency); /* pinned generation */
    /* existing = [e for e in worker_env.get("PYTHONPATH", "").split(os.pathsep) if e] */
    const char *pp = json_get_str(out, "PYTHONPATH", "");
    if (pp) {
        const char *cur = pp;
        while (*cur) {
            const char *end = strchr(cur, PATHSEP);
            size_t len = end ? (size_t)(end - cur) : strlen(cur);
            if (len) {
                char *tok = (char *)malloc(len + 1);
                memcpy(tok, cur, len);
                tok[len] = '\0';
                add_unique(&parts, &n, &cap, tok);
                free(tok);
            }
            if (!end) break;
            cur = end + 1;
        }
    }

    size_t total = 2; /* trailing NUL + room */
    for (size_t i = 0; i < n; ++i) total += strlen(parts[i]) + 1;
    char *joined = (char *)calloc(1, total);
    for (size_t i = 0; i < n; ++i) {
        if (i) strcat(joined, ";");
        strcat(joined, parts[i]);
    }
    json_set(out, "PYTHONPATH", json_string(joined));

    free(joined);
    for (size_t i = 0; i < n; ++i) free(parts[i]);
    free(parts);
    return out;
}
