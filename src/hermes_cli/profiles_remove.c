/*
 * profiles_remove.c — faithful C11 port of upstream Hermes
 * hermes_cli/profiles_remove.py (wave14 gate w14_profilesre_2c9e41).
 *
 * PoP: w14_profilesre_2c9e41_make_writable_actions @ hermes_cli/profiles_remove.py:_rmtree_make_writable
 * PoP: w14_profilesre_2c9e41_rmtree_retry_actions @ hermes_cli/profiles_remove.py:_rmtree_with_retry
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* local strdup (POSIX strdup name collision avoided). */
static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n + 1); }
    return p;
}

/* POSIX dirname semantics used by os.path.dirname on POSIX paths. */
static char *py_dirname(const char *p) {
    const char *slash;
    size_t n;
    char *r;
    if (!p) return NULL;
    slash = strrchr(p, '/');
    if (!slash) return NULL;                 /* no directory component -> "" */
    if (slash == p) return dupstr("/");      /* "/a" -> "/" */
    n = (size_t)(slash - p);
    r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, p, n);
    r[n] = '\0';
    return r;
}

/* _rmtree_make_writable: only PermissionError is handled; everything else is
 * re-raised. On PermissionError chmod(path) and chmod(dirname(path)) are
 * attempted (each suppressing OSError), then func(path) runs. We model the
 * observable decision/actions rather than touching the real filesystem. */
struct json_t *w14_profilesre_2c9e41_make_writable_actions(const struct json_t *in) {
    const char *error = json_get_str(in, "error", "");
    const char *path = json_get_str(in, "path", "");
    int is_perm = !strcmp(error, "PermissionError");
    struct json_t *out = json_object();
    struct json_t *chmod = json_array();

    json_set(out, "reraise", json_bool(!is_perm));
    if (is_perm) {
        char *parent;
        json_set(out, "call_func", json_bool(1));
        if (path && path[0]) json_append(chmod, json_string(path));
        parent = py_dirname(path ? path : "");
        if (parent && parent[0]) json_append(chmod, json_string(parent));
        free(parent);
    } else {
        json_set(out, "call_func", json_bool(0));
    }
    json_set(out, "chmod", chmod);
    return out;
}

static int boolish(const struct json_t *v, int dflt) {
    if (!v) return dflt;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return dflt;
}

/* _rmtree_with_retry: attempts = 3. Each iteration calls rmtree (onexc
 * handler); on success returns "success". On OSError, if the path no longer
 * exists returns "gone"; otherwise, before the last attempt, sleeps once.
 * Exhausting all attempts re-raises the last error -> "raised". */
struct json_t *w14_profilesre_2c9e41_rmtree_retry_actions(const struct json_t *in) {
    const struct json_t *ok_arr = json_obj_get(in, "rmtree_ok");
    const struct json_t *ex_arr = json_obj_get(in, "exists");
    const char *result = "raised";
    int rmtree_calls = 0, exists_calls = 0, sleeps = 0, done = 0;
    int attempt;

    for (attempt = 0; attempt < 3; attempt++) {
        int ok = 1, exists = 1;
        rmtree_calls++;
        if (ok_arr && ok_arr->type == JSON_ARRAY && (size_t)attempt < json_len(ok_arr))
            ok = boolish(json_get(ok_arr, (size_t)attempt), 1);
        if (ok) { result = "success"; done = 1; break; }

        if (ex_arr && ex_arr->type == JSON_ARRAY && (size_t)attempt < json_len(ex_arr))
            exists = boolish(json_get(ex_arr, (size_t)attempt), 1);
        exists_calls++;
        if (!exists) { result = "gone"; done = 1; break; }
        if (attempt < 2) sleeps++;
    }
    (void)done; /* result already defaults to "raised" */

    {
        struct json_t *out = json_object();
        json_set(out, "result", json_string(result));
        json_set(out, "rmtree_calls", json_number(rmtree_calls));
        json_set(out, "exists_calls", json_number(exists_calls));
        json_set(out, "sleeps", json_number(sleeps));
        return out;
    }
}
