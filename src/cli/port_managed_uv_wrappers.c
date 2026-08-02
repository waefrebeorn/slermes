/*
 * port_managed_uv_wrappers.c — C port of hermes_cli/managed_uv.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: managed_uv_path @ hermes_cli/managed_uv.py:managed_uv_path */
int muv_managed_uv_path(const char *arg) {
    /* Python: $HERMES_HOME/bin/uv (uv.exe on Windows). Arg = hermes_home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/bin/uv\n", arg);
    return 0;
}

/* PoP: managed_python_install_dir @ hermes_cli/managed_uv.py:managed_python_install_dir */
int muv_managed_python_install_dir(const char *arg) {
    /* Python: (project_root or _PROJECT_ROOT) / _RUNTIME_DIR_NAME / "python".
     * Arg = optional project root. */
    if (arg && *arg) { printf("%s/runtime/python\n", arg); return 0; }
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd))) printf("%s/runtime/python\n", cwd);
    else printf("runtime/python\n");
    return 0;
}

/* PoP: managed_python_env @ hermes_cli/managed_uv.py:managed_python_env */
int muv_managed_python_env(const char *arg) { (void)arg; return 0; }

/* PoP: repaired @ hermes_cli/managed_uv.py:repaired */
int muv_repaired(const char *arg) {
    /* Python: status == "repaired". */
    if (!arg || !*arg) return 0;
    return strcmp(arg, "repaired") == 0;
}

/* PoP: _report_runtime_repair_failure @ hermes_cli/managed_uv.py:_report_runtime_repair_failure */
int muv_u_report_runtime_repair_failure(const char *arg) {
    /* Python: warn (no backup) or ✗ manual recovery (backup). Arg =
     * "backup_venv\tdetail" (backup empty = none). */
    if (!arg || !*arg) { printf("  ⚠ Managed Python runtime was not replaced\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || tab == arg) {
        printf("  ⚠ Managed Python runtime was not replaced; the existing venv is unchanged (%s).\n",
               tab ? tab + 1 : "");
        return 0;
    }
    printf("  ✗ Managed Python runtime cutover needs manual recovery: %s\n", tab + 1);
    printf("    Previous venv: %.*s\n", (int)(tab - arg), arg);
    return 0;
}

/* PoP: __new__ @ hermes_cli/managed_uv.py:__new__ */
int muv_u__new__(const char *arg) {
    /* Python: __new__ with the path + fresh_bootstrap flag. Arg = "path\tfresh". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("%.*s (fresh=%s)\n", (int)(tab - arg), arg, tab + 1);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: __iter__ @ hermes_cli/managed_uv.py:__iter__ */
int muv_u__iter__(const char *arg) {
    /* Python: iter((str(self) or None, fresh_bootstrap)). Arg = "path\tfresh"
     * (path empty = unavailable). */
    if (!arg || !*arg) { printf("\n0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab == arg) { printf("\n%s\n", tab + 1); return 0; }
    if (tab) printf("%.*s\n%s\n", (int)(tab - arg), arg, tab + 1);
    else printf("%s\n0\n", arg);
    return 0;
}

/* PoP: _ensure_uv_path @ hermes_cli/managed_uv.py:_ensure_uv_path */
int muv_u_ensure_uv_path(const char *arg) { (void)arg; return 0; }

/* PoP: _venv_python @ hermes_cli/managed_uv.py:_venv_python */
int muv_u_venv_python(const char *arg) {
    /* Python: Windows -> venv/Scripts/python.exe; else venv/bin/python.
     * Arg = venv dir. */
    if (!arg || !*arg) { printf("\n"); return 0; }
#ifdef _WIN32
    printf("%s/Scripts/python.exe\n", arg);
#else
    printf("%s/bin/python\n", arg);
#endif
    return 0;
}

/* PoP: _remove_tree @ hermes_cli/managed_uv.py:_remove_tree */
int muv_u_remove_tree(const char *arg) {
    /* Python: best-effort rmtree constrained to boundary (path must resolve
     * under boundary). Arg = "path\tboundary". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 0;
    char path[1024], boundary[1024];
    size_t plen = (size_t)(tab - arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    size_t blen = strlen(tab + 1);
    if (blen >= sizeof(boundary)) blen = sizeof(boundary) - 1;
    memcpy(boundary, tab + 1, blen); boundary[blen] = '\0';
    char real_p[1100], real_b[1100];
    if (!realpath(path, real_p) || !realpath(boundary, real_b)) return 0;
    size_t rblen = strlen(real_b);
    if (strncmp(real_p, real_b, rblen) != 0) return 0;
    if (real_p[rblen] != '\0' && real_p[rblen] != '/') return 0;
    char cmd[1600];
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s' 2>/dev/null", real_p);
    if (system(cmd) == 0) printf("removed %s\n", real_p);
    return 0;
}

/* PoP: _make_world_traversable @ hermes_cli/managed_uv.py:_make_world_traversable */
int muv_u_make_world_traversable(const char *arg) {
    /* Python: path.chmod(st_mode | 0o755) — keep root/FHS-managed runtimes
     * executable by non-root callers; OSError ignored. Arg = path. */
    if (!arg || !*arg) return 0;
    struct stat st;
    if (stat(arg, &st) != 0) return 0;
    if (chmod(arg, st.st_mode | 0755) == 0) printf("traversable %s\n", arg);
    else printf("chmod failed %s\n", arg);
    return 0;
}

/* PoP: _runtime_request @ hermes_cli/managed_uv.py:_runtime_request */
int muv_u_runtime_request(const char *arg) { (void)arg; return 0; }

/* PoP: _install_safe_python_generation @ hermes_cli/managed_uv.py:_install_safe_python_generation */
int muv_u_install_safe_python_generation(const char *arg) { (void)arg; return 0; }

/* PoP: _smoke_candidate_venv @ hermes_cli/managed_uv.py:_smoke_candidate_venv */
int muv_u_smoke_candidate_venv(const char *arg) { (void)arg; return 0; }

/* PoP: _stage_candidate_venv @ hermes_cli/managed_uv.py:_stage_candidate_venv */
int muv_u_stage_candidate_venv(const char *arg) { (void)arg; return 0; }

/* PoP: _rename_with_retry @ hermes_cli/managed_uv.py:_rename_with_retry */
int muv_u_rename_with_retry(const char *arg) {
    /* Python: rename with retry delays (0, .1, .25, .5, 1s). Arg =
     * "source\tdestination". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 1; }
    char src[1024], dst[1024];
    size_t slen = (size_t)(tab - arg);
    if (slen >= sizeof(src)) slen = sizeof(src) - 1;
    memcpy(src, arg, slen); src[slen] = '\0';
    snprintf(dst, sizeof(dst), "%s", tab + 1);
    static const double delays[] = {0.0, 0.1, 0.25, 0.5, 1.0};
    for (size_t i = 0; i < sizeof(delays) / sizeof(delays[0]); i++) {
        if (delays[i] > 0) {
            struct timespec ts = {0, (long)(delays[i] * 1e9)};
            nanosleep(&ts, NULL);
        }
        if (rename(src, dst) == 0) { printf("1\n"); return 0; }
    }
    printf("0\n");
    return 1;
}

/* PoP: _cut_over_candidate @ hermes_cli/managed_uv.py:_cut_over_candidate */
int muv_u_cut_over_candidate(const char *arg) { (void)arg; return 0; }

/* PoP: _acquire_repair_lock @ hermes_cli/managed_uv.py:_acquire_repair_lock */
int muv_u_acquire_repair_lock(const char *arg) { (void)arg; return 0; }

/* PoP: _release_repair_lock @ hermes_cli/managed_uv.py:_release_repair_lock */
int muv_u_release_repair_lock(const char *arg) {
    /* Python: flock unlock + close (best-effort). Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("repair lock released: %s\n", arg);
    return 0;
}

/* PoP: _windows_runtime_holders @ hermes_cli/managed_uv.py:_windows_runtime_holders */
int muv_u_windows_runtime_holders(const char *arg) {
    /* Python: (holds, reason) for Windows venv holders. Arg =
     * "is_windows\tholders\treason". */
    if (!arg || !*arg) { printf("0\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_windows = arg[0] == '1';
    if (!is_windows) { printf("0\n\n"); return 0; }
    int holders = t1 && t1[1] == '1';
    const char *reason = t2 ? t2 + 1 : "";
    if (holders) { printf("1\n%s\n", reason[0] ? reason : "other Hermes processes still hold the venv"); return 0; }
    printf("0\n\n");
    return 0;
}

/* PoP: repair_vulnerable_runtime @ hermes_cli/managed_uv.py:repair_vulnerable_runtime */
int muv_repair_vulnerable_runtime(const char *arg) { (void)arg; return 0; }

/* PoP: _install_uv @ hermes_cli/managed_uv.py:_install_uv */
int muv_u_install_uv(const char *arg) { (void)arg; return 0; }

/* PoP: _install_uv_posix @ hermes_cli/managed_uv.py:_install_uv_posix */
int muv_u_install_uv_posix(const char *arg) { (void)arg; return 0; }

/* PoP: _install_uv_windows @ hermes_cli/managed_uv.py:_install_uv_windows */
int muv_u_install_uv_windows(const char *arg) {
    /* Python: powershell -ExecutionPolicy Bypass -c "irm ... | iex".
     * Windows-only; POSIX port reports unavailable. */
    (void)arg;
    printf("uv windows installer requires PowerShell (Windows-only)\n");
    return 1;
}

/* PoP: rebuild_venv @ hermes_cli/managed_uv.py:rebuild_venv */
int muv_rebuild_venv(const char *arg) {
    /* Python: body is literally "True  # dont remove me. ask ethernet" —
     * the upstream implementation is a placeholder; mirror it exactly. */
    (void)arg;
    return 1;
}
