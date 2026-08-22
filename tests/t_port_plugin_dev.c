/*
 * t_port_plugin_dev.c — parity harness for src/hermes_cli/plugin_dev.c
 * (hermes_cli/plugin_dev.py deterministic surface).
 *
 * Fixture ops (JSON {"ops":[...]}), one output line per op:
 *   {"kind":"format","path":..,"manifest":{"name","version","kind"}|null,
 *    "errors":[..],"warnings":[..],"tools":[..],"hooks":[..]}
 *   {"kind":"mkdirs","dirs":["@SBX@/x", ...]}
 *   {"kind":"resolve","target":..,"home":"@SBX@/home",
 *    "bundled":"@SBX@/bundled","cwd":"@SBX@"}
 *   {"kind":"manifest_v2","manifest":{...},"installed":{"pkg":"1.2.3"|null}}
 *   {"kind":"registrations","declared_hooks":[..],"registered_hooks":[..],
 *    "declared_tools":[..],"registered_tools":[..]}
 */
#include "plugin_dev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static char **str_array(const json_t *o, const char *key, size_t *n) {
    *n = 0;
    const json_t *arr = json_obj_get((json_t *)o, key);
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    size_t cnt = json_len(arr);
    char **v = malloc((cnt ? cnt : 1) * sizeof(char *));
    for (size_t i = 0; i < cnt; i++) {
        const json_t *e = json_get(arr, i);
        v[i] = (e && e->type == JSON_STRING && e->str_val) ? strdup(e->str_val)
                                                           : strdup("");
    }
    *n = cnt;
    return v;
}

static json_t *findings_to_json(const pd_report_t *r) {
    json_t *arr = json_array();
    for (size_t i = 0; i < r->n_findings; i++) {
        json_t *f = json_object();
        json_set(f, "level", json_string(r->findings[i].level));
        json_set(f, "message", json_string(r->findings[i].message));
        json_append(arr, f);
    }
    return arr;
}

static json_t *g_op_global = NULL;
static json_t *g_installed(void) { return g_op_global; }

static int hook_dist_version(const char *dist, char **out) {
    json_t *installed = json_obj_get(g_installed(), "installed");
    if (!installed || installed->type != JSON_OBJECT) return -1;
    size_t n = json_object_size(installed);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(installed, i);
        if (strcmp(k, dist) == 0) {
            const json_t *v = json_object_get_at(installed, i);
            if (!v || v->type == JSON_NULL) return 0; /* PackageNotFound */
            *out = strdup(v->type == JSON_STRING ? v->str_val : "");
            return 1;
        }
    }
    return -1; /* not in map => other exception => skipped */
}

/* forward decl trick: current manifest op accessor */
static void mkdirs(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

static void emit(json_t *o) {
    char *line = json_serialize(o);
    printf("%s\n", line);
    free(line);
    json_free(o);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    plugin_dev_dist_version_hook = hook_dist_version;
    json_t *ops = json_obj_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_object();
            if (strcmp(kind, "format") == 0) {
                pd_report_t *r = pd_report_new(json_get_str(c, "path", "."));
                const json_t *man = json_obj_get(c, "manifest");
                if (man && man->type == JSON_OBJECT) {
                    r->has_manifest = true;
                    r->manifest_name = strdup(json_get_str(man, "name", ""));
                    const json_t *ver = json_obj_get(man, "version");
                    if (ver && ver->type == JSON_STRING && ver->str_val)
                        r->manifest_version = strdup(ver->str_val);
                    r->manifest_kind = strdup(json_get_str(man, "kind", ""));
                }
                size_t n;
                char **errs = str_array(c, "errors", &n);
                for (size_t k = 0; errs && k < n; k++) pd_report_error(r, errs[k]);
                char **warns = str_array(c, "warnings", &n);
                for (size_t k = 0; warns && k < n; k++) pd_report_warning(r, warns[k]);
                r->registered_tools = str_array(c, "tools", &r->n_registered_tools);
                r->registered_hooks = str_array(c, "hooks", &r->n_registered_hooks);
                char *txt = plugin_dev_format_text(r);
                json_set(o, "out", json_string(txt ? txt : ""));
                free(txt);
                for (size_t k = 0; errs && k < n; k++) free(errs[k]);
                free(errs);
                for (size_t k = 0; warns && k < n; k++) free(warns[k]);
                free(warns);
                pd_report_free(r);
            } else if (strcmp(kind, "mkdirs") == 0) {
                const json_t *dirs = json_obj_get(c, "dirs");
                if (dirs && dirs->type == JSON_ARRAY)
                    for (size_t k = 0; k < json_len(dirs); k++) {
                        const json_t *d = json_get(dirs, k);
                        if (d && d->type == JSON_STRING) mkdirs(d->str_val);
                    }
                json_set(o, "out", json_bool(1));
            } else if (strcmp(kind, "resolve") == 0) {
                char *err = NULL;
                char *resolved = plugin_dev_resolve_plugin_path(
                    json_get_str(c, "target", ""),
                    json_get_str(c, "home", ""),
                    json_get_str(c, "bundled", ""),
                    json_get_str(c, "cwd", "."), &err);
                if (resolved) {
                    json_set(o, "out", json_string(resolved));
                    free(resolved);
                } else {
                    json_set(o, "out", json_null());
                    json_set(o, "error", json_string(err ? err : ""));
                }
                free(err);
            } else if (strcmp(kind, "manifest_v2") == 0) {
                g_op_global = c;
                pd_report_t *r = pd_report_new(".");
                plugin_dev_check_manifest_v2(r, json_obj_get(c, "manifest"));
                json_set(o, "out", findings_to_json(r));
                pd_report_free(r);
                g_op_global = NULL;
            } else if (strcmp(kind, "registrations") == 0) {
                pd_report_t *r = pd_report_new(".");
                size_t n1, n2, n3, n4;
                char **dh = str_array(c, "declared_hooks", &n1);
                char **rh = str_array(c, "registered_hooks", &n2);
                char **dt = str_array(c, "declared_tools", &n3);
                char **rt = str_array(c, "registered_tools", &n4);
                plugin_dev_check_registrations(r, dh, n1, rh, n2, dt, n3, rt, n4);
                json_set(o, "out", findings_to_json(r));
                for (size_t k = 0; dh && k < n1; k++) free(dh[k]);
                free(dh);
                for (size_t k = 0; rh && k < n2; k++) free(rh[k]);
                free(rh);
                for (size_t k = 0; dt && k < n3; k++) free(dt[k]);
                free(dt);
                for (size_t k = 0; rt && k < n4; k++) free(rt[k]);
                free(rt);
                pd_report_free(r);
            } else {
                json_set(o, "out", json_null());
            }
            emit(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
