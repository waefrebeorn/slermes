/*
 * browser_harness_dirs.c - C home for upstream Hermes tools/browser_harness_dirs.py.
 * PoP: w14_browserhar_09a2c6_daemon_identity @ tools/browser_harness_dirs.py:daemon_identity
 * PoP: w14_browserhar_09a2c6_daemon_env @ tools/browser_harness_dirs.py:daemon_env
 * PoP: w14_browserhar_09a2c6_socket_dir_budget @ tools/browser_harness_dirs.py:socket_dir_budget
 * PoP: w14_browserhar_09a2c6_fits_socket_budget @ tools/browser_harness_dirs.py:fits_socket_budget
 * PoP: w14_browserhar_09a2c6_harness_home @ tools/browser_harness_dirs.py:_harness_home
 * PoP: w14_browserhar_09a2c6_harness_pid_file @ tools/browser_harness_dirs.py:harness_pid_file
 *
 * Pure env/string helpers for per-profile browser-harness daemon dirs. All
 * environment values arrive as JSON (no process env is read): the harness
 * resolves dirs against the child env, so fixtures carry HOME etc. explicitly.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_browserhar_09a2c6.h"

static const char *KEYS[4] = {"BH_RUNTIME_DIR", "BH_RUNTIME_DIR_SHARED", "BH_TMP_DIR", "BH_TMP_DIR_SHARED"};

static const char *gs(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}
static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmt2(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b) + 1; char *p = (char *)malloc(n);
    if (p) { strcpy(p, a); strcat(p, b); } return p;
}
static char *fmt3(const char *a, const char *b, const char *c) {
    char *ab = fmt2(a, b); char *r = fmt2(ab ? ab : "", c); free(ab); return r;
}

/* pathlib on this host (Windows) uses ntpath: '/' folds to '\', repeated
 * separators collapse, '.' components drop, a leading separator is kept as
 * one '\'. Reproduce str(Path(...)) so the differential oracle matches. */
static char *norm_path(const char *s) {
    if (!s) s = "";
    int drive = (s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z');
    drive = drive && s[1] == ':';
    int rooted = s[0] == '/' || s[0] == '\\';
    const char *p = drive ? s + 2 : s;
    size_t ncomp = 0; char **comp = NULL;
    while (*p) {
        const char *q = p;
        while (*q && *q != '/' && *q != '\\') q++;
        size_t len = (size_t)(q - p);
        if (len && !(len == 1 && p[0] == '.')) {
            char **np = (char **)realloc(comp, (ncomp + 1) * sizeof(char *));
            if (np) {
                comp = np;
                comp[ncomp] = (char *)malloc(len + 1);
                if (comp[ncomp]) {
                    memcpy(comp[ncomp], p, len); comp[ncomp][len] = 0; ncomp++;
                }
            }
        }
        p = *q ? q + 1 : q;
    }
    size_t total = (drive ? 2 : 0) + (rooted ? 1 : 0);
    for (size_t i = 0; i < ncomp; i++) total += strlen(comp[i]) + (i ? 1 : 0);
    char *out = (char *)calloc(total + 3, 1);
    size_t k = 0;
    if (drive) { out[k++] = s[0]; out[k++] = ':'; }
    if (rooted) out[k++] = '\\';
    for (size_t i = 0; i < ncomp; i++) {
        if (k && out[k - 1] != '\\') out[k++] = '\\';
        strcpy(out + k, comp[i]); k += strlen(comp[i]);
        free(comp[i]);
    }
    free(comp);
    if (!k) { out[k++] = '.'; }
    out[k] = 0;
    return out;
}

/* str(Path(a) / b): absolute/drive b resets; '.' spells neutral. */
static char *path_join(const char *a, const char *b) {
    char *na = norm_path(a), *nb = norm_path(b);
    int nb_abs = nb[0] == '\\' || (nb[0] && nb[1] == ':');
    if (nb_abs) { free(na); return nb; }
    if (!strcmp(na, ".")) { free(na); return nb; }
    if (!strcmp(nb, ".")) { free(nb); return na; }
    size_t n = strlen(na) + strlen(nb) + 2;
    char *r = (char *)malloc(n);
    strcpy(r, na); strcat(r, "\\"); strcat(r, nb);
    free(na); free(nb);
    return r;
}

struct json_t *w14_browserhar_09a2c6_daemon_identity(const struct json_t *env) {
    json_t *a = json_array();
    for (int i = 0; i < 4; i++) json_append(a, json_string(gs(env, KEYS[i], "")));
    json_append(a, json_string(gs(env, "BU_NAME", "default")));
    return a;
}

struct json_t *w14_browserhar_09a2c6_daemon_env(const struct json_t *env, const struct json_t *identity) {
    json_t *target = json_object();
    size_t n = json_object_size(env);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(env, i);
        if (!k) continue;
        int skip = 0;
        for (int j = 0; j < 4; j++) if (!strcmp(k, KEYS[j])) { skip = 1; break; }
        if (!skip) json_set(target, k, json_copy(json_object_get_at(env, i)));
    }
    if (identity && identity->type == JSON_ARRAY) {
        size_t cnt = json_len(identity);
        if (cnt) {
            for (size_t i = 0; i + 1 < cnt && i < 4; i++) {
                json_t *v = json_get(identity, i);
                const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
                if (s[0]) json_set(target, KEYS[i], json_string(s));
            }
            json_t *last = json_get(identity, cnt - 1);
            const char *nm = (last && last->type == JSON_STRING) ? last->str_val : "";
            json_set(target, "BU_NAME", json_string(nm));
        }
    }
    return target;
}

struct json_t *w14_browserhar_09a2c6_socket_dir_budget(const char *platform) {
    if (!strcmp(platform, "darwin")) return json_number(30);
    if (!strncmp(platform, "linux", 5)) return json_number(34);
    return json_null();
}

int w14_browserhar_09a2c6_fits_socket_budget(const char *resolved_dir, const struct json_t *budget) {
    if (!budget || budget->type == JSON_NULL) return 1;
    double b = budget->num_val;
    return (double)strlen(resolved_dir) <= b;
}

static char *harness_home_s(const struct json_t *env) {
    const char *root = gs(env, "BH_HOME", "");
    if (!root[0]) root = gs(env, "BROWSER_HARNESS_HOME", "");
    if (!root[0]) {
        const char *xdg = gs(env, "XDG_CONFIG_HOME", "");
        if (xdg[0]) return path_join(xdg, "browser-harness");
        const char *home = gs(env, "HOME", "");
        if (!home[0]) home = ".";  /* Path.home() fallback — fixtures always carry HOME */
        char *tmp = path_join(home, ".config");
        char *res = path_join(tmp, "browser-harness");
        free(tmp);
        return res;
    }
    /* str(Path(root).expanduser()): no '~' in fixture inputs, identity here. */
    return norm_path(root);
}

struct json_t *w14_browserhar_09a2c6_harness_home(const struct json_t *env) {
    char *h = harness_home_s(env);
    json_t *r = json_string(h ? h : "");
    free(h);
    return r;
}

struct json_t *w14_browserhar_09a2c6_harness_pid_file(const struct json_t *env, const char *suffix) {
    const char *isolated = gs(env, "BH_RUNTIME_DIR", "");
    if (!isolated[0]) isolated = gs(env, "BH_TMP_DIR", "");
    char *runtime;
    if (isolated[0]) runtime = norm_path(isolated);
    else {
        char *h = harness_home_s(env);
        runtime = path_join(h, "runtime");
        free(h);
    }
    int shared = !isolated[0] || !strcmp(gs(env, "BH_RUNTIME_DIR_SHARED", ""), "1");
    char *fname = shared
        ? fmt3("bu-", gs(env, "BU_NAME", "default"), suffix)
        : fmt2("bu", suffix);
    char *res = path_join(runtime, fname);
    free(runtime); free(fname);
    json_t *r = json_string(res ? res : "");
    free(res);
    return r;
}

