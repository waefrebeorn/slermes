/*
 * auth_external_process.c - C11 port of upstream
 * hermes_cli/auth_external_process.py (gate w24_authextern_606ea5).
 *
 * PoP: w24_authextern_606ea5_command @ hermes_cli/auth_external_process.py:external_process_command
 * PoP: w24_authextern_606ea5_lookup @ hermes_cli/auth_external_process.py:resolve_external_process_command
 * PoP: w24_authextern_606ea5_launch_env_names @ hermes_cli/auth_external_process.py:external_process_launch_env_names
 * PoP: w24_authextern_606ea5_denylist @ hermes_cli/auth_external_process.py:env_writer_launch_denylist
 *
 * What is ported is the launch policy: the first set process_command_env_vars
 * override wins over the profile's process_command (a GUI/service launch has a
 * bare PATH, which is why the overrides exist at all); an operator-configured
 * relative path is anchored to the launch cwd before the resolver's
 * absolute-only explicit check sees it; and the env names that choose an
 * external-process provider's executable or argv are collected from every
 * registered profile so the generic env writers must refuse them.
 *
 * Seams, deliberately not ported: locate_command (a filesystem walk),
 * hermes_platform.resolver.known_dirs.user_local_bin, Claude Code's
 * find_claude_code_cli, providers.list_providers and left_core_migration's
 * launch_env_names (a sqlite read).  They become the arguments here: the lookup
 * string is returned instead of the file it finds, and the profile/launch_env
 * tables are handed in.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_authextern_606ea5.h"

/* ---------------------------------------------------------------- helpers */

static char *ax_dup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *d = (char *)malloc(n + 1);
    if (d) memcpy(d, s ? s : "", n + 1);
    return d;
}

static int ax_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str() of the scalar shapes a command holds; NULL for the rest. */
static char *ax_pystr(const json_t *v) {
    char *buf;
    double d;

    if (!v) return NULL;
    if (v->type == JSON_STRING) return ax_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return ax_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        buf = (char *)malloc(40);
        if (!buf) return NULL;
        d = v->num_val;
        if (d == (double)(long long)d && d > -1e17 && d < 1e17) snprintf(buf, 40, "%.0f", d);
        else snprintf(buf, 40, "%.15g", d);
        return buf;
    }
    return NULL;
}

/* str.strip() over ASCII whitespace, in place. */
static void ax_strip(char *s) {
    size_t n;
    char *p = s;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') s[--n] = '\0';
        else break;
    }
}

typedef struct { char *p; size_t n, cap; } ax_buf;

static void ax_buf_init(ax_buf *b) {
    b->cap = 64; b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void ax_buf_add(ax_buf *b, const char *s, size_t len) {
    if (!b->p) return;
    if (b->n + len + 1 > b->cap) {
        size_t cap = b->cap;
        char *np;
        while (b->n + len + 1 > cap) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np; b->cap = cap;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void ax_buf_adds(ax_buf *b, const char *s) { ax_buf_add(b, s ? s : "", strlen(s ? s : "")); }

/* ndpath.expanduser: "~" or "~<sep>..." -> the user home (a "~user" form is left
 * alone, as the live helper falls through). */
static char *ax_expanduser(const char *path, const char *home) {
    ax_buf b;
    size_t hl;

    if (path[0] != '~') return ax_dup(path);
    if (path[1] != '\0' && path[1] != '\\' && path[1] != '/') return ax_dup(path);
    if (!home) return ax_dup(path);
    ax_buf_init(&b);
    hl = strlen(home);
    while (hl > 0 && (home[hl - 1] == '\\' || home[hl - 1] == '/')) hl--;
    ax_buf_add(&b, home, hl);
    ax_buf_adds(&b, path + 1);
    return b.p;
}

/* ntpath.expandvars: %NAME% and $NAME / ${NAME}; an unknown name is left
 * verbatim, exactly as the live helper does. */
static char *ax_expandvars(const char *path, const json_t *env) {
    ax_buf b;
    size_t i = 0, n = strlen(path);

    if (!strchr(path, '%') && !strchr(path, '$')) return ax_dup(path);
    ax_buf_init(&b);
    while (i < n) {
        if (path[i] == '%') {
            const char *close = strchr(path + i + 1, '%');
            if (close && close > path + i + 1) {
                size_t nlen = (size_t)(close - (path + i + 1));
                char *name = (char *)malloc(nlen + 1);
                const char *val = NULL;
                if (name) {
                    memcpy(name, path + i + 1, nlen);
                    name[nlen] = '\0';
                    val = json_get_str(env, name, NULL);
                }
                if (val) ax_buf_adds(&b, val);
                else ax_buf_add(&b, path + i, (size_t)(close - (path + i)) + 1);
                free(name);
                i = (size_t)(close - path) + 1;
                continue;
            }
            ax_buf_add(&b, "%", 1);
            i++;
            continue;
        }
        if (path[i] == '$' && i + 1 < n) {
            size_t s = i + 1, e = s;
            const char *val = NULL;
            char *name;
            if (path[s] == '{') {
                const char *close = strchr(path + s + 1, '}');
                if (close) {
                    s++;
                    e = (size_t)(close - path);
                } else {
                    ax_buf_add(&b, "$", 1);
                    i++;
                    continue;
                }
            } else {
                while (e < n && (path[e] == '_' || (path[e] >= '0' && path[e] <= '9') ||
                                 (path[e] >= 'a' && path[e] <= 'z') || (path[e] >= 'A' && path[e] <= 'Z')))
                    e++;
                if (e == s) {
                    ax_buf_add(&b, "$", 1);
                    i++;
                    continue;
                }
            }
            name = (char *)malloc(e - s + 1);
            if (name) {
                memcpy(name, path + s, e - s);
                name[e - s] = '\0';
                val = json_get_str(env, name, NULL);
            }
            if (val) {
                ax_buf_adds(&b, val);
                i = (path[s - 1] == '{') ? e + 1 : e;
            } else {
                size_t end = (path[s - 1] == '{') ? e + 1 : e;
                ax_buf_add(&b, path + i, end - i);
                i = end;
            }
            free(name);
            continue;
        }
        ax_buf_add(&b, path + i, 1);
        i++;
    }
    return b.p;
}

/* ntpath.normpath: separators unified to '\\', "." dropped, ".." resolved
 * lexically, no trailing separator (except a bare drive root). */
static char *ax_normpath(const char *path) {
    char *w = ax_dup(path);
    size_t len = strlen(w), i;
    char drive[3] = { 0, 0, 0 };
    int rooted = 0;
    char **comp = NULL;
    size_t ncomp = 0, cap = 0, j;
    ax_buf b;

    for (i = 0; i < len; i++) if (w[i] == '/') w[i] = '\\';
    if (len >= 2 && w[1] == ':') { drive[0] = w[0]; drive[1] = ':'; memmove(w, w + 2, len - 1); len -= 2; }
    if (len > 0 && w[0] == '\\') { rooted = 1; memmove(w, w + 1, len); len--; }

    i = 0;
    while (i <= len) {
        size_t s = i;
        while (i < len && w[i] != '\\') i++;
        if (i > s) {
            size_t clen = i - s;
            if (clen == 1 && w[s] == '.') {
                /* skip */
            } else if (clen == 2 && w[s] == '.' && w[s + 1] == '.') {
                if (ncomp > 0 && strcmp(comp[ncomp - 1], "..") != 0) ncomp--;
                else if (!rooted) {
                    if (ncomp == cap) { cap = cap ? cap * 2 : 8; comp = (char **)realloc(comp, cap * sizeof(char *)); }
                    comp[ncomp] = ax_dup("..");
                    ncomp++;
                }
            } else {
                if (ncomp == cap) { cap = cap ? cap * 2 : 8; comp = (char **)realloc(comp, cap * sizeof(char *)); }
                comp[ncomp] = (char *)malloc(clen + 1);
                if (comp[ncomp]) { memcpy(comp[ncomp], w + s, clen); comp[ncomp][clen] = '\0'; }
                ncomp++;
            }
        }
        i++;
    }

    ax_buf_init(&b);
    ax_buf_adds(&b, drive);
    if (rooted) ax_buf_add(&b, "\\", 1);
    for (j = 0; j < ncomp; j++) {
        if (j) ax_buf_add(&b, "\\", 1);
        ax_buf_adds(&b, comp[j]);
        free(comp[j]);
    }
    free(comp);
    free(w);
    if (!b.p) return NULL;
    if (b.n == 0) {
        free(b.p);
        return ax_dup(".");
    }
    return b.p;
}

/* ntpath.abspath: a relative path is anchored on the cwd, then normalized. */
static char *ax_abspath(const char *path, const char *cwd) {
    ax_buf b;
    int absolute;

    if (!path[0]) {
        /* _getfullpathname("") is the cwd */
        return ax_normpath(cwd ? cwd : "\\");
    }
    absolute = path[0] == '\\' || path[0] == '/' ||
               (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':' &&
                (path[2] == '\\' || path[2] == '/'));
    if (absolute) return ax_normpath(path);
    ax_buf_init(&b);
    ax_buf_adds(&b, cwd ? cwd : "");
    ax_buf_add(&b, "\\", 1);
    ax_buf_adds(&b, path);
    {
        char *joined = b.p;
        char *norm = ax_normpath(joined ? joined : path);
        free(joined);
        return norm;
    }
}

/* ------------------------------------------------------------- the policy */

json_t *w24_authextern_606ea5_command(const json_t *env_vars, const json_t *env,
                                      const json_t *process_command) {
    size_t i, n;
    /* for var in (profile.process_command_env_vars or ()): a truthy non-sequence
     * is the live TypeError seam. */
    if (env_vars && env_vars->type != JSON_NULL && env_vars->type != JSON_ARRAY &&
        ax_truthy(env_vars))
        return json_null();
    n = json_len(env_vars);

    /* overrides = (os.getenv(var, "").strip() for var in ...); first truthy wins */
    for (i = 0; i < n; i++) {
        const json_t *name = json_get(env_vars, i);
        const char *val;
        if (!name || name->type != JSON_STRING) continue;
        val = json_get_str(env, name->str_val, "");
        if (val && val[0]) {
            char *copy = ax_dup(val);
            if (!copy) return json_null();
            ax_strip(copy);                    /* .strip() before the truth test */
            if (copy[0]) {
                json_t *r = json_string(copy);
                free(copy);
                return r ? r : json_null();
            }
            free(copy);
        }
    }
    /* or str(getattr(profile, "process_command", "") or "") */
    if (!ax_truthy(process_command)) return json_string("");
    {
        char *s = ax_pystr(process_command);
        json_t *r;
        if (!s) return json_null();  /* str() seam */
        r = json_string(s);
        free(s);
        return r;
    }
}

json_t *w24_authextern_606ea5_lookup(const json_t *command, const json_t *cwd,
                                     const json_t *home, const json_t *env) {
    const char *cmd, *cw, *hm;
    char *expanded, *vars, *abs;
    json_t *out;

    if (!command) return json_null();
    if (command->type != JSON_STRING) return json_null();  /* `os.sep in 42` seam */
    cmd = command->str_val ? command->str_val : "";
    if (!cmd[0]) return json_null();                        /* if not command: None */
    cw = (cwd && cwd->type == JSON_STRING) ? cwd->str_val : NULL;
    hm = (home && home->type == JSON_STRING) ? home->str_val : NULL;
    /* A bare name has no separator: it goes to the resolver untouched. */
    if (!strchr(cmd, '\\') && !strchr(cmd, '/')) return json_string(cmd);
    if (!cw) return json_null();

    expanded = ax_expanduser(cmd, hm);
    if (!expanded) return json_null();
    vars = ax_expandvars(expanded, env);
    free(expanded);
    if (!vars) return json_null();
    abs = ax_abspath(vars, cw);
    free(vars);
    if (!abs) return json_null();
    out = json_string(abs);
    free(abs);
    return out;
}

/* name list -> sorted array of unique strings */
static int ax_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *ax_sorted_unique(char **names, size_t n) {
    json_t *arr = json_array();
    size_t i;

    if (n > 1) qsort(names, n, sizeof(char *), ax_cmp_str);
    for (i = 0; i < n; i++) {
        if (i > 0 && strcmp(names[i], names[i - 1]) == 0) continue;
        json_append(arr, json_string(names[i]));
    }
    for (i = 0; i < n; i++) free(names[i]);
    free(names);
    return arr;
}

/* Collect every env name an external_process profile declares.  Returns NULL
 * with *seam set when a profile is missing the attribute the live comprehension
 * reads (the AttributeError seam). */
static char **ax_profile_names(const json_t *profiles, size_t *count, int *seam) {
    size_t i, n = json_len(profiles), cap = 0, used = 0;
    char **names = NULL;

    *seam = 0;
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(profiles, i);
        const json_t *vars, *arg;
        size_t j, vn;
        const char *auth;

        if (!p || p->type != JSON_OBJECT) continue;
        auth = json_get_str(p, "auth_type", NULL);
        if (!auth || strcmp(auth, "external_process") != 0) continue;
        if (!json_obj_get(p, "process_command_env_vars") || !json_obj_get(p, "process_args_env_var")) {
            *seam = 1;   /* profile.process_command_env_vars / .process_args_env_var */
            *count = 0;
            if (names) { for (j = 0; j < used; j++) free(names[j]); free(names); }
            return NULL;
        }
        vars = json_obj_get(p, "process_command_env_vars");
        vn = json_len(vars);
        for (j = 0; j <= vn; j++) {
            const json_t *name = j < vn ? json_get(vars, j) : NULL;
            if (j == vn) {
                /* process_args_env_var, after the tuple expansion */
                arg = json_obj_get(p, "process_args_env_var");
                if (!ax_truthy(arg) || !arg || arg->type != JSON_STRING) break;
                name = arg;
            }
            if (!name || name->type != JSON_STRING || !name->str_val || !name->str_val[0]) continue;
            if (used == cap) {
                cap = cap ? cap * 2 : 8;
                names = (char **)realloc(names, cap * sizeof(char *));
                if (!names) { *count = 0; return NULL; }
            }
            names[used++] = ax_dup(name->str_val);
        }
    }
    *count = used;
    return names;
}

json_t *w24_authextern_606ea5_launch_env_names(const json_t *profiles) {
    size_t n = 0;
    int seam = 0;
    char **names = ax_profile_names(profiles, &n, &seam);
    if (seam) return json_null();
    if (!names) return json_array();
    return ax_sorted_unique(names, n);
}

json_t *w24_authextern_606ea5_denylist(const json_t *profiles, const json_t *launch_env) {
    size_t n = 0, extra = json_len(launch_env), i, used = 0, cap = 0;
    int seam = 0;
    char **names = ax_profile_names(profiles, &n, &seam);
    char **all;

    if (seam) return json_null();
    cap = n + extra + 1;
    all = (char **)calloc(cap, sizeof(char *));
    if (!all) { if (names) { for (i = 0; i < n; i++) free(names[i]); free(names); } return json_array(); }
    for (i = 0; i < n; i++) all[used++] = names[i];
    /* Every left-core row's launch_env, verbatim: the ``if name`` filter is the
     * external-process comprehension's, not this union's. */
    for (i = 0; i < extra; i++) {
        const json_t *v = json_get(launch_env, i);
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        all[used++] = ax_dup(v->str_val);
    }
    free(names);
    return ax_sorted_unique(all, used);
}
