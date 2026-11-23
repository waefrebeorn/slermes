/*
 * runtime_stage.c — faithful C11 port of the deterministic planning logic in
 * upstream Hermes pm/runtime_stage.py (gate w6_runtimestag_fecd45).
 *
 * PoP: w6_runtimestag_plan @ pm/runtime_stage.py:stage_runtime
 *
 * stage_runtime is entirely effectful (venv build, file copies, subprocess).
 * Its observable decision trace — resolved paths, cache/offline choices, the
 * copy list, sync mode, the validation argv and the InstallError message — is
 * reproduced here with every collaborator injected via JSON seams, mirroring
 * the seam-recorded differential oracle.  No process environment is mutated.
 */
#include "noc_w6_runtimestag_fecd45.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* member, or NULL when absent or JSON null */
static json_t *m_or_null(const json_t *o, const char *k) {
    json_t *v = (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
    return (v && v->type == JSON_NULL) ? NULL : v;
}
static const char *s_or(const json_t *o, const char *k, const char *d) {
    json_t *v = m_or_null(o, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : d;
}

/* Convert backslash separators to forward slashes (path comparison form). */
static char *fwd(const char *s) {
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) if (*p == '\\') *p = '/';
    return o;
}

/* Python Path(p).absolute() with injected cwd; result uses '/' separators. */
static char *path_abs(const char *p, const char *cwd) {
    if (!p) p = "";
    if (!cwd) cwd = "";
    size_t lp = strlen(p);
    char *tmp;
    int unc = lp >= 2 && (p[0] == '/' || p[0] == '\\') && (p[1] == '/' || p[1] == '\\');
    int drive = lp >= 3 && isalpha((unsigned char)p[0]) && p[1] == ':' &&
                (p[2] == '/' || p[2] == '\\');
    int rooted = lp >= 1 && (p[0] == '/' || p[0] == '\\');
    if (unc || drive) {
        tmp = (char *)malloc(lp + 1);
        memcpy(tmp, p, lp); tmp[lp] = '\0';
    } else if (rooted) {
        int hd = isalpha((unsigned char)cwd[0]) && cwd[1] == ':';
        size_t pre = hd ? 2 : 0;
        tmp = (char *)malloc(pre + lp + 1);
        if (hd) { tmp[0] = cwd[0]; tmp[1] = ':'; }
        memcpy(tmp + pre, p, lp); tmp[pre + lp] = '\0';
    } else if (lp == 0 || (lp == 1 && p[0] == '.')) {
        tmp = strdup(cwd);
    } else {
        size_t lc = strlen(cwd);
        tmp = (char *)malloc(lc + 1 + lp + 1);
        memcpy(tmp, cwd, lc);
        size_t n = lc;
        if (n == 0 || (tmp[n - 1] != '/' && tmp[n - 1] != '\\')) tmp[n++] = '/';
        memcpy(tmp + n, p, lp); tmp[n + lp] = '\0';
    }
    char *out = fwd(tmp);
    free(tmp);
    return out;
}

/* Python str.strip() for the ASCII whitespace stderr carries. */
static char *strip_ws(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *o = (char *)malloc((size_t)(b - a) + 1);
    memcpy(o, a, (size_t)(b - a));
    o[b - a] = '\0';
    return o;
}

json_t *w6_runtimestag_plan(const json_t *in) {
    const char *cwd = s_or(in, "cwd", "");
    const char *project = s_or(in, "project", "");
    const char *destination = s_or(in, "destination", "");
    const char *uv_def = s_or(in, "uv_cache_default", "");
    const char *executable = s_or(in, "executable", "");
    json_t *wh = m_or_null(in, "wheelhouse");
    json_t *cache = m_or_null(in, "cache");
    int has_wh = wh && wh->type == JSON_STRING;
    int offline = json_get_bool(in, "offline", false) ? 1 : 0;
    int rc = (int)json_get_num(in, "check_rc", 0);
    const char *check_stderr = s_or(in, "check_stderr", "");

    int eff_offline = offline || has_wh;
    char *dest_abs = path_abs(destination, cwd);
    char *proj_fwd = fwd(project);
    char *cache_val;
    const char *cache_source;
    if (cache && cache->type == JSON_STRING) {
        cache_val = path_abs(cache->str_val, cwd);
        cache_source = "explicit";
    } else {
        cache_val = fwd(uv_def);
        cache_source = "default";
    }

    json_t *out = json_object();
    json_set(out, "project", json_string(proj_fwd));
    json_set(out, "destination", json_string(dest_abs));
    json_set(out, "cache", json_string(cache_val));
    json_set(out, "cache_source", json_string(cache_source));
    json_set(out, "offline", json_bool(eff_offline != 0));

    json_t *copies = json_array();
    json_append(copies, json_string("pyproject.toml"));
    json_append(copies, json_string("uv.lock"));
    json_set(out, "copy_files", copies);
    json_set(out, "created", json_bool(1)); /* environment.create() always runs */
    json_set(out, "sync_mode", json_string(has_wh ? "wheelhouse" : "locked"));

    json_t *argv = json_array();
    json_append(argv, json_string(executable));
    json_append(argv, json_string("-I"));
    json_append(argv, json_string("-B"));
    json_append(argv, json_string("-c"));
    json_append(argv, json_string("import packaging, tomli_w, truststore; from ruamel.yaml import YAML"));
    json_set(out, "check_argv", argv);

    if (rc != 0) {
        /* pm.package.InstallError.__init__ builds a single composed message:
         *   f"{package}: {cause} — {remedy}"  with remedy defaulting to
         *   "retry, or run `hermes pm doctor`". */
        char *msg = strip_ws(check_stderr);
        char full[1400];
        snprintf(full, sizeof(full),
                 "pm-runtime: dependency validation failed: %s — retry, or run `hermes pm doctor`",
                 msg);
        json_t *err = json_array();
        json_append(err, json_string(full));
        json_set(out, "error", err);
        free(msg);
    } else {
        json_set(out, "error", json_null());
    }
    json_set(out, "result", json_string(executable));

    free(proj_fwd); free(dest_abs); free(cache_val);
    return out;
}
