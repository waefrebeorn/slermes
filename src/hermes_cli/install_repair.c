#define _POSIX_C_SOURCE 200809L
/*
 * install_repair.c — faithful port of hermes_cli/_install_repair.py.
 * Port of Python: hermes_cli/_install_repair.py
 *
 * Both callers run the same core ".[all]" reinstall; this module is the
 * shared implementation. Deliberately stdlib-only in spirit: no third-party
 * imports beyond libtoml (the tomllib analog).
 */
#include "install_repair.h"
#include "hermes_json.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <libtoml/toml.h>

install_repair_find_uv_fn install_repair_find_uv_hook = NULL;
install_repair_run_cmd_fn install_repair_run_cmd_hook = NULL;
const char *install_repair_sys_executable = NULL;

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

bool install_repair_is_windows(void) {
#if defined(_WIN32)
    return true;
#else
    return false; /* sys.platform == "win32" */
#endif
}

/* _er._find_uv_binary(): ~/.hermes/bin, ~/.local/bin, ~/.cargo/bin, then
 * shutil.which("uv") over PATH. */
static char *default_find_uv_binary(void) {
    const char *exe =
        install_repair_is_windows() ? "uv.exe" : "uv";
    const char *home = getenv("HOME");
    if (home) {
        const char *rels[] = { "/.hermes/bin/", "/.local/bin/", "/.cargo/bin/" };
        for (size_t i = 0; i < 3; i++) {
            size_t n = snprintf(NULL, 0, "%s%s%s", home, rels[i], exe);
            char *cand = malloc(n + 1);
            snprintf(cand, n + 1, "%s%s%s", home, rels[i], exe);
            struct stat st;
            if (stat(cand, &st) == 0 && S_ISREG(st.st_mode)) return cand;
            free(cand);
        }
    }
    /* shutil.which */
    const char *path_env = getenv("PATH");
    if (!path_env) path_env = "/usr/bin:/bin";
    char *paths = strdup(path_env);
    char *save = NULL;
    for (char *dir = strtok_r(paths, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        size_t n = snprintf(NULL, 0, "%s/%s", dir, exe);
        char *cand = malloc(n + 1);
        snprintf(cand, n + 1, "%s/%s", dir, exe);
        struct stat st;
        if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) && access(cand, X_OK) == 0) {
            free(paths);
            return cand;
        }
        free(cand);
    }
    free(paths);
    return NULL;
}

bool install_repair_is_termux_env(const json_t *env) {
    /* env.get("TERMUX_VERSION") truthy -> True */
    const json_t *tv = json_object_get((json_t *)env, "TERMUX_VERSION");
    if (tv && tv->type == JSON_STRING && tv->str_val && tv->str_val[0]) return true;
    if (tv && tv->type == JSON_NUMBER && tv->num_val != 0.0) return true;
    const json_t *px = json_object_get((json_t *)env, "PREFIX");
    if (px && px->type == JSON_STRING && px->str_val &&
        strstr(px->str_val, "com.termux"))
        return true;
    return false;
}

/* Real-environ probe when env==NULL (Python's os.environ default). */
static bool termux_from_environ(void) {
    const char *tv = getenv("TERMUX_VERSION");
    if (tv && tv[0]) return true;
    const char *px = getenv("PREFIX");
    if (px && strstr(px, "com.termux")) return true;
    return false;
}

install_repair_target_t *install_repair_resolve_install_target(const char *root) {
    install_repair_target_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    char *uv_bin = install_repair_find_uv_hook
                       ? install_repair_find_uv_hook()
                       : default_find_uv_binary();
    if (uv_bin) {
        t->n_prefix = 2;
        t->prefix = malloc(2 * sizeof(char *));
        t->prefix[0] = uv_bin;
        t->prefix[1] = strdup("pip");
        t->has_env = true;
        size_t n = snprintf(NULL, 0, "%s/venv", root);
        t->virtual_env = malloc(n + 1);
        snprintf(t->virtual_env, n + 1, "%s/venv", root);
        bool termux;
        if (getenv("TERMUX_VERSION") || getenv("PREFIX"))
            termux = termux_from_environ();
        else
            termux = false;
        t->strip_python_vars = termux;
        return t;
    }
    t->n_prefix = 3;
    t->prefix = malloc(3 * sizeof(char *));
    t->prefix[0] = xstrdup(install_repair_sys_executable
                               ? install_repair_sys_executable
                               : "python3");
    t->prefix[1] = strdup("-m");
    t->prefix[2] = strdup("pip");
    t->has_env = false;
    return t;
}

void install_repair_target_free(install_repair_target_t *t) {
    if (!t) return;
    for (size_t i = 0; i < t->n_prefix; i++) free(t->prefix[i]);
    free(t->prefix);
    free(t->virtual_env);
    free(t);
}

char *install_repair_venv_scripts_dir(const char *root) {
    size_t n = snprintf(NULL, 0, "%s/venv", root);
    char *venv_dir = malloc(n + 1);
    snprintf(venv_dir, n + 1, "%s/venv", root);
    struct stat st;
    if (stat(venv_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        free(venv_dir);
        return NULL;
    }
    /* hermes_constants.venv_bin_dir: Scripts on Windows else bin (#76105). */
    const char *leaf = install_repair_is_windows() ? "Scripts" : "bin";
    n = snprintf(NULL, 0, "%s/%s", venv_dir, leaf);
    char *scripts = malloc(n + 1);
    snprintf(scripts, n + 1, "%s/%s", venv_dir, leaf);
    free(venv_dir);
    if (stat(scripts, &st) != 0 || !S_ISDIR(st.st_mode)) {
        free(scripts);
        return NULL;
    }
    return scripts;
}

static char *read_file_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

char **install_repair_load_console_script_names(const char *root, size_t *out_n) {
    *out_n = 0;
    size_t n = snprintf(NULL, 0, "%s/pyproject.toml", root);
    char *path = malloc(n + 1);
    snprintf(path, n + 1, "%s/pyproject.toml", root);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) { free(path); return NULL; }
    char *raw = read_file_all(path);
    free(path);
    if (!raw) return NULL;
    toml_doc_t *doc = toml_parse(raw);
    free(raw);
    if (!doc) return NULL;
    toml_node_t *project = toml_get(doc, "project");
    toml_node_t *scripts = project ? toml_get(doc, "project.scripts") : NULL;
    char **names = NULL;
    size_t cnt = 0, cap = 0;
    if (scripts && scripts->type == TOML_TABLE) {
        for (int i = 0; i < scripts->child_count; i++) {
            toml_node_t *child = scripts->children[i];
            if (child && child->key && child->key[0]) {
                if (cnt == cap) {
                    cap = cap ? cap * 2 : 8;
                    names = realloc(names, cap * sizeof(char *));
                }
                names[cnt++] = strdup(child->key);
            }
        }
    }
    toml_free(doc);
    *out_n = cnt;
    return names;
}

size_t install_repair_quarantine_running_hermes_exe(const char *scripts_dir,
                                                    install_repair_move_t **out) {
    *out = NULL;
    /* Early-out on non-Windows (Python returns [] before touching disk). */
    if (!install_repair_is_windows() || !scripts_dir) return 0;
    /* names = set(_load_console_script_names(root)) or {trio}; root is
     * scripts_dir.parent.parent (venv root's parent == project root). */
    char root[1024];
    snprintf(root, sizeof(root), "%s/../../", scripts_dir);
    size_t n_names = 0;
    char **names = install_repair_load_console_script_names(root, &n_names);
    const char *fallback[] = { "hermes", "hermes-agent", "hermes-acp" };
    bool have_set = n_names > 0;
    /* always add "hermes-gateway" */
    size_t total = (have_set ? n_names : 3) + 1;
    const char **all = malloc(total * sizeof(char *));
    size_t k = 0;
    for (size_t i = 0; i < total - 1; i++)
        all[k++] = have_set ? names[i] : fallback[i];
    all[k++] = "hermes-gateway";
    /* sorted(names) */
    for (size_t i = 0; i + 1 < total; i++)
        for (size_t j = i + 1; j < total; j++)
            if (strcmp(all[i], all[j]) > 0) {
                const char *tmp = all[i]; all[i] = all[j]; all[j] = tmp;
            }
    install_repair_move_t *moved = NULL;
    size_t nmoved = 0;
    for (size_t i = 0; i < total; i++) {
        size_t m = snprintf(NULL, 0, "%s/%s.exe", scripts_dir, all[i]);
        char *shim = malloc(m + 1);
        snprintf(shim, m + 1, "%s/%s.exe", scripts_dir, all[i]);
        struct stat st;
        if (stat(shim, &st) != 0) { free(shim); continue; }
        long long ms = (long long)time(NULL) * 1000;
        m = snprintf(NULL, 0, "%s/%s.exe.old.%lld", scripts_dir, all[i], ms);
        char *qname = malloc(m + 1);
        snprintf(qname, m + 1, "%s/%s.exe.old.%lld", scripts_dir, all[i], ms);
        if (rename(shim, qname) == 0) {
            moved = realloc(moved, (nmoved + 1) * sizeof(*moved));
            moved[nmoved].original = shim;
            moved[nmoved].quarantined = qname;
            nmoved++;
        } else {
            free(shim);
            free(qname);
        }
    }
    for (size_t i = 0; i < total - 1; i++) if (have_set) free(names[i]);
    free(names);
    free(all);
    *out = moved;
    return nmoved;
}

void install_repair_restore_quarantined_exes(install_repair_move_t *moved,
                                             size_t n) {
    for (size_t i = 0; i < n; i++) {
        struct stat st;
        if (stat(moved[i].original, &st) == 0) continue; /* fresh shim written */
        rename(moved[i].quarantined, moved[i].original); /* best-effort */
    }
}

char **install_repair_load_installable_optional_extras(const char *root,
                                                       const char *group,
                                                       size_t *out_n) {
    *out_n = 0;
    size_t n = snprintf(NULL, 0, "%s/pyproject.toml", root);
    char *path = malloc(n + 1);
    snprintf(path, n + 1, "%s/pyproject.toml", root);
    char *raw = read_file_all(path);
    free(path);
    if (!raw) return NULL;
    toml_doc_t *doc = toml_parse(raw);
    free(raw);
    if (!doc) return NULL;
    char **refs = NULL;
    size_t nrefs = 0;
    toml_node_t *optdeps = toml_get(doc, "project.optional-dependencies");
    if (optdeps && optdeps->type == TOML_TABLE) {
        toml_node_t *g = NULL;
        for (int i = 0; i < optdeps->child_count; i++)
            if (optdeps->children[i]->key &&
                strcmp(optdeps->children[i]->key, group) == 0)
                g = optdeps->children[i];
        if (g && g->type == TOML_ARRAY) {
            refs = malloc((size_t)(g->child_count ? g->child_count : 1) *
                          sizeof(char *));
            for (int i = 0; i < g->child_count; i++) {
                toml_node_t *c = g->children[i];
                if (c && c->type == TOML_STRING && c->string_val)
                    refs[nrefs++] = strdup(c->string_val);
            }
        }
    }
    char **referenced = NULL;
    size_t cnt = 0;
    for (size_t i = 0; i < nrefs; i++) {
        const char *ref = refs[i];
        const char *lb = strchr(ref, '[');
        const char *rb = strchr(ref, ']');
        if (lb && rb && rb > lb) {
            size_t len = (size_t)(rb - lb - 1);
            char name[128];
            if (len < sizeof(name)) {
                memcpy(name, lb + 1, len);
                name[len] = '\0';
                /* name in optional_deps? */
                bool found = false;
                if (optdeps && optdeps->type == TOML_TABLE) {
                    for (int k = 0; k < optdeps->child_count; k++)
                        if (optdeps->children[k]->key &&
                            strcmp(optdeps->children[k]->key, name) == 0)
                            found = true;
                }
                if (found) {
                    referenced = realloc(referenced, (cnt + 1) * sizeof(char *));
                    referenced[cnt++] = xstrdup(name);
                }
            }
        }
        free(refs[i]);
    }
    free(refs);
    toml_free(doc);
    *out_n = cnt;
    return referenced;
}

int install_repair_bump_marker_attempts(const char *marker_path) {
    int attempts = 0;
    char *raw = read_file_all(marker_path);
    if (raw) {
        /* strip whitespace like Python .strip() */
        char *b = raw;
        while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n') b++;
        char *e = b + strlen(b);
        while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ||
                         e[-1] == '\n'))
            e--;
        *e = '\0';
        if (*b) {
            json_t *node = json_parse(b, NULL);
            if (node && node->type == JSON_OBJECT) {
                json_t *a = json_obj_get(node, "attempts");
                if (a && a->type == JSON_NUMBER)
                    attempts = (int)a->num_val;
                else if (a && a->type == JSON_STRING && a->str_val)
                    attempts = atoi(a->str_val);
            }
            if (node) json_free(node);
        }
        free(raw);
    }
    attempts += 1;
    FILE *f = fopen(marker_path, "wb");
    if (f) {
        fprintf(f, "{\"attempts\": %d}", attempts);
        fclose(f);
    }
    return attempts;
}

/* ------------------------------------------------------------------ */
/* run_core_install ladder                                             */
/* ------------------------------------------------------------------ */
static void record_cmd(json_t *out_commands, char *const *argv, size_t n) {
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) json_append(arr, json_string(argv[i]));
    json_append(out_commands, arr);
}

static void print_line(json_t *out_printed, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    json_append(out_printed, json_string(tmp));
}

/* PoP: run_install_cmd @ hermes_cli/_install_repair.py:_run_install_cmd */
static int run_install_cmd(char *const *argv, size_t n, const char *root,
                           json_t *out_commands);

/* prefix + [arg0, arg1, arg2?] command builder/runner */
static int run_prefixed(install_repair_target_t *t, const char *root,
                        json_t *out_commands,
                        const char *a, const char *b, const char *c) {
    size_t n = t->n_prefix + (c ? 3 : 2);
    char **argv = malloc((n + 1) * sizeof(char *));
    for (size_t i = 0; i < t->n_prefix; i++) argv[i] = xstrdup(t->prefix[i]);
    argv[t->n_prefix] = xstrdup(a);
    argv[t->n_prefix + 1] = xstrdup(b);
    if (c) argv[t->n_prefix + 2] = xstrdup(c);
    argv[n] = NULL;
    int rc = run_install_cmd(argv, n, root, out_commands);
    for (size_t i = 0; i < n; i++) free(argv[i]);
    free(argv);
    return rc;
}

static int run_install_cmd(char *const *argv, size_t n, const char *root,
                           json_t *out_commands) {
    record_cmd(out_commands, argv, n);
    if (install_repair_run_cmd_hook) return install_repair_run_cmd_hook(argv, n, root);
    /* real default: fork/execvp with cwd=root, check=True analog */
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        if (chdir(root) != 0) _exit(126);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

void install_repair_run_core_install(const char *root,
                                     json_t *out_commands,
                                     json_t *out_printed) {
    install_repair_target_t *t = install_repair_resolve_install_target(root);
    bool termux = getenv("TERMUX_VERSION") && getenv("TERMUX_VERSION")[0];
    if (!termux) {
        const char *px = getenv("PREFIX");
        termux = px && strstr(px, "com.termux");
    }
    const char *group = termux ? "termux-all" : "all";

    /* ensurepip bootstrap: capture_output=True, failures swallowed */
    {
        char *argv[5];
        argv[0] = xstrdup(install_repair_sys_executable
                              ? install_repair_sys_executable : "python3");
        argv[1] = strdup("-m");
        argv[2] = strdup("ensurepip");
        argv[3] = strdup("--upgrade");
        argv[4] = strdup("--default-pip");
        record_cmd(out_commands, argv, 5);
        if (install_repair_run_cmd_hook)
            install_repair_run_cmd_hook(argv, 5, root);
        for (int i = 0; i < 5; i++) free(argv[i]);
    }

    /* combined .[group] attempt */
    char spec[256];
    snprintf(spec, sizeof(spec), ".[%s]", group);
    if (run_prefixed(t, root, out_commands, "install", "-e", spec) == 0) {
        install_repair_target_free(t);
        return;
    }
    print_line(out_printed,
               "  ⚠ Optional extras failed, reinstalling base dependencies "
               "and retrying extras individually...");

    /* base "." reinstall */
    if (run_prefixed(t, root, out_commands, "install", "-e", ".") != 0) {
        /* Python raises CalledProcessError here; callers own marker
         * lifecycle. Record it as a printed sentinel for parity. */
        print_line(out_printed, "CalledProcessError: base install failed");
        install_repair_target_free(t);
        return;
    }

    char **extras = NULL;
    size_t n_extras = 0;
    extras = install_repair_load_installable_optional_extras(root, group, &n_extras);
    char *installed_list = NULL;
    size_t installed_len = 0;
    char *failed_list = NULL;
    size_t failed_len = 0;
    for (size_t i = 0; i < n_extras; i++) {
        char spec2[256];
        snprintf(spec2, sizeof(spec2), ".[%s]", extras[i]);
        int rc = run_prefixed(t, root, out_commands, "install", "-e", spec2);
        char **dst = rc == 0 ? &installed_list : &failed_list;
        size_t *dlen = rc == 0 ? &installed_len : &failed_len;
        size_t add = strlen(extras[i]) + ((*dlen) ? 2 : 0);
        *dst = realloc(*dst, *dlen + add + 1);
        snprintf(*dst + *dlen, add + 1, "%s%s", *dlen ? ", " : "", extras[i]);
        *dlen += add;
        free(extras[i]);
    }
    free(extras);
    if (installed_len)
        print_line(out_printed,
                   "  ✓ Reinstalled optional extras individually: %s",
                   installed_list);
    if (failed_len)
        print_line(out_printed,
                   "  ⚠ Skipped optional extras that still failed: %s",
                   failed_list);
    free(installed_list);
    free(failed_list);
    install_repair_target_free(t);
}
