/*
 * t_port_install_repair.c — parity harness for
 * src/hermes_cli/install_repair.c (hermes_cli/_install_repair.py).
 *
 * Fixture ops (JSON {"ops":[...]}), one output line per op:
 *   {"kind":"termux","env":{...}|null}
 *   {"kind":"console_scripts","root":"@FIXDIR@/proj1"}
 *   {"kind":"extras","root":"@FIXDIR@/proj1","group":"all"}
 *   {"kind":"bump","content":"{\"attempts\": 4}"}
 *   {"kind":"resolve_target","root":"@SBX@","exe":"/usr/bin/py",
 *    "uv_at":".hermes/bin"|null}
 *   {"kind":"core_install","root":"...","exe":"...","uv":true,
 *    "fail":[["uv","pip","install","-e",".[all]"]]}
 */
#include "install_repair.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static void mkdir_p_and_touch_uv(const char *dir) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
    size_t n = strlen(tmp) + 8;
    char *uv = malloc(n + 1);
    snprintf(uv, n + 1, "%s/uv", tmp);
    FILE *f = fopen(uv, "w");
    if (f) { fputs("#!/bin/sh\n", f); fclose(f); }
    chmod(uv, 0755);
    free(uv);
}

static json_t *g_op = NULL;

static char *hook_find_uv(void) {
    json_t *v = json_object_get(g_op, "uv");
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
        return strdup(v->str_val);
    return NULL;
}

static int hook_run_cmd(char *const *argv, size_t n, const char *cwd) {
    (void)n; (void)cwd;
    json_t *fails = json_object_get(g_op, "fail");
    if (fails && fails->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(fails); i++) {
            json_t *pat = json_get(fails, i);
            if (!pat || pat->type != JSON_ARRAY) continue;
            if (json_len(pat) != n) continue;
            bool eq = true;
            for (size_t k = 0; k < n; k++) {
                json_t *want_v = json_get(pat, k);
                const char *want = (want_v && want_v->type == JSON_STRING &&
                                    want_v->str_val) ? want_v->str_val : "\x01";
                if (strcmp(want, argv[k]) != 0) { eq = false; break; }
            }
            if (eq) return 1;
        }
    }
    return 0;
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
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "termux") == 0) {
                json_t *env = json_object_get(c, "env");
                bool t = env && env->type == JSON_OBJECT
                             ? install_repair_is_termux_env(env)
                             : install_repair_is_termux_env(NULL);
                json_object_set(o, "out", json_bool(t ? 1 : 0));
            } else if (strcmp(kind, "console_scripts") == 0) {
                size_t n = 0;
                char **names = install_repair_load_console_script_names(
                    json_get_str(c, "root", "."), &n);
                json_t *arr = json_new_array();
                for (size_t k = 0; names && k < n; k++)
                    json_append(arr, json_string(names[k]));
                json_object_set(o, "out", arr);
                for (size_t k = 0; names && k < n; k++) free(names[k]);
                free(names);
            } else if (strcmp(kind, "extras") == 0) {
                size_t n = 0;
                char **names = install_repair_load_installable_optional_extras(
                    json_get_str(c, "root", "."),
                    json_get_str(c, "group", "all"), &n);
                json_t *arr = json_new_array();
                for (size_t k = 0; names && k < n; k++)
                    json_append(arr, json_string(names[k]));
                json_object_set(o, "out", arr);
                for (size_t k = 0; names && k < n; k++) free(names[k]);
                free(names);
            } else if (strcmp(kind, "bump") == 0) {
                const char *tmplbase = getenv("TMPDIR");
                if (!tmplbase || !tmplbase[0]) tmplbase = "/tmp";
                char path[1024];
                snprintf(path, sizeof(path), "%s/bump_marker_%zu.json",
                         tmplbase, (size_t)getpid());
                FILE *f = fopen(path, "wb");
                const char *content = json_get_str(c, "content", "");
                fputs(content, f);
                fclose(f);
                int attempts = install_repair_bump_marker_attempts(path);
                unlink(path);
                json_object_set(o, "out", json_int(attempts));
            } else if (strcmp(kind, "resolve_target") == 0) {
                /* materialize a fake uv binary when requested */
                const char *uv_at = json_get_str(c, "uv_at", "");
                if (uv_at && uv_at[0]) {
                    const char *sbx = getenv("SLERMES_HOME");
                    if (!sbx || !sbx[0]) sbx = getenv("HOME");
                    char dir[1024];
                    snprintf(dir, sizeof(dir), "%s/%s", sbx, uv_at);
                    mkdir_p_and_touch_uv(dir);
                }
                install_repair_sys_executable = json_get_str(c, "exe", "python3");
                install_repair_find_uv_hook = NULL; /* use the real lookup */
                install_repair_target_t *t =
                    install_repair_resolve_install_target(
                        json_get_str(c, "root", "@SBX@"));
                json_t *meta = json_new_object();
                json_t *prefix = json_new_array();
                for (size_t k = 0; k < t->n_prefix; k++)
                    json_append(prefix, json_string(t->prefix[k]));
                json_object_set(meta, "prefix", prefix);
                json_object_set(meta, "has_env", json_bool(t->has_env ? 1 : 0));
                json_object_set(meta, "virtual_env",
                                json_string(t->virtual_env ? t->virtual_env : ""));
                json_object_set(meta, "strip_python_vars",
                                json_bool(t->strip_python_vars ? 1 : 0));
                json_object_set(o, "out", meta);
                install_repair_target_free(t);
                install_repair_sys_executable = NULL;
            } else if (strcmp(kind, "make_uv") == 0) {
                const char *sbx = getenv("SLERMES_HOME");
                if (!sbx || !sbx[0]) sbx = getenv("HOME");
                char dir[1024];
                snprintf(dir, sizeof(dir), "%s/%s", sbx,
                         json_get_str(c, "at", ".hermes/bin"));
                mkdir_p_and_touch_uv(dir);
                json_object_set(o, "out", json_bool(1));
            } else if (strcmp(kind, "core_install") == 0) {
                g_op = c;
                install_repair_sys_executable =
                    json_get_str(c, "exe", "python3");
                install_repair_find_uv_hook = hook_find_uv;
                install_repair_run_cmd_hook = hook_run_cmd;
                json_t *cmds = json_new_array();
                json_t *printed = json_new_array();
                install_repair_run_core_install(json_get_str(c, "root", "."),
                                                cmds, printed);
                json_t *meta = json_new_object();
                json_object_set(meta, "commands", cmds);
                json_object_set(meta, "printed", printed);
                json_object_set(o, "out", meta);
                install_repair_find_uv_hook = NULL;
                install_repair_run_cmd_hook = NULL;
                install_repair_sys_executable = NULL;
                g_op = NULL;
            } else {
                json_object_set(o, "out", json_new_null());
            }
            emit(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
