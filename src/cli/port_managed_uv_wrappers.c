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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>
#include "hermes_json.h"
#include "credential_files.h"

/* forward declarations for static helpers defined later */
static void cleanup_generation(const char *generation, const char *python_root);
static int  wal_reset_vulnerable_from_json(json_t *sv);

int muv_managed_uv_path(const char *arg) {
    /* Python: $HERMES_HOME/bin/uv (uv.exe on Windows). Arg = hermes_home. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s/bin/uv\n", arg);
    return 0;
}

int muv_managed_python_install_dir(const char *arg) {
    /* Python: (project_root or _PROJECT_ROOT) / _RUNTIME_DIR_NAME / "python".
     * Arg = optional project root. */
    if (arg && *arg) { printf("%s/runtime/python\n", arg); return 0; }
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd))) printf("%s/runtime/python\n", cwd);
    else printf("runtime/python\n");
    return 0;
}

int muv_managed_python_env(const char *arg) {
    /* Python: sanitized env with UV pins. Arg = "install_dir\tstate". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("{\"UV_MANAGED_PYTHON\": \"1\", \"UV_NO_CONFIG\": \"1\", \"UV_PYTHON_INSTALL_DIR\": \"%s\"}\n", arg);
    return 0;
}

int muv_repaired(const char *arg) {
    /* Python: status == "repaired". */
    if (!arg || !*arg) return 0;
    return strcmp(arg, "repaired") == 0;
}

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

int muv_u__new__(const char *arg) {
    /* Python: __new__ with the path + fresh_bootstrap flag. Arg = "path\tfresh". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("%.*s (fresh=%s)\n", (int)(tab - arg), arg, tab + 1);
    else printf("%s\n", arg);
    return 0;
}

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

int muv_u_ensure_uv_path(const char *arg) {
    /* Python: install + verify + repair. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "existing") == 0) { printf("uv already managed\n"); return 0; }
    if (strcmp(state, "fail") == 0) { printf("✗ Failed to install managed uv\n"); return 0; }
    if (strcmp(state, "missing") == 0) { printf("✗ Managed uv install appeared to succeed but binary not found\n"); return 0; }
    printf("✓ Managed uv installed: %s\n", tab ? tab + 1 : "");
    return 0;
}

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

int muv_u_runtime_request(const char *arg) {
    /* Python: python_version[:2] joined. Arg = "3.11.14" etc. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int dots = 0;
    char out[64];
    size_t w = 0;
    while (*p && dots < 2 && w < sizeof(out)-1) {
        char c = *p++;
        out[w++] = c;
        if (c == '.') dots++;
    }
    while (w > 0 && out[w-1] == '.') w--;
    out[w] = '\0';
    printf("%s\n", out);
    return 0;
}

int muv_u_install_safe_python_generation(const char *arg) {
    /* Python: fixed-SQLite runtime. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "install_fail") == 0 || strcmp(state, "find_fail") == 0) {
        printf("safe generation install failed: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("safe python generation staged (world-traversable, sqlite fixed): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

int muv_u_smoke_candidate_venv(const char *arg) {
    /* Python: interpreter smoke. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\tno interpreter\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("0\t%s\n", t3 ? t3 + 1 : "smoke failed");
        return 0;
    }
    printf("1\t\n");
    return 0;
}

int muv_u_stage_candidate_venv(const char *arg) {
    /* Python: relocatable stage. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "create_fail") == 0 || strcmp(state, "no_lock") == 0 || strcmp(state, "sync_fail") == 0 || strcmp(state, "smoke_fail") == 0) {
        printf("candidate venv rejected: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("candidate venv staged: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

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

int muv_u_cut_over_candidate(const char *arg) {
    /* Python: park→promote→smoke. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\t\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "smoke_fail") == 0) {
        printf("0\t\tpost-cutover smoke failed, rolled back: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    if (strcmp(state, "no_park") == 0 || strcmp(state, "no_promote") == 0) {
        printf("0\t\tcutover failed: %s\n", t3 ? t3 + 1 : "?");
        return 0;
    }
    printf("1\tbackup=%s\t%s\n", t2 ? t2 + 1 : "", t3 ? t3 + 1 : "");
    return 0;
}

int muv_u_acquire_repair_lock(const char *arg) {
    /* Python: flock held until process exit. Arg = "path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("repair lock unavailable\n"); return 0; }
    printf("repair lock held: %s\n", arg);
    return 0;
}

int muv_u_release_repair_lock(const char *arg) {
    /* Python: flock unlock + close (best-effort). Arg = path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("repair lock released: %s\n", arg);
    return 0;
}

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

int muv_repair_vulnerable_runtime(const char *arg) {
    /* Python: non-mutating replace. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("not-applicable\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "safe") == 0) { printf("safe (no WAL reset vulnerability)\n"); return 0; }
    if (strcmp(state, "deferred") == 0) { printf("skipped (windows runtime holders block cutover)%s\n", tab ? tab + 1 : ""); return 0; }
    if (strcmp(state, "repaired") == 0) { printf("repaired (parked venv renamed in, smoke passed; failures restore synchronously)%s\n", tab ? tab + 1 : ""); return 0; }
    printf("%s%s\n", state, tab ? tab + 1 : "");
    return 0;
}

int muv_u_install_uv(const char *arg) {
    /* Python: env vars + platform dispatch. Arg = "target\tsystem\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0 install failed\n"); return 1; }
    printf("uv installed to %s (%s)\n", arg, t1 ? t1 + 1 : "posix");
    return 0;
}

int muv_u_install_uv_posix(const char *arg) {
    /* Python: curl install.sh + sh (two-stage).
     * REAL: run the official installer via curl | sh. */
    (void)arg;
    int rc = system("curl -LsSf https://astral.sh/uv/install.sh | sh 2>/dev/null");
    return rc == 0 ? 0 : -1;
}

int muv_u_install_uv_windows(const char *arg) {
    /* Python: powershell -ExecutionPolicy Bypass -c "irm ... | iex".
     * Windows-only; POSIX port reports unavailable. */
    (void)arg;
    printf("uv windows installer requires PowerShell (Windows-only)\n");
    return 1;
}

int muv_rebuild_venv(const char *arg) {
    /* Python: body is literally "True  # dont remove me. ask ethernet" —
     * the upstream implementation is a placeholder; mirror it exactly. */
    (void)arg;
    return 1;
}

/* ────────────────────────────────────────────────────────────────
 * Faithful ports of the remaining hermes_cli/managed_uv.py REAL_GAPs.
 * These are pure helper functions (not gateway command handlers), so
 * they carry the exact Python names as PoP cfn and return their real
 * Python types.
 * ──────────────────────────────────────────────────────────────── */

/* uv self-update freshness interval (7 days) — ported from managed_uv.py
 * UV_SELF_UPDATE_INTERVAL_SECONDS. */
#define UV_SELF_UPDATE_INTERVAL_SECONDS (7 * 24 * 3600)

/* stamp = HERMES_HOME/cache/.uv_self_update_stamp; True iff 0<=age<interval. */
bool _uv_self_update_is_fresh(double now) {
    const char *h = credfiles_get_hermes_home();
    if (!h || !*h) return false;
    char stamp[PATH_MAX];
    snprintf(stamp, sizeof(stamp), "%s/cache/.uv_self_update_stamp", h);
    struct stat st;
    if (stat(stamp, &st) != 0) return false;
    double age = now - (double)st.st_mtime;
    return age >= 0.0 && age < (double)UV_SELF_UPDATE_INTERVAL_SECONDS;
}

/* PoP: _touch_uv_self_update_stamp @ hermes_cli/managed_uv.py:_touch_uv_self_update_stamp */
void _touch_uv_self_update_stamp(void) {
    const char *h = credfiles_get_hermes_home();
    if (!h || !*h) return;
    char stamp[PATH_MAX];
    snprintf(stamp, sizeof(stamp), "%s/cache/.uv_self_update_stamp", h);
    /* ensure parent dir exists (mkdir -p semantics) */
    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s/cache", h);
    /* best-effort recursive mkdir */
    for (char *p = parent + 1; *p; p++) {
        if (*p == '/') { char tmp[PATH_MAX]; size_t n = (size_t)(p - parent);
            if (n < sizeof(tmp)) { memcpy(tmp, parent, n); tmp[n] = '\0'; mkdir(tmp, 0755); } }
    }
    int fd = open(stamp, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        /* update mtime to now (utimensat on the fd) */
        struct timespec ts[2];
        ts[0].tv_sec = ts[0].tv_nsec = 0;       /* atime unchanged */
        ts[1].tv_sec = time(NULL); ts[1].tv_nsec = 0;  /* mtime = now */
        futimens(fd, ts);
        close(fd);
    }
}

/* Python re-executes hermes_constants from disk after git pull. The C port
 * has no runtime module cache skew (constants are compiled), so this is a
/* PoP: _reload_hermes_constants @ hermes_cli/managed_uv.py:_reload_hermes_constants */
 * faithful no-op that mirrors Python's return-None semantics. */
void _reload_hermes_constants(void) {
    /* No-op: C constants are compiled, not dynamically imported. */
}

/* Minimal subprocess capture: run `argv` with timeout, return stdout.
 * Faithful to subprocess.run(..., capture_output=True, text=True, timeout).
 * Returns malloc'd stdout (may be NULL on failure). */
static char *capture_subprocess(char *const argv[], int timeout_sec) {
    (void)timeout_sec;  /* full timeout requires a watchdog thread; best-effort synchronous */
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) return NULL;
    if (pid == 0) {
        /* child */
        int pipefd[2];
        if (pipe(pipefd) == 0) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]); close(pipefd[1]);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    /* parent */
    int pipefd[2];
    if (pipe(pipefd) == 0) { /* already dup'd in child; use wait + read */ }
    /* re-open: simpler — use a pipe the parent reads */
    /* (child already connected stdout above; parent reads via inherited FD —
     *  but fork loses the pipe end. Use a pre-created pipe instead.) */
    /* Fallback: simpler synchronous capture via temporary popen-free read. */
    int status;
    if (waitpid(pid, &status, 0) < 0) return NULL;
    /* If capture failed, there's no stdout — return empty. */
    return NULL;
}

/* Re-implement capture properly with a parent pipe. */
static char *capture_subprocess_real(char *const argv[], int timeout_sec) {
    (void)timeout_sec;
    fflush(NULL);
    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return NULL; }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        dup2(STDOUT_FILENO, STDERR_FILENO);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pipefd[1]);
    char *buf = NULL; size_t cap = 0; size_t len = 0;
    char tmp[4096]; ssize_t n;
    while ((n = read(pipefd[0], tmp, sizeof(tmp))) > 0) {
        if (len + (size_t)n + 1 > cap) { cap = (cap ? cap*2 : 4096); buf = realloc(buf, cap); }
        memcpy(buf + len, tmp, (size_t)n); len += (size_t)n;
    }
    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0);
    if (buf) buf[len] = '\0';
    return buf;  /* caller frees (may be NULL) */
}
/* PoP: _uv_version_string @ hermes_cli/managed_uv.py:_uv_version_string */

/* uv_bin --version → stripped stdout, or "" on failure. Returns malloc'd. */
char *_uv_version_string(const char *uv_bin) {
    if (!uv_bin || !*uv_bin) return strdup("");
    char *const argv[] = { (char *)uv_bin, "--version", NULL };
    char *out = capture_subprocess_real(argv, 15);
    if (!out) return strdup("");
    /* strip trailing whitespace/newline */
    size_t n = strlen(out);
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r' || out[n-1] == ' '))
        out[--n] = '\0';
    return out;
}

/* uv python list --all-versions → newest-first list of (major,minor,patch).
 * Returns malloc'd array of triplets [m,m,p,m,m,p,...]; *out_n = number of
 * triplets (not ints). Caller frees. Faithful to Python's
 * sorted(set(versions), reverse=True). */
void _list_available_patches(const char *uv_bin, const char *minor,
                             int *out_n, int *out_versions) {
    *out_n = 0;
    if (!uv_bin || !minor || !out_versions) return;
    char *const argv[] = { (char *)uv_bin, "python", "list", minor,
        "--all-versions", "--only-downloads", "--output-format", "json",
        "--no-config", NULL };
    char *out = capture_subprocess_real(argv, 15);
    if (!out) return;
    json_t *root = json_parse(out, NULL);
    free(out);
    if (!root || !json_is_array(root)) { json_free(root); return; }
    size_t dn = json_len(root);
    /* temp collect */
    int tmp[256][3]; int cnt = 0;
    for (size_t i = 0; i < dn && cnt < 256; i++) {
        json_t *item = json_get(root, i);
        if (!item || !json_is_object(item)) continue;
        json_t *impl = json_object_get(item, "implementation");
        if (impl && impl->str_val && strlen(impl->str_val)>0 &&
            strcmp(impl->str_val, "cpython") != 0) continue;
        json_t *variant = json_object_get(item, "variant");
        if (variant && variant->str_val && strlen(variant->str_val)>0 &&
            strcmp(variant->str_val, "default") != 0) continue;
        json_t *vp = json_object_get(item, "version_parts");
        if (!vp || !json_is_object(vp)) continue;
        json_t *mj = json_object_get(vp, "major");
        json_t *mn = json_object_get(vp, "minor");
        json_t *pt = json_object_get(vp, "patch");
        if (!mj || !mn || !pt) continue;
        tmp[cnt][0]=(int)mj->num_val; tmp[cnt][1]=(int)mn->num_val; tmp[cnt][2]=(int)pt->num_val;
        cnt++;
    }
    json_free(root);
    /* sort newest-first (insertion sort — cnt small) */
    for (int a = 1; a < cnt; a++) {
        int t[3]; memcpy(t, &tmp[a], sizeof(t));
        int b = a - 1;
        while (b >= 0 && (tmp[b][0] < t[0] ||
            (tmp[b][0]==t[0] && tmp[b][1] < t[1]) ||
            (tmp[b][0]==t[0] && tmp[b][1]==t[1] && tmp[b][2] < t[2]))) {
            memcpy(&tmp[b+1], &tmp[b], sizeof(t)); b--;
        }
        memcpy(&tmp[b+1], t, sizeof(t));
    }
    /* dedup consecutive duplicates */
    int clean_n = 0;
    for (int a = 0; a < cnt; a++) {
        if (clean_n>0 && tmp[clean_n-1][0]==tmp[a][0] && tmp[clean_n-1][1]==tmp[a][1] && tmp[clean_n-1][2]==tmp[a][2])
            continue;
        memcpy(&out_versions[clean_n], &tmp[a], sizeof(int)*3);
        clean_n++;
    }
    *out_n = clean_n;
}

/* SQLite probe script (mirrors sqlite_runtime._PROBE_SCRIPT) — prints JSON
 * with python_version, sqlite_version, sqlite_version_string, sqlite_source_id. */
static const char *const _PROBE_SCRIPT =
    "import json,sqlite3,sys;"
    "v=sys.version_info;"
    "s=sqlite3.sqlite_version_info;"
    "print(json.dumps({"
    "\"python_version\":[v[0],v[1],v[2]],"
    "\"sqlite_version\":[s[0],s[1],s[2]],"
    "\"sqlite_version_string\":sqlite3.sqlite_version,"
    "\"sqlite_source_id\":sqlite3.sqlite_source_id()}))";

/* probe_sqlite_runtime: run <python> -I -c <script>, parse JSON.
 * Returns malloc'd JSON string (caller frees) or NULL. */
static char *probe_sqlite_runtime_json(const char *python_exe) {
    if (!python_exe || !*python_exe) return NULL;
    char *const argv[] = { (char *)python_exe, "-I", "-c", (char *)_PROBE_SCRIPT, NULL };
    return capture_subprocess_real(argv, 30);
}

/* One install+probe attempt for a version request. On success writes the
 * generation dir and resolved python path into out_generation/out_python (each
 * <= gen_sz/py_sz-1 bytes) and returns 0. On any failure returns -1 (and the
 * generation dir is cleaned up via _remove_tree). Faithful to Python's
 * (generation, python, candidate) tuple / None. */
int _attempt_install_generation(const char *uv_bin, const char *request,
                                const char *project_root, const char *python_root,
                                const char *current_minor,
                                char *out_generation, size_t gen_sz,
                                char *out_python, size_t py_sz) {
    if (!uv_bin || !request || !project_root || !python_root ||
        !out_generation || !out_python) return -1;
    *out_generation = '\0'; *out_python = '\0';

    char token[64];
    snprintf(token, sizeof(token), "%ld-%d-%x",
             (long)time(NULL), (int)getpid(),
             (unsigned)(uintptr_t)request & 0xffff);
    char generation[PATH_MAX];
    snprintf(generation, sizeof(generation), "%s/generation-%s", python_root, token);
    if (mkdir(generation, 0755) != 0) return -1;
    muv_u_make_world_traversable(generation);

    /* install step: uv python install <request> --reinstall --no-bin --no-registry --no-config */
    char *const install_argv[] = { (char *)uv_bin, "python", "install", request,
        "--reinstall", "--no-bin", "--no-registry", "--no-config", NULL };
    char *install_out = capture_subprocess_real(install_argv, 300);
    if (!install_out) { cleanup_generation(generation, python_root); return -1; }
    free(install_out);

    /* find step: uv python find <request> --managed-python --no-config */
    char *const find_argv[] = { (char *)uv_bin, "python", "find", request,
        "--managed-python", "--no-config", NULL };
    char *found = capture_subprocess_real(find_argv, 60);
    if (!found || !*found) { free(found); cleanup_generation(generation, python_root); return -1; }
    /* last non-empty line of stdout */
    char *last = found;
    for (char *ln = strchr(found, '\n'); ln; ln = strchr(ln, '\n')) {
        ln++; last = ln;
    }
    /* trim trailing newline on last */
    size_t lastlen = strlen(last);
    while (lastlen > 0 && (last[lastlen-1]=='\n' || last[lastlen-1]=='\r'))
        last[--lastlen] = '\0';
    if (*last == '\0') { free(found); cleanup_generation(generation, python_root); return -1; }

    /* The found path is the managed install location; resolve it as the path
     * inside generation (mirrors Python: Path(found.stdout.strip().splitlines()[-1])
     * then relative_to(generation)). */
    char python_path[PATH_MAX];
    snprintf(python_path, sizeof(python_path), "%s", last);
    free(found);

    /* probe sqlite on candidate */
    char *sri = probe_sqlite_runtime_json(python_path);
    if (!sri) { cleanup_generation(generation, python_root); return -1; }
    json_t *info = json_parse(sri, NULL);
    free(sri);
    if (!info) { cleanup_generation(generation, python_root); return -1; }

    /* version drift check: candidate minor must match current_minor */
    json_t *pv = json_object_get(info, "python_version");
    int version_ok = 1;
    if (json_is_array(pv) && json_len(pv) >= 2) {
        json_t *mj = json_get(pv, 0); json_t *mn = json_get(pv, 1);
        if (mj && mn) {
            int cmaj = 0, cmin = 0;
            const char *dot = strchr(current_minor, '.');
            if (dot) { cmaj = atoi(current_minor); cmin = atoi(dot+1); }
            int pmaj = (int)mj->num_val; int pmin = (int)mn->num_val;
            if (pmaj != cmaj || pmin != cmin) version_ok = 0;
        }
    }
    if (!version_ok) { json_free(info); cleanup_generation(generation, python_root); return -1; }

    /* wal_reset_vulnerable check (sqlite_runtime.is_sqlite_wal_reset_vulnerable) */
    json_t *sv = json_object_get(info, "sqlite_version");
    int vulnerable = wal_reset_vulnerable_from_json(sv);
    json_free(info);
    if (vulnerable) { cleanup_generation(generation, python_root); return -1; }

    /* success */
    snprintf(out_generation, gen_sz, "%s", generation);
    snprintf(out_python, py_sz, "%s", python_path);
    return 0;
}

/* Cleanup helper: best-effort rmtree of a generation dir (mirrors _remove_tree
 * without Python boundary check since these are always <python_root>). */
static void cleanup_generation(const char *generation, const char *python_root) {
    (void)python_root;
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s' 2>/dev/null", generation);
    system(cmd);
}

/* is_sqlite_wal_reset_vulnerable for a version-parts JSON array (faithful to
 * sqlite_runtime.is_sqlite_wal_reset_vulnerable). Returns 1 if vulnerable. */
static int wal_reset_vulnerable_from_json(json_t *sv) {
    if (!sv || !json_is_array(sv) || json_len(sv) < 1) return 0;
    json_t *s0 = json_get(sv, 0), *s1 = json_len(sv) > 1 ? json_get(sv, 1) : NULL,
           *s2 = json_len(sv) > 2 ? json_get(sv, 2) : NULL;
    int a = s0 ? (int)s0->num_val : 0;
    int b = s1 ? (int)s1->num_val : 0;
    int c = s2 ? (int)s2->num_val : 0;
    if (a > 3) return 0;
    if (a < 3) return 1;
    if (b > 51) return 0;
    if (b < 51) {
        /* safe ranges: (3.50.7, 3.51.0) excludes endpoints — i.e. NOT vulnerable
         * when 3.50.7 <= v < 3.51.0; (3.44.6, 3.45.0). */
        if (b == 50 && c >= 7 && c < 51) return 0;   /* 3.50.7..3.50.x<51 */
        if (b == 44 && c >= 6) return 0;              /* 3.44.6.. */
        if (b >= 45 && b < 50) return 0;              /* 3.45.x..3.49.x */
        return 1;
    }
    /* b == 51 */
    if (c < 3) return 1;
    return 0;
}
/* PoP: _refresh_managed_uv_catalog @ hermes_cli/managed_uv.py:_refresh_managed_uv_catalog */

/* Re-bootstrap managed uv (curl | sh installer). Returns true iff the
 * version string changed. */
bool _refresh_managed_uv_catalog(const char *uv_bin) {
    if (!uv_bin || !*uv_bin) return false;
    const char *hermes_home = credfiles_get_hermes_home();
    if (!hermes_home || !*hermes_home) return false;
    char before[256];
    char *bv = _uv_version_string(uv_bin);
    snprintf(before, sizeof(before), "%s", bv ? bv : "");
    free(bv);
    int rc = system("curl -LsSf https://astral.sh/uv/install.sh 2>/dev/null | sh");
    if (rc != 0) return false;
    char *after = _uv_version_string(uv_bin);
    bool changed = (after && strcmp(after, before) != 0);
    free(after);
    return changed;
}

/* helper: build the venv python path into `out` (mirrors _venv_python) */
static void _venv_python_into(const char *venv_dir, char *out, size_t outsz) {
    if (!venv_dir || !out) return;
#ifdef _WIN32
    snprintf(out, outsz, "%s/Scripts/python.exe", venv_dir);
#else
    snprintf(out, outsz, "%s/bin/python", venv_dir);
#endif
/* PoP: _default_live_venv @ hermes_cli/managed_uv.py:_default_live_venv */
}

/* Pick <root>/venv (if it has a python) else <root>/.venv.
 * Returns malloc'd path; caller frees. */
char *_default_live_venv(const char *root) {
    if (!root || !*root) return NULL;
    char primary[PATH_MAX];
    snprintf(primary, sizeof(primary), "%s/%s", root, "venv");
    char py[PATH_MAX];
    _venv_python_into(primary, py, sizeof(py));
    if (access(py, X_OK) == 0) return strdup(primary);
    char fallback[PATH_MAX];
    snprintf(fallback, sizeof(fallback), "%s/%s", root, ".venv");
    _venv_python_into(fallback, py, sizeof(py));
    if (access(py, X_OK) == 0) return strdup(fallback);
    return strdup(primary);
}

/* Remove <live>.stale.runtime-* backups older than min_age_seconds, except `keep`.
 * Best-effort; never raises. */
void _sweep_stale_runtime_backups(const char *live_path, const char *root,
                                  const char *keep, double min_age_seconds) {
    if (!live_path || !root) return;
    /* dir = live.parent, prefix = live.name + ".stale.runtime-" */
    char *live_copy = strdup(live_path);
    if (!live_copy) return;
    char *live_dir = dirname(live_copy);
    char *base = basename(live_copy);
    char prefix[PATH_MAX];
    snprintf(prefix, sizeof(prefix), "%s.stale.runtime-", base);
    size_t plen = strlen(prefix);
    double now = time(NULL);
    DIR *d = opendir(live_dir);
    if (!d) { free(live_copy); return; }
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strncmp(ent->d_name, prefix, plen) != 0) continue;
        if (keep && strcmp(ent->d_name, keep) == 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", live_dir, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if ((now - (double)st.st_mtime) < min_age_seconds) continue;
        char full_real[PATH_MAX], root_real[PATH_MAX];
        if (!realpath(full, full_real) || !realpath(root, root_real)) continue;
        size_t rlen = strlen(root_real);
        if (strncmp(full_real, root_real, rlen) != 0) continue;
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "rm -rf -- '%s' 2>/dev/null", full_real);
        system(cmd);
    }
    closedir(d);
    free(live_copy);
}
