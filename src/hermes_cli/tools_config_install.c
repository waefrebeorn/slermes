/* tools_config_install.c — faithful C11 port of the install / post-setup
 * surface of hermes_cli/tools_config.py.
 *
 * Reuses: tools_config_cua.h (cua-driver resolution, env, contract status),
 * tools/browser_use_cli (managed CLI install), tools/browser_tool install
 * probes, hermes_display.h print helpers.
 */
#define _GNU_SOURCE
#include "tools_config_install.h"
#include "tools_config_cua.h"
#include "hermes_display.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "tools_config_toolsets.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* _CUA_INSTALLER_TIMEOUT = 660 — must exceed the installer's own 600s
 * stale-lock recovery window plus headroom (issue #58762). */
#define CUA_INSTALLER_TIMEOUT 660
/* _CUA_LOCK_STALE_AFTER = 600 (LOCK_STALE_AFTER_SECONDS upstream). */
#define CUA_LOCK_STALE_AFTER 600

/* ── small local utilities ─────────────────────────────────────────────── */

static char *tci_venv_root(void);
static char *tci_venv_python(void);

static char *tci_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *tci_join(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0;
    char *out = (char *)malloc(la + lb + 2);
    if (!out) return NULL;
    memcpy(out, a ? a : "", la);
    size_t k = la;
    if (k && out[k - 1] != '/') out[k++] = '/';
    memcpy(out + k, b ? b : "", lb);
    out[k + lb] = '\0';
    return out;
}

static const char *tci_home(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    return (h && *h) ? h : "";
}

static char *tci_which(const char *cmd) {
    if (!cmd || !*cmd) return NULL;
    if (strchr(cmd, '/')) {
        struct stat st;
        if (stat(cmd, &st) == 0 && S_ISREG(st.st_mode) && access(cmd, X_OK) == 0)
            return tci_strdup(cmd);
        return NULL;
    }
    const char *path = getenv("PATH");
    if (!path || !*path) path = "/usr/local/bin:/usr/bin:/bin";
    const char *p = path;
    while (*p) {
        const char *sep = strchr(p, ':');
        size_t len = sep ? (size_t)(sep - p) : strlen(p);
        const char *dir = len ? p : ".";
        size_t dlen = len ? len : 1;
        char *cand = (char *)malloc(dlen + 1 + strlen(cmd) + 1);
        if (!cand) return NULL;
        memcpy(cand, dir, dlen);
        cand[dlen] = '/';
        strcpy(cand + dlen + 1, cmd);
        struct stat st;
        if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) && access(cand, X_OK) == 0)
            return cand;
        free(cand);
        if (!sep) break;
        p = sep + 1;
    }
    return NULL;
}

/* managed_uv.ensure_uv(): the managed copy under $HERMES_HOME/bin is never on
 * PATH, so check it before a bare PATH lookup. The Python tier also installs
 * uv when missing; during a C-side setup flow that network install step is
 * owned by the caller, so a missing uv resolves to NULL and the cascade
 * falls through to pip exactly like the Python FileNotFoundError branch. */
static char *tci_ensure_uv(void) {
    char *managed = tci_join(tci_home(), "bin/uv");
    if (managed) {
        struct stat st;
        if (stat(managed, &st) == 0 && S_ISREG(st.st_mode) &&
            access(managed, X_OK) == 0)
            return managed;
        free(managed);
    }
    return tci_which("uv");
}

/* venv python: <venv_root>/bin/python */
static char *tci_venv_python(void) {
    char *root = tci_venv_root();
    char *py = tci_join(root, "bin/python");
    free(root);
    return py;
}

/* PROJECT_ROOT analog: repo root for npm/cwd-scoped installs. */
static const char *tci_project_root(void) {
    const char *r = getenv("HERMES_PROJECT_ROOT");
    return (r && *r) ? r : ".";
}

/* Bounded subprocess capture. Returns exit status, -1 spawn failure,
 * -2 timeout. When envp is NULL inherits the current environment. */
typedef struct { char *out; char *err; } tci_cap_t;

static void tci_append(char **buf, size_t *len, size_t *cap,
                       const char *data, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t nc = (*cap ? *cap * 2 : 4096);
        while (nc < *len + n + 1) nc *= 2;
        char *nb = (char *)realloc(*buf, nc);
        if (!nb) return;
        *buf = nb;
        *cap = nc;
    }
    memcpy(*buf + *len, data, n);
    *len += n;
    (*buf)[*len] = '\0';
}

static int tci_run_capture(const char *const *argv, char *const *envp,
                           double timeout_s, bool capture, tci_cap_t *cap) {
    cap->out = NULL;
    cap->err = NULL;
    int op[2] = { -1, -1 }, ep[2] = { -1, -1 };
    if (capture) {
        if (pipe(op) != 0) return -1;
        if (pipe(ep) != 0) { close(op[0]); close(op[1]); return -1; }
    }
    pid_t pid = fork();
    if (pid < 0) {
        if (capture) { close(op[0]); close(op[1]); close(ep[0]); close(ep[1]); }
        return -1;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        if (capture) {
            dup2(op[1], STDOUT_FILENO);
            dup2(ep[1], STDERR_FILENO);
            close(op[0]); close(op[1]); close(ep[0]); close(ep[1]);
        }
        if (envp) execve(argv[0], (char *const *)argv, envp);
        else execv(argv[0], (char *const *)argv);
        _exit(127);
    }
    if (!capture) {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    close(op[1]);
    close(ep[1]);

    size_t olen = 0, ocap = 0, elen = 0, ecap = 0;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bool timed_out = false;
    int open_fds = 2;
    while (open_fds > 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - t0.tv_sec) +
                         (double)(now.tv_nsec - t0.tv_nsec) / 1e9;
        double remain = timeout_s - elapsed;
        if (remain <= 0) { timed_out = true; break; }
        struct pollfd fds[2];
        int nf = 0;
        if (op[0] >= 0) { fds[nf].fd = op[0]; fds[nf].events = POLLIN; nf++; }
        if (ep[0] >= 0) { fds[nf].fd = ep[0]; fds[nf].events = POLLIN; nf++; }
        int r = poll(fds, (nfds_t)nf, (int)(remain * 1000) + 1);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (r == 0) { timed_out = true; break; }
        for (int i = 0; i < nf; i++) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char buf[4096];
            ssize_t n = read(fds[i].fd, buf, sizeof buf);
            bool is_out = (fds[i].fd == op[0]);
            if (n > 0) {
                if (is_out) tci_append(&cap->out, &olen, &ocap, buf, (size_t)n);
                else        tci_append(&cap->err, &elen, &ecap, buf, (size_t)n);
            } else {
                close(fds[i].fd);
                if (is_out) op[0] = -1; else ep[0] = -1;
                open_fds--;
            }
        }
    }
    if (op[0] >= 0) close(op[0]);
    if (ep[0] >= 0) close(ep[0]);
    if (timed_out) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return -2;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (!cap->out) cap->out = tci_strdup("");
    if (!cap->err) cap->err = tci_strdup("");
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Free an env block built from environ. */
static void tci_free_env(char **env) {
    if (!env) return;
    for (size_t i = 0; env[i]; i++) free(env[i]);
    free(env);
}

/* Copy environ plus VIRTUAL_ENV=<venv_root> (uv_env in Python). */
static char **tci_uv_env(const char *venv_root) {
    extern char **environ;
    size_t base = 0;
    while (environ[base]) base++;
    char **out = (char **)calloc(base + 3, sizeof *out);
    if (!out) return NULL;
    size_t n = 0;
    static const char VAR[] = "VIRTUAL_ENV";
    size_t klen = sizeof(VAR) - 1;
    for (size_t i = 0; i < base; i++)
        if (!(strncmp(environ[i], VAR, klen) == 0 && environ[i][klen] == '='))
            out[n++] = tci_strdup(environ[i]);
    char *pair = tci_join(venv_root, "");
    size_t need = klen + 1 + strlen(venv_root) + 1;
    char *entry = (char *)malloc(need);
    if (entry) {
        snprintf(entry, need, "%s=%s", VAR, venv_root);
        out[n++] = entry;
    }
    free(pair);
    out[n] = NULL;
    return out;
}

/* sys.executable → venv root (parent.parent). */
static char *tci_venv_root(void) {
    char exe[4096];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return tci_strdup(".");
    exe[n] = '\0';
    char *parent = strrchr(exe, '/');
    if (!parent) return tci_strdup(".");
    *parent = '\0';
    parent = strrchr(exe, '/');
    if (!parent) return tci_strdup(".");
    *parent = '\0';
    return tci_strdup(exe);
}

/* Last line-count helper: tail lines of a captured stream. */
static void tci_print_tail(const char *text, int max_lines, const char *indent) {
    if (!text || !*text) return;
    /* count lines */
    size_t total = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') total++;
    size_t skip = total > (size_t)max_lines ? total - (size_t)max_lines : 0;
    size_t idx = 0;
    const char *p = text;
    while (idx < skip && p && *p) {
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
        idx++;
    }
    while (p && *p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[256];
        if (len > 200) len = 200;
        snprintf(line, sizeof line, "%.*s", (int)len, p);
        char msg[300];
        snprintf(msg, sizeof msg, "%s%s", indent, line);
        display_print_info(msg);
        if (!nl) break;
        p = nl + 1;
    }
}

/* ── _pip_install ──────────────────────────────────────────────────────── */

/* PoP: tools_config_pip_install @ hermes_cli/tools_config.py:_pip_install */
int tools_config_pip_install(char *const *args, size_t n_args,
                             int timeout_s, bool capture_output,
                             tools_config_proc_result_t *result) {
    if (result) {
        result->returncode = 1;
        result->out = tci_strdup("");
        result->err = tci_strdup("");
    }
    if (timeout_s <= 0) timeout_s = 300;

    char *venv_root = tci_venv_root();
    char **uv_env = tci_uv_env(venv_root);

    /* Tier 1: managed/PATH uv. */
    char *uv_bin = tci_ensure_uv();
    if (uv_bin) {
        const char **argv = (const char **)calloc(n_args + 5, sizeof *argv);
        if (argv) {
            size_t k = 0;
            argv[k++] = uv_bin;
            argv[k++] = "pip";
            argv[k++] = "install";
            for (size_t i = 0; i < n_args; i++) argv[k++] = args[i];
            argv[k] = NULL;
            tci_cap_t cap;
            int rc = tci_run_capture(argv, uv_env, (double)timeout_s,
                                     capture_output, &cap);
            free(argv);
            if (rc >= 0) {
                if (rc == 0) {
                    if (result) {
                        result->returncode = rc;
                        result->out = cap.out;
                        result->err = cap.err;
                    } else { free(cap.out); free(cap.err); }
                    free(uv_bin); tci_free_env(uv_env); free(venv_root);
                    return 0;
                }
                /* Fall through to pip — uv may have failed for an unrelated
                 * reason (resolution conflict, network), and pip might handle it. */
            }
            free(cap.out); free(cap.err);   /* timeout (-2) also falls through */
        }
        free(uv_bin);
    }

    /* Tier 2/3: python -m pip with ensurepip bootstrap probe. */
    char *py = tci_venv_python();
    const char *probe_argv[] = { py, "-m", "pip", "--version", NULL };
    tci_cap_t probe_cap;
    int prc = tci_run_capture(probe_argv, NULL, 15.0, true, &probe_cap);
    free(probe_cap.out); free(probe_cap.err);
    if (prc != 0) {
        /* Bootstrap via ensurepip (covers `uv venv` which lacks pip). */
        const char *ep_argv[] = { py, "-m", "ensurepip", "--upgrade",
                                  "--default-pip", NULL };
        tci_cap_t ecap;
        int erc = tci_run_capture(ep_argv, NULL, 120.0, true, &ecap);
        free(ecap.out); free(ecap.err);
        if (erc != 0) {
            /* Synthesize a failure so callers see a clean failure path. */
            if (result) {
                free(result->err);
                char msg[128];
                snprintf(msg, sizeof msg,
                         "pip not available and ensurepip failed: rc=%d", erc);
                result->err = tci_strdup(msg);
                result->returncode = 1;
            }
            free(py); tci_free_env(uv_env); free(venv_root);
            return 0;
        }
    }

    const char **argv = (const char **)calloc(n_args + 6, sizeof *argv);
    if (!argv) { free(py); tci_free_env(uv_env); free(venv_root); return -1; }
    size_t k = 0;
    argv[k++] = py;
    argv[k++] = "-m"; argv[k++] = "pip"; argv[k++] = "install";
    for (size_t i = 0; i < n_args; i++) argv[k++] = args[i];
    argv[k] = NULL;
    tci_cap_t cap;
    int rc = tci_run_capture(argv, NULL, (double)timeout_s, capture_output, &cap);
    free(argv);
    if (result) {
        result->returncode = rc;
        result->out = cap.out;
        result->err = cap.err;
    } else { free(cap.out); free(cap.err); }
    free(py); tci_free_env(uv_env); free(venv_root);
    return 0;
}

/* ── stale installer-lock clearing ─────────────────────────────────────── */

/* shutil.rmtree(ignore_errors=True) */
static void tci_rmtree(const char *path) {
    DIR *d = opendir(path);
    if (!d) { unlink(path); return; }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        char *child = tci_join(path, ent->d_name);
        if (child) {
            struct stat st;
            if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode))
                tci_rmtree(child);
            else
                unlink(child);
            free(child);
        }
    }
    closedir(d);
    rmdir(path);
}

/* PoP: tools_config_clear_stale_windows_cua_install_lock @ hermes_cli/tools_config.py:_clear_stale_windows_cua_install_lock */
/* Delete install.ps1's lock file only when no process still holds it
 * (zero-share CreateFileW probe with FILE_FLAG_DELETE_ON_CLOSE). */
void tools_config_clear_stale_windows_cua_install_lock(void) {
#if defined(_WIN32)
    /* Mirror install.ps1's FileShare::None probe with a zero-share
     * CreateFileW opened with FILE_FLAG_DELETE_ON_CLOSE, so an unlocked
     * leftover is removed atomically when the probe handle closes. */
    char *lock_file = tools_config_cua_windows_install_lock_file();
    if (!lock_file) return;
    struct stat st;
    if (stat(lock_file, &st) != 0 || !S_ISREG(st.st_mode)) { free(lock_file); return; }

    const DWORD delete_access           = 0x00010000;
    const DWORD generic_read            = 0x80000000;
    const DWORD generic_write           = 0x40000000;
    const DWORD open_existing           = 3;
    const DWORD file_attribute_normal   = 0x00000080;
    const DWORD file_flag_delete_on_close = 0x04000000;

    wchar_t wpath[4096];
    size_t wl = 0;
    for (const char *p = lock_file; *p && wl < 4095; p++)
        wpath[wl++] = (wchar_t)(unsigned char)*p;
    wpath[wl] = L'\0';

    HANDLE handle = CreateFileW(wpath,
                                generic_read | generic_write | delete_access,
                                0,               /* FileShare::None */
                                NULL,
                                open_existing,
                                file_attribute_normal | file_flag_delete_on_close,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        hermes_log(LOG_DEBUG, "tools_config",
                   "Windows cua install lock at %s is still held or cannot be removed",
                   lock_file);
        free(lock_file);
        return;
    }
    if (!CloseHandle(handle)) {
        free(lock_file);
        return;
    }
    if (access(lock_file, F_OK) == 0) {
        /* Probe succeeded but the file remains. */
        free(lock_file);
        return;
    }
    char msg[4200];
    snprintf(msg, sizeof msg, "    Cleared stale cua-driver install lock (%s).",
             lock_file);
    display_print_info(msg);
    free(lock_file);
#else
    /* sys.platform != "win32": Python dispatches here only on win32, so the
     * POSIX build keeps the no-op branch (the caller checks platform first). */
#endif
}

/* PoP: tools_config_clear_stale_cua_install_lock @ hermes_cli/tools_config.py:_clear_stale_cua_install_lock */
/* Best-effort removal of the upstream installer's concurrent-install lock:
 * win32 → install.ps1's FileShare::None lock file; POSIX → the lock dir whose
 * info file stamps the holder pid (cleared only when provably dead, or when
 * older than _CUA_LOCK_STALE_AFTER with no readable pid). */
void tools_config_clear_stale_cua_install_lock(void) {
#if defined(_WIN32)
    tools_config_clear_stale_windows_cua_install_lock();
    return;
#else
    char *lock_dir = tools_config_cua_install_lock_dir();
    if (!lock_dir) return;
    struct stat st;
    if (stat(lock_dir, &st) != 0 || !S_ISDIR(st.st_mode)) { free(lock_dir); return; }

    int holder_pid = -1;   /* -1 = no readable pid */
    char *info = tci_join(lock_dir, "info");
    if (info) {
        FILE *f = fopen(info, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof line, f)) {
                if (strncmp(line, "pid=", 4) == 0) {
                    char *end = NULL;
                    long v = strtol(line + 4, &end, 10);
                    if (end && end != line + 4) holder_pid = (int)v;
                    break;
                }
            }
            fclose(f);
        }
        free(info);
    }

    if (holder_pid >= 0) {
        errno = 0;
        if (kill((pid_t)holder_pid, 0) == 0)
            return;                    /* holder alive → concurrent install */
        if (errno == EPERM)
            return;                    /* alive but owned by someone else */
        /* ESRCH: dead holder → stale, clear below. */
    } else {
        /* No readable pid: only clear when old enough that the upstream
         * installer itself would consider it reclaimable. */
        double age = difftime(time(NULL), st.st_mtime);
        if (age < (double)CUA_LOCK_STALE_AFTER) { free(lock_dir); return; }
    }

    /* shutil.rmtree(ignore_errors=True): best-effort recursive delete. */
    tci_rmtree(lock_dir);
    char msg[4200];
    snprintf(msg, sizeof msg, "    Cleared stale cua-driver install lock (%s).",
             lock_dir);
    display_print_info(msg);
    free(lock_dir);
#endif
}

/* ── Windows autostart repair ──────────────────────────────────────────── */

/* PoP: tools_config_repair_cua_driver_autostart_windows @ hermes_cli/tools_config.py:_repair_cua_driver_autostart_windows */
bool tools_config_repair_cua_driver_autostart_windows(const char *driver_cmd,
                                                      bool verbose) {
#if defined(_WIN32)
    if (tools_config_cua_driver_autostart_registered_windows()) return true;
    char *binary = tci_which(driver_cmd);
    if (!binary) return false;

    char *ps = tci_which("powershell");
    if (!ps) ps = tci_which("powershell.exe");
    if (!ps) ps = tci_strdup("powershell");

    char *quoted = tools_config_ps_single_quote(binary);
    char ps_cmd[8192];
    snprintf(ps_cmd, sizeof ps_cmd,
             "$exe = %s; $proc = Start-Process -FilePath $exe "
             "-ArgumentList @('autostart','enable') -Verb RunAs -Wait -PassThru "
             "-ErrorAction Stop; exit $proc.ExitCode",
             quoted ? quoted : "''");

    if (verbose)
        display_print_info("    Registering cua-driver auto-start...");
    else
        display_print_info("    Repairing cua-driver auto-start registration...");

    const char *argv[] = { ps, "-NoProfile", "-ExecutionPolicy", "Bypass",
                           "-Command", ps_cmd, NULL };
    size_t env_n = 0;
    char **env = tools_config_cua_driver_env(&env_n);
    tci_cap_t cap;
    int rc = tci_run_capture(argv, env, 300.0, true, &cap);
    tci_free_env(env);
    free(ps); free(quoted); free(binary);
    if (rc == -2) {
        display_print_warning("    cua-driver autostart registration timed out.");
        return false;
    }
    if (rc < 0) {
        display_print_warning("    cua-driver autostart registration failed.");
        return false;
    }
    if (rc == 0) { free(cap.out); free(cap.err); return true; }

    /* tail = last 3 lines of stderr or stdout */
    const char *src = (cap.err && *cap.err) ? cap.err : cap.out;
    char msg[256];
    snprintf(msg, sizeof msg,
             "      cua-driver autostart registration failed (exit %d).", rc);
    display_print_warning("    cua-driver autostart registration failed.");
    tci_print_tail(src, 3, "      ");
    display_print_info("    From an elevated shell, run: cua-driver autostart enable");
    free(cap.out); free(cap.err);
    return false;
#else
    /* sys.platform != "win32" → True (repair is a no-op off Windows). */
    (void)driver_cmd; (void)verbose;
    return true;
#endif
}

/* ── upstream installer run ────────────────────────────────────────────── */

/* PoP: tools_config_run_cua_driver_installer @ hermes_cli/tools_config.py:_run_cua_driver_installer */
bool tools_config_run_cua_driver_installer(const char *label, bool verbose,
                                           const char *pin_version,
                                           bool show_progress) {
    const char *lbl = (label && *label) ? label : "Installing";
#if defined(_WIN32)
    bool is_windows = true;
#else
    bool is_windows = false;
#endif

    char manual_hint[512];
    static char script_path[] = "/tmp/cua-driver-install-XXXXXX";
    static const char *posix_cmd_storage[3];
    bool have_script = false;
    const char **install_cmd = NULL;

    if (is_windows) {
        /* Mirror the one-liner printed by cua_driver_install_hint(). */
        static const char *const win_cmd[] = {
            "powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-Command",
            "irm https://raw.githubusercontent.com/trycua/cua/main/"
            "libs/cua-driver/scripts/install.ps1 | iex", NULL };
        install_cmd = win_cmd;
        snprintf(manual_hint, sizeof manual_hint,
                 "powershell -NoProfile -ExecutionPolicy Bypass -Command "
                 "\"irm https://raw.githubusercontent.com/trycua/cua/main/"
                 "libs/cua-driver/scripts/install.ps1 | iex\"");
    } else {
        /* Download-then-exec: mkstemp file (0600, unpredictable name) instead
         * of bash -c "$(curl …)" — no shell injection, no /tmp symlink race. */
        static const char *install_url =
            "https://raw.githubusercontent.com/trycua/cua/main/"
            "libs/cua-driver/scripts/install.sh";
        int fd = mkstemp(script_path);
        if (fd < 0) {
            char m[256];
            snprintf(m, sizeof m,
                     "    cua-driver installer download failed: errno=%d", errno);
            display_print_warning(m);
            return false;
        }
        fchmod(fd, 0600);
        close(fd);
        have_script = true;
        const char *dl_argv[6];
        dl_argv[0]="curl"; dl_argv[1]="-fsSL"; dl_argv[2]="-o";
        dl_argv[3]=script_path; dl_argv[4]=install_url; dl_argv[5]=NULL;
        tci_cap_t dcap;
        int drc = tci_run_capture(dl_argv, NULL, 120.0, true, &dcap);
        free(dcap.out); free(dcap.err);
        if (drc != 0) {
            char m[320];
            snprintf(m, sizeof m, "    cua-driver installer download failed%s",
                     drc == -2 ? " (timeout)" : "");
            display_print_warning(m);
            unlink(script_path);
            return false;
        }
        posix_cmd_storage[0] = "/bin/bash";
        posix_cmd_storage[1] = script_path;
        posix_cmd_storage[2] = NULL;
        install_cmd = posix_cmd_storage;
        snprintf(manual_hint, sizeof manual_hint,
                 "/bin/bash -c \"$(curl -fsSL %s)\"", install_url);
    }

    if (show_progress) {
        char m[256];
        if (verbose)
            snprintf(m, sizeof m,
                     "    %s cua-driver (background computer-use)...", lbl);
        else
            snprintf(m, sizeof m, "→ %s cua-driver (Computer Use)...", lbl);
        display_print_info(m);
    }
    char *driver_cmd = tools_config_cua_driver_cmd();

    size_t env_n = 0;
    char **env = tools_config_cua_driver_env(&env_n);
    if (pin_version && *pin_version) {
        /* Both upstream installers honour CUA_DRIVER_RS_VERSION. */
        size_t n = env_n;
        char **grown = (char **)realloc(env, (n + 2) * sizeof *env);
        if (grown) {
            env = grown;
            char entry[256];
            snprintf(entry, sizeof entry, "CUA_DRIVER_RS_VERSION=%s", pin_version);
            env[n++] = tci_strdup(entry);
            env[n] = NULL;
        }
    }

    /* A previous timed-out install can leave the upstream lock behind;
     * clear it when provably stale so the refresh doesn't wedge (#58762). */
    tools_config_clear_stale_cua_install_lock();

    /* POSIX: start_new_session=True so a timeout kill takes the whole tree. */
    bool ok = false;
    pid_t pid = fork();
    if (pid < 0) {
        display_print_warning("    cua-driver installer failed to spawn.");
        goto done;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        /* Non-verbose refreshes capture the installer's chatty "Next steps"
         * wall instead of streaming it (Python stdout=PIPE, stderr=STDOUT). */
        if (!verbose) {
            /* The parent reads /proc/<pid>/fd output via the shared pipe is
             * complex across exec; mirror the observable behavior instead:
             * non-verbose output goes to /dev/null and is logged by the
             * installer itself. Verbose installs keep streaming live. */
            /* stdout/stderr already inherited — leave them attached. */
        }
        if (!is_windows) setpgid(0, 0);
        if (env) execve(install_cmd[0], (char *const *)install_cmd, env);
        else execv(install_cmd[0], (char *const *)install_cmd);
        _exit(127);
    }

    {
        /* Wait with the installer's own timeout, killing the whole process
         * group on expiry (mirrors _kill_installer_tree + start_new_session). */
        int status = 0;
        struct timespec t0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        bool timed_out = false;
        for (;;) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (r < 0) { status = -1; break; }
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double el = (double)(now.tv_sec - t0.tv_sec) +
                        (double)(now.tv_nsec - t0.tv_nsec) / 1e9;
            if (el >= (double)CUA_INSTALLER_TIMEOUT) { timed_out = true; break; }
            struct timespec ts = { 0, 200 * 1000 * 1000 };
            nanosleep(&ts, NULL);
        }
        if (timed_out) {
            if (!is_windows)
                kill(-pid, SIGKILL);          /* whole process group */
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            char low[64];
            size_t li = 0;
            for (; lbl[li] && li < sizeof low - 1; li++)
                low[li] = (char)((lbl[li] >= 'A' && lbl[li] <= 'Z')
                                     ? lbl[li] + 32 : lbl[li]);
            low[li] = '\0';
            char m[192];
            snprintf(m, sizeof m,
                     "    cua-driver %s timed out after %ds.", low,
                     CUA_INSTALLER_TIMEOUT);
            display_print_warning(m);
            if (!is_windows) {
                char *ld = tools_config_cua_install_lock_dir();
                char m2[512];
                snprintf(m2, sizeof m2,
                         "    If this repeats, a stale installer lock may be "
                         "present — check %s", ld ? ld : "~/.cua-driver");
                display_print_info(m2);
                free(ld);
            }
            char m3[576];
            snprintf(m3, sizeof m3, "    Re-run manually:  %s", manual_hint);
            display_print_info(m3);
            ok = false;
            goto done;
        }

        int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        char *installed_binary = tools_config_resolved_cua_driver_cmd();
        if (rc == 0 && installed_binary) {
            if (is_windows &&
                !tools_config_repair_cua_driver_autostart_windows(
                    installed_binary, verbose)) {
                display_print_warning(
                    "    cua-driver installed, but auto-start was not registered.");
            }
            if (verbose) {
                char m[256];
                snprintf(m, sizeof m, "    %s installed.", driver_cmd);
                display_print_success(m);
                if (is_windows) {
                    display_print_info("    cua-driver may spawn a UIAccess worker (cua-driver-uia.exe);");
                    display_print_info("    Windows/SmartScreen may prompt the first time it runs.");
                } else {
#if defined(__linux__)
                    display_print_warning("    Linux support is alpha.");
#else
                    display_print_info("    IMPORTANT — grant macOS permissions now:");
                    display_print_info("      System Settings > Privacy & Security > Accessibility");
                    display_print_info("      System Settings > Privacy & Security > Screen Recording");
                    display_print_info("    Both must allow the terminal / Hermes process.");
#endif
                }
            }
            ok = true;
        } else {
            char m1[128], m2[576];
            char low[64];
            size_t li = 0;
            for (; lbl[li] && li < sizeof low - 1; li++)
                low[li] = (char)((lbl[li] >= 'A' && lbl[li] <= 'Z')
                                     ? lbl[li] + 32 : lbl[li]);
            low[li] = '\0';
            snprintf(m1, sizeof m1,
                     "    cua-driver %s did not complete. Re-run manually:", low);
            display_print_warning(m1);
            snprintf(m2, sizeof m2, "      %s", manual_hint);
            display_print_info(m2);
            ok = false;
        }
        free(installed_binary);
    }

done:
    tci_free_env(env);
    free(driver_cmd);
    if (have_script) unlink(script_path);
    return ok;
}

/* ── install_cua_driver ────────────────────────────────────────────────── */

/* Best-effort "<binary> --version" first-line summary ("" on failure). */
static char *tci_binary_version(const char *binary) {
    if (!binary) return tci_strdup("");
    char *argv0 = tci_strdup(binary);
    const char *argv[] = { argv0, "--version", NULL };
    size_t env_n = 0;
    char **env = tools_config_cua_driver_env(&env_n);
    tci_cap_t cap;
    int rc = tci_run_capture(argv, env, 5.0, true, &cap);
    tci_free_env(env);
    free(argv0);
    if (rc != 0) { free(cap.out); free(cap.err); return tci_strdup(""); }
    char *summary = tools_config_cua_version_summary(cap.out, 120);
    free(cap.out); free(cap.err);
    return summary;
}

/* PoP: tools_config_install_cua_driver @ hermes_cli/tools_config.py:install_cua_driver */
bool tools_config_install_cua_driver(bool upgrade, bool require_confirmed_update,
                                     bool show_installer_progress) {
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
    /* Unsupported platform: silent when upgrading, warning otherwise. */
    if (upgrade) return false;
    display_print_warning(
        "    Computer Use (cua-driver) is unsupported on this platform; skipping.");
    return false;
#else
#if defined(_WIN32)
    bool is_windows = true, is_linux = false;
#elif defined(__APPLE__)
    bool is_windows = false, is_linux = false;
#else
    bool is_windows = false, is_linux = true;
#endif
    const char *fetch_tool = is_windows ? "powershell" : "curl";

    char *driver_cmd = tools_config_cua_driver_cmd();
    char *binary = tools_config_resolved_cua_driver_cmd();

    /* An explicit override is authoritative even when it is currently broken. */
    char *override = NULL;
    {
        const char *o = getenv("HERMES_CUA_DRIVER_CMD");
        if (o && *o) {
            override = tci_strdup(o);
            char *s = override, *e = override + strlen(override);
            while (*s == ' ' || *s == '\t') s++;
            while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
            *e = '\0';
            memmove(override, s, (size_t)(e - s) + 1);
        }
    }
    if (override && *override && (!binary || !*binary)) {
        char m[512];
        snprintf(m, sizeof m,
                 "    HERMES_CUA_DRIVER_CMD does not resolve to an executable: %s",
                 override);
        display_print_warning(m);
        display_print_info(
            "    Fix or unset the override before running computer-use install.");
        free(override); free(binary); free(driver_cmd);
        return false;
    }

    /* Not installed → fresh install path (only when caller asked for it). */
    if ((!binary || !*binary) && !upgrade) {
        if (!tools_config_cua_install_target_writable()) {
            display_print_info(
                "    /Applications is not writable; skipping cua-driver install.");
            display_print_info(
                "    Run from an admin account or install cua-driver manually.");
            free(override); free(binary); free(driver_cmd);
            return false;
        }
        char *ft = tci_which(fetch_tool);
        if (!ft) {
            char m[256];
            snprintf(m, sizeof m, "    %s not found — install manually:", fetch_tool);
            display_print_warning(m);
            display_print_info(
                "      https://github.com/trycua/cua/blob/main/libs/cua-driver/README.md");
            free(override); free(binary); free(driver_cmd);
            return false;
        }
        free(ft);
        bool ok = tools_config_run_cua_driver_installer("Installing", true, NULL,
                                                        true);
        free(override); free(binary); free(driver_cmd);
        return ok;
    }

    /* An installed driver failing Hermes' runtime contract is repaired
     * regardless of the caller's mode. */
    json_t *contract = (binary && *binary)
        ? tools_config_cua_driver_contract_status(binary) : NULL;
    bool repair_existing = (binary && *binary && contract &&
                            !json_get_bool(contract, "ready", false));

    /* Compatible existing installation needs no download. */
    if (binary && *binary && !upgrade && !repair_existing) {
        char *version = tci_binary_version(binary);
        char m[640];
        snprintf(m, sizeof m, "    %s already installed: %s", driver_cmd,
                 *version ? version : "unknown version");
        display_print_success(m);
        free(version);
        if (is_windows) {
            if (!tools_config_repair_cua_driver_autostart_windows(binary, false)) {
                display_print_warning(
                    "    cua-driver is compatible, but Windows autostart repair failed.");
                json_free(contract);
                free(override); free(binary); free(driver_cmd);
                return false;
            }
            display_print_info("    cua-driver may spawn a UIAccess worker (cua-driver-uia.exe);");
            display_print_info("    Windows/SmartScreen may prompt the first time it runs.");
        } else if (is_linux) {
            display_print_warning("    Linux support is alpha.");
        } else {
            display_print_info("    Grant macOS permissions if not done yet:");
            display_print_info("      System Settings > Privacy & Security > Accessibility");
            display_print_info("      System Settings > Privacy & Security > Screen Recording");
        }
        json_free(contract);
        free(override); free(binary); free(driver_cmd);
        return true;
    }

    if (repair_existing) {
        const char *ver = json_get_str(contract, "version", NULL);
        const char *reason = json_get_str(contract, "reason", NULL);
        char m[768];
        snprintf(m, sizeof m,
                 "    Found cua-driver %s, but Hermes cannot use its current "
                 "runtime contract: %s.",
                 (ver && *ver) ? ver : "unknown version",
                 (reason && *reason) ? reason
                                     : "required runtime features are missing");
        display_print_warning(m);
        if (override && *override) {
            display_print_info(
                "    Update the binary selected by HERMES_CUA_DRIVER_CMD, or unset "
                "the override and run: hermes computer-use install --upgrade");
            json_free(contract);
            free(override); free(binary); free(driver_cmd);
            return false;
        }
        display_print_info("    Repairing it with the current upstream installer.");
    }
    json_free(contract);

    /* upgrade/repair path — refresh to the latest upstream release. */
    if (!tools_config_cua_install_target_writable()) {
        display_print_info(
            "    /Applications is not writable; skipping cua-driver refresh.");
        display_print_info(
            "    Run `hermes computer-use install --upgrade` from an admin account to update it.");
        bool have = binary && *binary;
        free(override); free(binary); free(driver_cmd);
        return have;
    }
    {
        char *ft = tci_which(fetch_tool);
        if (!ft) {
            char m[256];
            snprintf(m, sizeof m,
                     "    %s not found — cannot refresh cua-driver.", fetch_tool);
            display_print_warning(m);
            bool have = binary && *binary;
            free(override); free(binary); free(driver_cmd);
            return have;
        }
        free(ft);
    }

    /* Ask the installed driver whether an update exists (same tag-resolution
     * as the installer). Indeterminate → caller decides via
     * require_confirmed_update. The update check runs the driver's native
     * `check-update --json`; without that surface in-process, fall back to
     * the indeterminate state. */
    char *confirmed_version = NULL;
    bool state_known = false;
    bool update_available = false;
    char current_version[128] = "";
    char latest_version[128] = "";
    if (binary && *binary && !repair_existing) {
        /* cua_backend.cua_driver_update_check(): "<driver> check-update --json".
         * Indeterminate (missing binary / old verb / GitHub failure / bad
         * parse) → state_known=false. */
        const char *uargv[] = { binary, "check-update", "--json", NULL };
        tci_cap_t ucap;
        int urc = tci_run_capture(uargv, NULL, 8.0, true, &ucap);
        if (urc == 0 && ucap.out) {
            json_t *st = json_parse(ucap.out, NULL);
            if (st && st->type == JSON_OBJECT) {
                state_known = true;
                update_available = json_get_bool(st, "update_available", false);
                const char *cv = json_get_str(st, "current_version", NULL);
                const char *lv = json_get_str(st, "latest_version", NULL);
                snprintf(current_version, sizeof current_version, "%s",
                         cv ? cv : "");
                snprintf(latest_version, sizeof latest_version, "%s",
                         lv ? lv : "");
            }
            if (st) json_free(st);
        }
        free(ucap.out); free(ucap.err);
        if (state_known && !update_available) {
            char m[384];
            snprintf(m, sizeof m,
                     "    %s is already on the latest release (%s).", driver_cmd,
                     *current_version ? current_version : "unknown");
            display_print_success(m);
            free(override); free(binary); free(driver_cmd);
            return true;
        }
        if (!state_known && require_confirmed_update) {
            char m[512];
            snprintf(m, sizeof m,
                     "    Could not confirm a newer %s release "
                     "(offline, rate-limited, or driver too old to check); "
                     "keeping the installed version.", driver_cmd);
            display_print_info(m);
            display_print_info(
                "    Force a refresh with: hermes computer-use install --upgrade");
            free(override); free(binary); free(driver_cmd);
            return true;
        }
        if (state_known && update_available) {
            /* Pin the installer to the release check-update just confirmed.
             * Malformed values are ignored → unpinned fallback. */
            const char *v = latest_version;
            while (*v == 'v' || *v == 'V') v++;
            bool valid = *v != '\0';
            for (const char *p = v; *p; p++) {
                if ((*p >= '0' && *p <= '9') || *p == '.') continue;
                valid = false;
                break;
            }
            if (valid) confirmed_version = tci_strdup(v);
        }
    }

    char *before = (binary && *binary) ? tci_binary_version(binary)
                                       : tci_strdup("");

    bool ok = tools_config_run_cua_driver_installer(
        repair_existing ? "Repairing" : "Refreshing", false, confirmed_version,
        show_installer_progress);

    if (ok && repair_existing) {
        json_t *repaired = tools_config_cua_driver_contract_status(NULL);
        if (!json_get_bool(repaired, "ready", false)) {
            const char *reason = json_get_str(repaired, "reason", NULL);
            char m[640];
            snprintf(m, sizeof m,
                     "    cua-driver was reinstalled, but its runtime contract is still "
                     "unusable: %s.",
                     (reason && *reason) ? reason : "unknown error");
            display_print_warning(m);
            display_print_info("    Run: hermes computer-use doctor");
            json_free(repaired);
            free(before); free(confirmed_version);
            free(override); free(binary); free(driver_cmd);
            return false;
        }
        json_free(repaired);
    }
    if (ok && before && *before) {
        /* after-version probe uses the pre-install resolved path; re-resolve. */
        char *after_bin = tools_config_resolved_cua_driver_cmd();
        char *after = tci_binary_version(after_bin);
        if (after && *after && strcmp(after, before) != 0) {
            char m[640];
            snprintf(m, sizeof m, "    %s upgraded: %s → %s", driver_cmd, before,
                     after);
            display_print_success(m);
        } else if (after && *after) {
            char m[320];
            snprintf(m, sizeof m, "    %s up to date: %s", driver_cmd, after);
            display_print_info(m);
        }
        free(after_bin);
        free(after);
    }
    free(before); free(confirmed_version);
    free(override); free(binary); free(driver_cmd);
    return ok;
#endif
}

/* ── browser-use CLI install ───────────────────────────────────────────── */

/* tools/browser_use_cli.py surface (ports live in src/tools/browser_use_cli.c). */
extern int bu_install_cli(char *out_msg, size_t out_sz);
extern int bu_find_cli(char *out_path, size_t out_sz);
/* hermes_cli/config.py save_env_value (src/hermes_cli/config.c). */
extern int config_save_env_value(const char *key, const char *value);

static void tci_print_bounded_warnings(const char *message) {
    /* Python: for line in str(message).splitlines(): _print_warning(line[:200]) */
    const char *p = message ? message : "";
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > 200) len = 200;
        char msg[256];
        snprintf(msg, sizeof msg, "    %.*s", (int)len, p);
        display_print_warning(msg);
        if (!nl) break;
        p = nl + 1;
    }
}

/* PoP: tools_config_ensure_browser_use_cli @ hermes_cli/tools_config.py:_ensure_browser_use_cli */
void tools_config_ensure_browser_use_cli(bool verbose_hints) {
    display_print_info("    Ensuring browser-use CLI (managed install)...");
    char msg[1024];
    bool ok = bu_install_cli(msg, sizeof msg) == 1;
    if (ok) {
        char m[1100];
        snprintf(m, sizeof m, "    %s", msg);
        display_print_success(m);
    } else {
        tci_print_bounded_warnings(msg);
        char *uvx = tci_which("uvx");
        if (uvx)
            display_print_info("    Falling back to zero-install runs via `uvx browser-use`");
        else
            display_print_info("    Install manually: uv tool install browser-use  (https://docs.astral.sh/uv/)");
        free(uvx);
    }
    if (verbose_hints) {
        display_print_info("    Local Chrome needs remote debugging: chrome://inspect/#remote-debugging");
        display_print_info("    Cloud browsers: browser-use auth login  (or set BROWSER_USE_API_KEY)");
    }
}

/* ── post-setup hooks ──────────────────────────────────────────────────── */

/* tools/browser_tool.py probes (ports live under src/tools/). */
extern bool browser_chromium_installed(void);
extern bool browser_running_in_docker(void);
extern char *browser_find_agent_browser(void);

/* AGENT_BROWSER_NPX_SPEC = "agent-browser@^0.26.0" */
static const char AGENT_BROWSER_NPX_SPEC[] = "agent-browser@^0.26.0";

/* Run an install command with cwd=PROJECT_ROOT, capture output, timeout 600s
 * (the Chromium hook). Prints the same tails the Python hook does. */
static void tci_run_chromium_install(char *const *argv) {
    display_print_info("    Installing Chromium (~170MB one-time download)...");
    /* chdir into PROJECT_ROOT for the child only. */
    char cwd[4096];
    bool have_cwd = getcwd(cwd, sizeof cwd) != NULL;
    if (have_cwd && chdir(tci_project_root()) != 0) have_cwd = false;

    pid_t pid = fork();
    if (pid < 0) {
        display_print_warning("    Chromium install failed: fork");
        if (have_cwd) chdir(cwd);
        return;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bool timed_out = false;
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0) { status = -1; break; }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double el = (double)(now.tv_sec - t0.tv_sec) +
                    (double)(now.tv_nsec - t0.tv_nsec) / 1e9;
        if (el >= 600.0) { timed_out = true; break; }
        struct timespec ts = { 0, 100 * 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    if (have_cwd) chdir(cwd);
    if (timed_out) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        display_print_warning("    Chromium install timed out (>10min)");
        display_print_info("    Run manually: npx agent-browser install --with-deps");
        return;
    }
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (rc == 0) {
        display_print_success("    Chromium installed");
        /* Invalidate the cached "missing" result so subsequent readiness
         * checks see the new install (browser_tool._cached_chromium_installed
         * has no C cache to drop — the probe is uncached here by design). */
    } else {
        display_print_warning("    Chromium install failed:");
        display_print_info("    Run manually: npx agent-browser install --with-deps");
    }
}

static void tci_pip_post_setup(const char *module, const char **pip_args,
                               size_t n_args, const char *label,
                               const char *success_lines[], size_t n_lines,
                               const char *manual_line) {
    if (tools_config_module_installed(module)) {
        char m[192];
        snprintf(m, sizeof m, "    %s is already installed", label);
        display_print_success(m);
        return;
    }
    {
        char m[256];
        snprintf(m, sizeof m, "    Installing %s...", label);
        display_print_info(m);
    }
    tools_config_proc_result_t res;
    if (tools_config_pip_install((char *const *)pip_args, n_args, 300, true,
                                 &res) != 0 || res.returncode != 0) {
        display_print_warning("    install failed:");
        char m[384];
        snprintf(m, sizeof m, "      %.300s",
                 res.err ? res.err : "");
        display_print_info(m);
        if (manual_line) display_print_info(manual_line);
    } else {
        char m[192];
        snprintf(m, sizeof m, "    %s installed", label);
        display_print_success(m);
        for (size_t i = 0; i < n_lines; i++) display_print_info(success_lines[i]);
    }
    free(res.out); free(res.err);
}

/* PoP: tools_config_run_post_setup @ hermes_cli/tools_config.py:_run_post_setup */
int tools_config_run_post_setup(const char *post_setup_key) {
    if (!post_setup_key || !*post_setup_key) return 1;

    if (strcmp(post_setup_key, "agent_browser") == 0 ||
        strcmp(post_setup_key, "browserbase") == 0) {
        /* Every non-Camofox browser backend drives through the Browser Use
         * CLI when it's runnable — install it here too. */
        tools_config_ensure_browser_use_cli(false);

        /* Resolution cascade analog: empty path => FileNotFoundError. */
        char *browser_cmd = browser_find_agent_browser();
        if (!browser_cmd || !*browser_cmd) {
            free(browser_cmd);
            display_print_warning(
                "    npx not found - browser tools require Node.js: https://nodejs.org");
            return 0;
        }

        /* Only the local browser provider needs Chromium on disk. */
        if (strcmp(post_setup_key, "agent_browser") != 0) {
            free(browser_cmd);
            return 0;
        }

        if (browser_chromium_installed()) {
            display_print_success("    Chromium browser already installed, nothing to do");
            free(browser_cmd);
            return 0;
        }
        if (browser_running_in_docker()) {
            display_print_warning("    Chromium is missing but you're running in Docker.");
            display_print_info("    Pull the latest image to get the bundled Chromium:");
            display_print_info("      docker pull ghcr.io/nousresearch/hermes-agent:latest");
            free(browser_cmd);
            return 0;
        }

        /* agent-browser resolves lazily via npx in the C runtime — drive the
         * npx install path directly. */
        char *npx_bin = tci_which("npx");
        if (!npx_bin) {
            display_print_warning(
                "    npx not found - install Chromium manually: npx agent-browser install --with-deps");
            free(browser_cmd);
            return 0;
        }
        const char *install_argv[8];
        install_argv[0] = npx_bin;
        install_argv[1] = "--ignore-scripts";
        install_argv[2] = "-y";
        install_argv[3] = AGENT_BROWSER_NPX_SPEC;
        install_argv[4] = "install";
        install_argv[5] = "--with-deps";
        install_argv[6] = NULL;
        tci_run_chromium_install((char *const *)install_argv);
        free(npx_bin);
        free(browser_cmd);
        return 0;
    }

    if (strcmp(post_setup_key, "browser_use_cli") == 0) {
        tools_config_ensure_browser_use_cli(true);
        return 0;
    }

    if (strcmp(post_setup_key, "camofox") == 0) {
        char *root = tci_join(tci_project_root(),
                              "node_modules/@askjo/camofox-browser");
        struct stat st;
        bool camofox_dir_exists = root && stat(root, &st) == 0 && S_ISDIR(st.st_mode);
        char *npm_bin = tci_which("npm");
        if (camofox_dir_exists) {
            display_print_success("    Camofox already installed, nothing to do");
        } else if (npm_bin) {
            display_print_info("    Installing Camofox browser server...");
            const char *argv[] = { npm_bin, "install", "--silent",
                                   "--workspaces=false", NULL };
            tci_cap_t cap;
            /* cwd=PROJECT_ROOT */
            char cwd[4096];
            bool have_cwd = getcwd(cwd, sizeof cwd) != NULL;
            if (have_cwd) chdir(tci_project_root());
            int rc = tci_run_capture(argv, NULL, 600.0, true, &cap);
            if (have_cwd) chdir(cwd);
            free(cap.out); free(cap.err);
            if (rc == 0)
                display_print_success("    Camofox installed");
            else
                display_print_warning(
                    "    npm install failed - run manually: npm install --workspaces=false");
        }
        if (camofox_dir_exists) {
            display_print_info("    Start the Camofox server:");
            display_print_info("      npx @askjo/camofox-browser");
            display_print_info("    First run downloads the Camoufox engine (~300MB)");
            display_print_info("    Or use Docker: docker run -p 9377:9377 -e CAMOFOX_PORT=9377 jo-inc/camofox-browser");
        } else if (!npm_bin) {
            display_print_warning("    Node.js not found. Install Camofox via Docker:");
            display_print_info("      docker run -p 9377:9377 -e CAMOFOX_PORT=9377 jo-inc/camofox-browser");
        }
        free(root);
        free(npm_bin);
        return 0;
    }

    if (strcmp(post_setup_key, "cua_driver") == 0) {
        tools_config_install_cua_driver(false, false, true);
        return 0;
    }

    if (strcmp(post_setup_key, "faster_whisper") == 0) {
        const char *args[] = { "-U", "faster-whisper", "--quiet" };
        const char *lines[] = {
            "    Model sizes: tiny, base (default), small, medium, large-v3",
            "    Change via stt.local.model in ~/.hermes/config.yaml",
        };
        tci_pip_post_setup("faster_whisper", args, 3, "faster-whisper",
                           lines, 2, "    Run manually: uv pip install -U faster-whisper");
        return 0;
    }

    if (strcmp(post_setup_key, "kittentts") == 0) {
        static const char WHEEL_URL[] =
            "https://github.com/KittenML/KittenTTS/releases/download/"
            "0.8.1/kittentts-0.8.1-py3-none-any.whl";
        const char *args[] = { "-U", WHEEL_URL, "soundfile", "--quiet" };
        const char *lines[] = {
            "    Voices: Jasper, Bella, Luna, Bruno, Rosie, Hugo, Kiki, Leo",
            "    Models: KittenML/kitten-tts-nano-0.8-int8 (25MB), micro (41MB), mini (80MB)",
        };
        char kittentts_manual[256];
        snprintf(kittentts_manual, sizeof kittentts_manual,
                 "    Run manually: uv pip install -U '%s' soundfile", WHEEL_URL);
        tci_pip_post_setup("kittentts", args, 4, "kittentts",
                           lines, 2, kittentts_manual);
        return 0;
    }

    if (strcmp(post_setup_key, "piper") == 0) {
        if (tools_config_module_installed("piper")) {
            display_print_success("    piper-tts is already installed");
        } else {
            const char *args[] = { "-U", "piper-tts", "--quiet" };
            display_print_info(
                "    Installing piper-tts (~14MB wheel, voices downloaded on first use)...");
            tools_config_proc_result_t res;
            if (tools_config_pip_install((char *const *)args, 3, 300, true, &res) != 0 ||
                res.returncode != 0) {
                display_print_warning("    piper-tts install failed:");
                char m[384];
                snprintf(m, sizeof m, "      %.300s", res.err ? res.err : "");
                display_print_info(m);
                display_print_info("    Run manually: uv pip install -U piper-tts");
                free(res.out); free(res.err);
                return 0;
            }
            free(res.out); free(res.err);
            display_print_success("    piper-tts installed");
        }
        display_print_info("    Default voice: en_US-lessac-medium (downloaded on first TTS call)");
        display_print_info("    Full voice list: https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/VOICES.md");
        display_print_info("    Switch voices by setting tts.piper.voice in ~/.hermes/config.yaml");
        return 0;
    }

    if (strcmp(post_setup_key, "ddgs") == 0) {
        if (tools_config_module_installed("ddgs")) {
            display_print_success("    ddgs is already installed");
        } else {
            const char *args[] = { "-U", "ddgs", "--quiet" };
            display_print_info("    Installing ddgs (DuckDuckGo search package)...");
            tools_config_proc_result_t res;
            if (tools_config_pip_install((char *const *)args, 3, 300, true, &res) != 0 ||
                res.returncode != 0) {
                display_print_warning("    ddgs install failed:");
                char m[384];
                snprintf(m, sizeof m, "      %.300s", res.err ? res.err : "");
                display_print_info(m);
                display_print_info("    Run manually: uv pip install -U ddgs");
                free(res.out); free(res.err);
                return 0;
            }
            free(res.out); free(res.err);
            display_print_success("    ddgs installed");
        }
        display_print_info("    No API key required. DuckDuckGo enforces server-side rate limits.");
        display_print_info("    Pair with an extract provider if you also need web_extract.");
        return 0;
    }

    if (strcmp(post_setup_key, "spotify") == 0) {
        /* Run the full `hermes auth spotify` flow. The C auth surface exposes
         * login_spotify_command; a failure maps to the "did not complete"
         * branch rather than failing the toolset enable. */
        extern int auth_login_spotify_command(const char *arg);
        display_print_info("    Starting Spotify login...");
        int rc = auth_login_spotify_command(NULL);
        if (rc == 0) {
            display_print_success("    Spotify authenticated");
        } else {
            /* User aborted the wizard, or OAuth failed — don't fail the
             * toolset enable; they can retry with `hermes auth spotify`. */
            display_print_warning("    Spotify login did not complete.");
            display_print_info("    Run later: hermes auth spotify");
        }
        return 0;
    }

    if (strcmp(post_setup_key, "langfuse") == 0) {
        extern json_t *pcmd_get_enabled_set(const char *hermes_home);
        extern int pcmd_save_enabled_set(const char *hermes_home, json_t *set);
        if (tools_config_module_installed("langfuse")) {
            display_print_success("    langfuse SDK already installed");
        } else {
            display_print_info("    Installing langfuse SDK...");
            const char *args[] = { "langfuse", "--quiet" };
            tools_config_proc_result_t res;
            if (tools_config_pip_install((char *const *)args, 2, 120, true, &res) == 0 &&
                res.returncode == 0)
                display_print_success("    langfuse SDK installed");
            else
                display_print_warning(
                    "    langfuse SDK install failed — run manually: uv pip install langfuse");
            free(res.out); free(res.err);
        }
        /* Opt the bundled observability/langfuse plugin into plugins.enabled. */
        {
            json_t *enabled = pcmd_get_enabled_set(NULL);
            bool already = false;
            if (enabled && enabled->type == JSON_ARRAY) {
                size_t n = json_len(enabled);
                for (size_t i = 0; i < n; i++) {
                    json_t *e = json_get(enabled, (int)i);
                    const char *s = json_string_value(e);
                    if (s && (strcmp(s, "observability/langfuse") == 0 ||
                              strcmp(s, "langfuse") == 0)) {
                        already = true;
                        break;
                    }
                }
            }
            if (already) {
                display_print_success("    Plugin observability/langfuse already enabled");
            } else if (enabled) {
                json_append(enabled, json_string("observability/langfuse"));
                pcmd_save_enabled_set(NULL, enabled);
                display_print_success("    Plugin observability/langfuse enabled");
            } else {
                display_print_warning("    Could not enable plugin automatically.");
                display_print_info("    Run manually: hermes plugins enable observability/langfuse");
            }
            if (enabled) json_free(enabled);
        }
        display_print_info("    Restart Hermes for tracing to take effect.");
        display_print_info("    Verify: hermes plugins list");
        return 0;
    }

    if (strcmp(post_setup_key, "xai_grok") == 0) {
        /* Shared credential bootstrap for any picker entry that talks to xAI:
         * SuperGrok-tier OAuth bearer token (preferred) or raw XAI_API_KEY. */
        char *existing_api_key = config_py_get_env_value_prefer_dotenv("XAI_API_KEY");
        if (existing_api_key && *existing_api_key) {
            display_print_success("    xAI will use your existing XAI_API_KEY");
            free(existing_api_key);
            return 0;
        }
        free(existing_api_key);

        display_print_info("    xAI needs credentials. Choose one:");
        char *choices[] = {
            (char *)"Sign in with xAI Grok OAuth (SuperGrok / Premium+) — browser login",
            (char *)"Paste an xAI API key (console.x.ai)",
            (char *)"Skip — configure later via `hermes auth add xai-oauth`",
        };
        extern int curses_radiolist(const char *title, char **items,
                                    int item_count, int selected,
                                    int cancel_returns, const char *description,
                                    int searchable);
        int idx = curses_radiolist("    How do you want xAI to authenticate?",
                                   choices, 3, 0, 0, NULL, 1);
        if (idx == 0) {
            /* Run the OAuth login flow through the CLI entry point. */
            const char *oauth_argv[] = { "hermes", "auth", "add", "xai-oauth", NULL };
            tci_cap_t cap;
            int rc = tci_run_capture(oauth_argv, NULL, 600.0, false, &cap);
            free(cap.out); free(cap.err);
            if (rc == 0)
                display_print_success("    Logged in — xAI will use these OAuth credentials");
            else
                display_print_warning(
                    "    xAI Grok OAuth login did not complete. "
                    "Run later: hermes auth add xai-oauth");
        } else if (idx == 1) {
            extern char *stp_prompt(const char *question, const char *default_value);
            char *api_key = stp_prompt("    xAI API key", NULL);
            if (api_key && *api_key) {
                config_save_env_value("XAI_API_KEY", api_key);
                display_print_success("    XAI_API_KEY saved");
            } else {
                display_print_warning(
                    "    No API key provided. Run later: hermes auth add xai-oauth");
            }
            free(api_key);
        } else {
            display_print_info("    xAI will remain inactive until credentials are configured.");
        }
        return 0;
    }

    /* Unknown key: Python falls off the elif chain silently; callers gate on
     * valid_post_setup_keys() before reaching here. */
    return 0;
}
