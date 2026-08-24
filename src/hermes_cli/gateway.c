/*
 * port_gateway.c — Port of Python hermes_cli/gateway.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

/*
 * _capture_gateway_argv — Return the live argv of a running gateway process.
 *
 * Python: def _capture_gateway_argv(pid: int) -> list[str] | None:
 *   if pid <= 1: return None
 *   try: import psutil
 *   except ImportError: return None
 *   try: argv = list(psutil.Process(pid).cmdline() or [])
 *   except (NoSuchProcess, AccessDenied, ZombieProcess): return None
 *   if not argv: return None
 *   return argv
 *
 * In C: read /proc/<pid>/cmdline which is null-separated.
 */
#define MAX_ARGV_LEN 4096
#define MAX_ARGC 256

/* Port of Python: _capture_gateway_argv */
int _capture_gateway_argv(int pid, char** argv_out, int max_argc)
{
    if (pid <= 1 || !argv_out || max_argc <= 0) {
        return 0;
    }

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return 0; /* Process doesn't exist or no permission */
    }

    char buf[MAX_ARGV_LEN];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';

    /* Count and copy null-separated arguments */
    int argc = 0;
    size_t pos = 0;
    while (pos < (size_t)n && argc < max_argc) {
        argv_out[argc] = strdup(buf + pos);
        if (!argv_out[argc]) break;
        argc++;
        pos += strlen(buf + pos) + 1;
    }

    return argc;
}

/*
 * _capture_gateway_argv_to_json — Return gateway argv as JSON array.
 */
json_t* _capture_gateway_argv_json(int pid)
{
    char* argv[MAX_ARGC];
    int argc = _capture_gateway_argv(pid, argv, MAX_ARGC);

    json_t* result = json_new_array();
    if (!result) return NULL;

    for (int i = 0; i < argc; i++) {
        json_array_append(result, json_new_string(argv[i]));
        free(argv[i]);
    }

    return result;
}

/*
 * launch_detached_gateway_restart_by_cmdline — Launch a gateway restart.
 *
 * Python: launches a detached process with the given argv.
 */
/* Port of Python: launch_detached_gateway_restart_by_cmdline */
bool launch_detached_gateway_restart_by_cmdline(const char* run_argv[], int argc)
{
    if (!run_argv || argc <= 0) {
        hermes_log(LOG_WARNING, "port", "launch_detached_gateway_restart_by_cmdline: empty argv");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        hermes_log(LOG_ERROR, "port", "launch_detached_gateway_restart_by_cmdline: fork failed: %s", strerror(errno));
        return false;
    }

    if (pid == 0) {
        /* Child: become session leader, redirect stdio */
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        /* Build argv array */
        char** child_argv = calloc(argc + 1, sizeof(char*));
        for (int i = 0; i < argc; i++) {
            child_argv[i] = strdup(run_argv[i]);
        }
        child_argv[argc] = NULL;

        execvp(child_argv[0], child_argv);
        _exit(127);
    }

    hermes_log(LOG_INFO, "port", "Launched gateway restart pid=%d", pid);
    return true;
}

/*
 * _spawn_gateway_restart_watcher — Spawn a watcher that monitors gateway restart.
 */
/* Port of Python: _spawn_gateway_restart_watcher */
bool _spawn_gateway_restart_watcher(int old_pid, const char* run_argv[], int argc)
{
    if (old_pid <= 1) {
        return false;
    }

    /* Check the old process is still running */
    if (kill(old_pid, 0) < 0 && errno == ESRCH) {
        /* Old process is gone — launch the restart */
        return launch_detached_gateway_restart_by_cmdline(run_argv, argc);
    }

    /* Old process still running — don't interfere */
    hermes_log(LOG_WARNING, "port", "Gateway pid %d still running, not restarting", old_pid);
    return false;
}

/*
 * _guard_named_profile_under_multiplexer — Guard against conflicting profiles.
 *
 * Python: checks if a named profile is running under a multiplexer and prevents
 * conflicts.
 */
/* Port of Python: _guard_named_profile_under_multiplexer */
void _guard_named_profile_under_multiplexer(const char* profile_name, bool force)
{
    if (!profile_name || !profile_name[0]) {
        return;
    }

    /* Check for running gateway with this profile */
    char pid_path[256];
    snprintf(pid_path, sizeof(pid_path), "/tmp/hermes-gateway-%s.pid", profile_name);

    int fd = open(pid_path, O_RDONLY);
    if (fd < 0) {
        return 0; /* No PID file — no conflict */
    }

    char pid_str[32];
    ssize_t n = read(fd, pid_str, sizeof(pid_str) - 1);
    close(fd);

    if (n <= 0) return;
    pid_str[n] = '\0';

    int old_pid = atoi(pid_str);
    if (old_pid <= 1) return;

    /* Check if process is alive */
    if (kill(old_pid, 0) == 0) {
        if (force) {
            hermes_log(LOG_WARNING, "port", "Force-killing conflicting gateway pid=%d", old_pid);
            kill(old_pid, SIGTERM);
            usleep(500000);
            /* Check if it's still alive, use SIGKILL */
            if (kill(old_pid, 0) == 0) {
                kill(old_pid, SIGKILL);
            }
            unlink(pid_path);
            return;
        } else {
            hermes_log(LOG_WARNING, "port", "Gateway pid=%d already running for profile=%s", old_pid, profile_name);
            return; /* Conflict — Python raises RuntimeError, C logs warning */
        }
    }

    /* Stale PID file — remove it */
    unlink(pid_path);
    return;
}

/* ===========================================================================
 *  PID-resolution helpers — ported from hermes_cli/gateway.py
 *  These were REAL_GAP.
 * =========================================================================== */

/* PoP: unavailable @ hermes_cli/gateway.py:_get_parent_pid */
/* Returns parent PID of pid, or -1 when unavailable (portable /proc walk). */
int get_parent_pid(int pid)
{
    if (pid <= 1) return -1;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    /* Format: pid (comm) state ppid ... — comm may contain spaces/parens,
     * so scan fields: skip pid, then read comm (balanced parens), then
     * state, then ppid is the 4th field. */
    int field = 0, ppid = -1;
    char buf[512];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    const char *p = buf;
    while (*p && field < 4) {
        /* skip to next field start */
        while (*p == ' ' || *p == '\t') p++;
        if (field == 1) {
            /* comm: ( ... ) possibly with spaces/parens inside */
            if (*p == '(') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '(') depth++;
                    else if (*p == ')') {
                        depth--;
                        if (depth == 0) { p++; break; }
                    }
                    p++;
                }
            } else {
                while (*p && *p != ' ' && *p != '\t') p++;
            }
            field = 2;
            continue;
        }
        /* read a single token */
        char tok[64] = {0};
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < (int)sizeof(tok)-1) tok[i++] = *p++;
        if (field == 0) field = 1;       /* pid consumed, next is comm */
        else if (field == 2) field = 3;  /* state consumed */
        else if (field == 3) { ppid = atoi(tok); field = 4; break; }
    }
    return (ppid > 0) ? ppid : -1;
}

/* PoP: is_pid_ancestor_of_current_process @ hermes_cli/gateway.py:_is_pid_ancestor_of_current_process */
int is_pid_ancestor_of_current_process(int target_pid)
{
    if (target_pid <= 0) return 0;
    int pid = (int)getpid();
    for (int guard = 0; guard < 64; guard++) {
        if (pid == target_pid) return 1;
        int parent = get_parent_pid(pid);
        if (parent <= 0) break;
        pid = parent;
    }
    return 0;
}

/* PoP: count @ hermes_cli/gateway.py:_get_ancestor_pids */
/* Fills out[] with ancestor PIDs (including self); returns count (<=max). */
int get_ancestor_pids(int *out, int max)
{
    int pid = (int)getpid();
    int n = 0;
    for (int guard = 0; guard < 64 && n < max; guard++) {
        int found = 0;
        for (int k = 0; k < n; k++) if (out[k] == pid) { found = 1; break; }
        if (found) break;
        out[n++] = pid;
        int parent = get_parent_pid(pid);
        if (parent <= 0) break;
        pid = parent;
    }
    return n;
}

/* PoP: pids @ hermes_cli/gateway.py:_append_unique_pid */
/* Appends pid to *pids (growing the array) if valid/unique/not excluded. */
void append_unique_pid(int **pids, int *count, int *cap, int pid,
                       const int *exclude, int exclude_n)
{
    if (pid <= 0) return;
    if (pid == (int)getpid()) return;
    for (int i = 0; i < exclude_n; i++) if (exclude[i] == pid) return;
    for (int i = 0; i < *count; i++) if ((*pids)[i] == pid) return;
    if (*count >= *cap) {
        int ncap = (*cap == 0) ? 8 : *cap * 2;
        int *np = realloc(*pids, ncap * sizeof(int));
        if (!np) return;
        *pids = np; *cap = ncap;
    }
    (*pids)[(*count)++] = pid;
}

/* ===========================================================================
 *  systemd / restart helpers — ported from hermes_cli/gateway.py
 *  These were REAL_GAP.
 * =========================================================================== */

/* PoP: supports_systemd_services @ hermes_cli/gateway.py:supports_systemd_services */
/* Return true if systemd is available and usable on this platform.
 * Mirrors the Python: not termux, systemctl in PATH, WSL/container checks.
 * On non-Linux or Termux, returns false. */
bool supports_systemd_services(void)
{
    /* Not Linux → false */
    FILE *f = popen("uname -s 2>/dev/null", "r");
    if (!f) return false;
    char buf[64];
    bool is_linux = false;
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = 0;
        if (0 == strcmp(buf, "Linux"))
            is_linux = true;
    }
    pclose(f);
    if (!is_linux)
        return false;

    /* Termux check: /data/data/com.termux prefix */
    if (access("/data/data/com.termux", F_OK) == 0)
        return false;

    /* systemctl must exist in PATH */
    if (system("which systemctl >/dev/null 2>&1") != 0)
        return false;

    /* WSL check: /proc/version contains "microsoft" */
    FILE *vf = fopen("/proc/version", "r");
    if (vf) {
        char vbuf[512];
        bool is_wsl = false;
        if (fgets(vbuf, sizeof(vbuf), vf)) {
            if (strstr(vbuf, "microsoft") || strstr(vbuf, "Microsoft"))
                is_wsl = true;
        }
        fclose(vf);
        if (is_wsl) {
            /* _wsl_systemd_operational: check systemd is actually running */
            return (access("/run/systemd/system", F_OK) == 0);
        }
    }

    /* Container check: /.dockerenv or /run/.containerenv */
    if (access("/.dockerenv", F_OK) == 0 || access("/run/.containerenv", F_OK) == 0) {
        /* _container_systemd_operational: check user or system scope works */
        if (access("/run/systemd/system", F_OK) == 0)
            return true;
        char *xdg = getenv("XDG_RUNTIME_DIR");
        if (xdg) {
            char path[512];
            snprintf(path, sizeof(path), "%s/systemd", xdg);
            if (access(path, F_OK) == 0)
                return true;
        }
        return false;
    }

    return true;
}

/* PoP: get_service_pids @ hermes_cli/gateway.py:_get_service_pids */
/* Return PIDs currently managed by systemd or launchd gateway services.
 * Fills pids[] array (caller-allocated, max elements), returns count.
 * Uses popen to run systemctl list-units + show --property=MainPID. */
int get_service_pids(int *pids, int max)
{
    int count = 0;
    if (!pids || max <= 0)
        return 0;

    if (supports_systemd_services()) {
        /* Try both scopes: --user and system */
        const char *scopes[2] = {"systemctl --user", "systemctl"};
        for (int s = 0; s < 2; s++) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                     "%s list-units 'hermes-gateway*' --plain --no-legend --no-pager 2>/dev/null",
                     scopes[s]);
            FILE *fp = popen(cmd, "r");
            if (!fp)
                continue;
            char line[256];
            while (fgets(line, sizeof(line), fp) && count < max) {
                /* Parse first field: service name ending in .service */
                char *space = strchr(line, ' ');
                if (space) *space = 0;
                char *nl = strchr(line, '\n');
                if (nl) *nl = 0;
                if (strlen(line) < 8 ||
                    strcmp(line + strlen(line) - 8, ".service") != 0)
                    continue;
                char svc[128];
                snprintf(svc, sizeof(svc), "%s", line);

                /* Get MainPID */
                char show_cmd[512];
                snprintf(show_cmd, sizeof(show_cmd),
                         "%s show '%s' --property=MainPID --value 2>/dev/null",
                         scopes[s], svc);
                FILE *sfp = popen(show_cmd, "r");
                if (!sfp)
                    continue;
                char pid_buf[32];
                if (fgets(pid_buf, sizeof(pid_buf), sfp)) {
                    pid_buf[strcspn(pid_buf, "\n")] = 0;
                    int pid = atoi(pid_buf);
                    if (pid > 0) {
                        /* Deduplicate */
                        bool found = false;
                        for (int i = 0; i < count; i++)
                            if (pids[i] == pid) { found = true; break; }
                        if (!found && count < max)
                            pids[count++] = pid;
                    }
                }
                pclose(sfp);
            }
            pclose(fp);
        }
    }

    /* launchd (macOS) path: check if we're on macOS via uname */
    FILE *mf = popen("uname -s 2>/dev/null", "r");
    if (mf) {
        char mb[16];
        if (fgets(mb, sizeof(mb), mf)) {
            mb[strcspn(mb, "\n")] = 0;
            if (0 == strcmp(mb, "Darwin") && count < max) {
                /* launchctl list <label> — get_launchd_label returns the label */
                /* We don't have get_launchd_label in C; skip launchd path
                 * (the systemd path is the primary on Linux/WSL/containers) */
            }
        }
        pclose(mf);
    }

    return count;
}

/* PoP: request_gateway_self_restart @ hermes_cli/gateway.py:_request_gateway_self_restart */
/* Ask a running gateway ancestor to restart itself asynchronously.
 * Returns true if SIGUSR1 was sent, false if not possible.
 * Mirrors Python: check SIGUSR1 exists, check pid is ancestor, os.kill. */
bool request_gateway_self_restart(int pid)
{
#ifdef SIGUSR1
    if (pid <= 0)
        return false;
    if (!is_pid_ancestor_of_current_process(pid))
        return false;
    if (kill(pid, SIGUSR1) == 0)
        return true;
    /* ESRCH = process not found (already gone) — Python returns True */
    if (errno == ESRCH)
        return true;
    return false;
#else
    return false;
#endif
}

/* PoP: graceful_restart_via_sigusr1 @ hermes_cli/gateway.py:_graceful_restart_via_sigusr1 */
/* Send SIGUSR1 to a gateway PID and wait for it to exit gracefully.
 * Polls /proc/<pid> existence every 0.5s up to drain_timeout seconds.
 * Returns true if the process exited within the timeout. */
bool graceful_restart_via_sigusr1(int pid, double drain_timeout)
{
#ifdef SIGUSR1
    if (pid <= 0)
        return false;

    int rc = kill(pid, SIGUSR1);
    if (rc != 0) {
        if (errno == ESRCH)
            return true; /* Already gone — nothing to drain */
        return false;    /* Permission error or other */
    }

    /* Wait for the process to exit.
     * drain_timeout is in seconds; minimum 1.0 per Python max(drain_timeout, 1.0) */
    double timeout = drain_timeout > 1.0 ? drain_timeout : 1.0;
    double elapsed = 0.0;
    const double poll_interval = 0.5;

    while (elapsed < timeout) {
        /* Check if process still exists via /proc/<pid> */
        char proc_path[64];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
        if (access(proc_path, F_OK) != 0)
            return true; /* Process exited */

        /* Also check via kill(pid, 0) — no signal sent */
        if (kill(pid, 0) != 0 && errno == ESRCH)
            return true;

        /* Sleep poll_interval seconds (500ms) */
        usleep((useconds_t)(poll_interval * 1000000));
        elapsed += poll_interval;
    }
    /* Drain didn't finish in time */
    return false;
#else
    (void)pid;
    (void)drain_timeout;
    return false;
#endif
}

/* ════════════════════════════════════════════════════════════════════
 * Gateway lifecycle (parity batch: probe/kill/stop/service/launchd/command)
 * ════════════════════════════════════════════════════════════════════ */

#include <time.h>
#include <dirent.h>
#include "gateway_lifecycle.h"
#include "gateway_status.h"
#include "slermes_home.h"
#include "cron_jobs.h"          /* cronjobs_parse_jobs_file — tolerant JSON reader */
#include "port_config_py_helpers.h" /* config_py_is_container */
#include "hermes_util_str.h"    /* hermes_home_dir */

#define GATEWAY_SERVICE_BASE "hermes-gateway"

/* Platform/profile helpers from gateway_platform.c (declared here; the C
 * tree keeps them header-less). */
extern char *gateway_profile_suffix(void);
extern int is_macos(void);
extern int is_windows(void);
extern char *profile_profiles_root(void);   /* src/hermes_cli/profiles.c */
extern bool config_py_is_container(void);   /* port_config_py_helpers.h */

/* This install's launchd/systemd service suffix ("", or "-<profile>").
 * Mirrors Python get_systemd_unit_name/_get_launchd_label naming built on
 * _profile_suffix. Returns a malloc'd string, never NULL-empty mix: "" when
 * default home. */
static char *gateway_service_suffix(void)
{
    char *sfx = gateway_profile_suffix();
    if (!sfx || !sfx[0]) {
        free(sfx);
        return strdup("");
    }
    char *with = malloc(strlen(sfx) + 2);
    if (with) {
        with[0] = '-';
        strcpy(with + 1, sfx);
    }
    free(sfx);
    return with;
}

static void gateway_unit_name(char *out, size_t sz)
{
    char *sfx = gateway_service_suffix();
    snprintf(out, sz, "%s%s.service", GATEWAY_SERVICE_BASE, sfx ? sfx : "");
    free(sfx);
}

/* Run a command, capture stdout first line; returns exit code (or -1). */
static int gw_run_capture(const char *cmd, char *out, size_t outsz)
{
    if (out && outsz) out[0] = '\0';
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    if (out && outsz) {
        size_t n = 0;
        int c;
        while ((c = fgetc(fp)) != EOF && c != '\n' && n + 1 < outsz)
            out[n++] = (char)c;
        out[n] = '\0';
    }
    int rc = pclose(fp);
    if (rc == -1) return -1;
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}


/* PoP: probe_gateway_loop_liveness @ hermes_cli/gateway.py:probe_gateway_loop_liveness */
/* Classify a gateway PID's event loop as alive / wedged / unknown.
 *
 * Reads the loop-liveness heartbeat file (<home>/state/gateway.heartbeat)
 * that the gateway rewrites every 30s while its loop is dispatching. Never
 * fails: missing file, unreadable JSON, or PID mismatch returns UNKNOWN so
 * callers default to the safe graceful-drain path. */
const char *probe_gateway_loop_liveness(int pid, double stale_after)
{
    double stale_budget =
        (stale_after > 0.0) ? stale_after : DEFAULT_LOOP_LIVENESS_STALE_AFTER_S;

    /* path = get_loop_heartbeat_path(home): base / "state" / "gateway.heartbeat" */
    char path[4096];
    snprintf(path, sizeof(path), "%s/state/gateway.heartbeat", slermes_home());

    struct stat st;
    if (stat(path, &st) != 0)
        return GATEWAY_LOOP_UNKNOWN;

    bool used_fallback = false;
    char *jerr = NULL;
    json_t *payload = cronjobs_parse_jobs_file(path, &used_fallback, &jerr);
    free(jerr);
    if (!payload)
        return GATEWAY_LOOP_UNKNOWN;

    long long heartbeat_pid = 0;
    const json_t *pnode = json_object_get(payload, "pid");
    if (pnode && json_node_is_number(pnode))
        heartbeat_pid = (long long)pnode->num_val;
    else if (pnode && json_node_is_string(pnode))
        heartbeat_pid = strtoll(pnode->str_val ? pnode->str_val : "0", NULL, 10);
    json_free(payload);

    if (heartbeat_pid <= 0 || pid <= 0 || heartbeat_pid != (long long)pid)
        return GATEWAY_LOOP_UNKNOWN;

    double age = difftime(time(NULL), st.st_mtime);
    if (age > stale_budget)
        return GATEWAY_LOOP_WEDGED;
    return GATEWAY_LOOP_ALIVE;
}

/* PoP: find @ hermes_cli/gateway.py:find_gateway_pids */
/* Cross-platform gateway process scan (adaptation of find_gateway_pids):
 * walk /proc, keep PIDs whose cmdline matches the strict gateway-command
 * matcher (runtime entrypoints included when all_profiles), excluding self +
 * ancestor chain (#13242), service-managed PIDs, and explicit exclusions.
 * psutil-free /proc walk instead of Process.iter_children. */
static int gw_find_gateway_pids(const int *exclude_pids, size_t n_exclude,
                                bool all_profiles, int *out, int max)
{
    if (!out || max <= 0) return 0;

    int ancestors[512];
    int n_anc = get_ancestor_pids(ancestors, 512);
    int service_pids[256];
    int n_svc = get_service_pids(service_pids, 256);
    int count = 0;
    pid_t self = getpid();

    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && count < max) {
        if (!(e->d_name[0] >= '1' && e->d_name[0] <= '9')) continue;
        pid_t pid = (pid_t)atoi(e->d_name);
        if (pid == self) continue;

        bool excluded = false;
        for (int i = 0; i < n_anc; i++)
            if (ancestors[i] == pid) { excluded = true; break; }
        for (int i = 0; !excluded && i < n_svc; i++)
            if (service_pids[i] == pid) { excluded = true; break; }
        for (size_t i = 0; !excluded && exclude_pids && i < n_exclude; i++)
            if (exclude_pids[i] == pid) { excluded = true; break; }
        if (excluded) continue;

        char cmdpath[64], cmdline[8192];
        snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", pid);
        FILE *fp = fopen(cmdpath, "r");
        if (!fp) continue;
        size_t nrd = fread(cmdline, 1, sizeof(cmdline) - 1, fp);
        fclose(fp);
        if (nrd == 0) continue;
        cmdline[nrd] = '\0';
        /* NUL-separated argv → space-joined command string. */
        for (size_t i = 0; i < nrd; i++)
            if (cmdline[i] == '\0') cmdline[i] = ' ';

        bool match = gwstatus_looks_like_gateway_command_line(cmdline);
        if (!match && all_profiles)
            match = gwstatus_looks_like_gateway_runtime_command_line(cmdline);
        if (!match) continue;

        bool dup = false;
        for (int i = 0; i < count; i++)
            if (out[i] == pid) { dup = true; break; }
        if (dup) continue;
        out[count++] = pid;
    }
    closedir(d);
    return count;
}

/* PoP: kill_gateway_processes @ hermes_cli/gateway.py:kill_gateway_processes */
/* Terminate any running gateway processes found by the scan.
 *   force        — platform force-kill instead of graceful terminate
 *   exclude_pids — PIDs to skip (e.g. just-restarted service-managed PIDs)
 *   all_profiles — widen the scan across profiles
 * Returns the number of processes successfully signalled. */
int kill_gateway_processes(bool force, const int *exclude_pids,
                           size_t n_exclude, bool all_profiles)
{
    int pids[1024];
    int npids = gw_find_gateway_pids(exclude_pids, n_exclude, all_profiles,
                                     pids, 1024);
    int killed = 0;

    for (int i = 0; i < npids; i++) {
        errno = 0;
        int rc = gwstatus_terminate_pid(pids[i], force);
        if (rc == 0) {
            killed++;
        } else if (errno == ESRCH) {
            /* Process already gone */
        } else if (errno == EPERM) {
            printf("\xe2\x9a\xa0 Permission denied to signal PID %d\n", pids[i]);
        } else {
            printf("Failed to signal PID %d: %s\n", pids[i], strerror(errno));
        }
    }
    return killed;
}

/* PoP: stop_profile_gateway @ hermes_cli/gateway.py:stop_profile_gateway */
/* Stop only the gateway for the current profile (home-scoped).
 *
 * Uses the PID file written at start, so it only signals THIS profile's
 * gateway. Writes a planned-stop marker first so the dying gateway treats
 * the SIGTERM as intentional, then bounded-waits (20 x 0.5s) and removes the
 * stale pid file. True when a recorded process was stopped. */
bool stop_profile_gateway(void)
{
    pid_t pid = gwstatus_get_running_pid(NULL, false);
    if (pid <= 0)
        return false;

    gwstatus_write_planned_stop_marker(pid);

    if (kill(pid, SIGTERM) != 0) {
        if (errno == ESRCH) {
            /* Already gone */
        } else if (errno == EPERM) {
            printf("\xe2\x9a\xa0 Permission denied to signal PID %d\n", pid);
            return false;
        }
    }

    /* Wait briefly for it to exit (cross-platform existence check). */
    for (int i = 0; i < 20; i++) {
        if (!gwstatus_pid_exists(pid))
            break;
        struct timespec ts = {0, 500 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }

    if (gwstatus_get_running_pid(NULL, false) <= 0)
        gwstatus_remove_pid_file();

    return true;
}

/* PoP: ensure_gateway_service @ hermes_cli/gateway.py:ensure_gateway_service */
/* Install and start the gateway service without prompting.
 *
 * Zero-decision path used by setup/import: when this host supports a service
 * manager and no gateway service exists yet, install user-scope and start.
 * Never prompts, never aborts the caller. Scope is always least-surprising/
 * no-privilege: user-scope systemd unit on Linux, LaunchAgent on macOS.
 *
 * Adaptation note: the C port renders no unit/plist files itself (those
 * install helpers are deferred in this tree), so "install" means enabling an
 * already-rendered user unit when present on disk; otherwise we fall into
 * the same warning branch as Python's generic except handler. */
bool ensure_gateway_service(const char *context)
{
    (void)context;
    if (config_py_is_container()) {
        /* Containers use restart policies, not service managers. */
        printf("Start the gateway to bring your bots online:\n");
        printf("   hermes gateway run          # Run as container main process\n\n");
        printf("For automatic restarts, use a Docker restart policy:\n");
        printf("   docker run --restart unless-stopped ...\n");
        return false;
    }

    bool supports_systemd = supports_systemd_services();
    if (!(supports_systemd || is_macos() || is_windows())) {
        printf("  No supported service manager found on this host.\n");
        printf("  Run the gateway in the foreground with: hermes gateway\n");
        return false;
    }

    char unit[256];
    gateway_unit_name(unit, sizeof(unit));

    /* Service-state probes: real systemctl queries on systemd; on macOS,
     * `launchctl list` reporting the label (installed) with a live PID. */
    bool installed = false, running = false, conflict = false;
    if (supports_systemd) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "systemctl --user is-active %s 2>/dev/null",
                 unit);
        running = gw_run_capture(cmd, NULL, 0) == 0;
        snprintf(cmd, sizeof(cmd), "systemctl --user cat %s >/dev/null 2>&1",
                 unit);
        installed = gw_run_capture(cmd, NULL, 0) == 0;
        if (installed) {
            /* Both user and system units would fight over bot tokens — don't
             * pile a fresh install onto a conflicted state. */
            char syspath[512];
            snprintf(syspath, sizeof(syspath), "/etc/systemd/system/%s", unit);
            conflict = access(syspath, F_OK) == 0;
        }
    } else if (is_macos()) {
        char label[256], cmd[512], out[128];
        char *sfx = gateway_service_suffix();
        snprintf(label, sizeof(label), "ai.hermes.gateway%s", sfx ? sfx : "");
        free(sfx);
        snprintf(cmd, sizeof(cmd), "launchctl list %s 2>/dev/null", label);
        int rc = gw_run_capture(cmd, out, sizeof(out));
        installed = (rc == 0);
        running = (rc == 0) && strstr(out, "\"PID\"") != NULL;
    }

    if (running)
        return true;

    if (!installed) {
        if (supports_systemd && conflict) {
            printf("  \xe2\x9a\xa0 Both a user and a system gateway unit exist; "
                   "they would fight over bot tokens.\n");
            return false;
        }
        printf("  Installing the gateway background service ...\n");
        if (supports_systemd) {
            char cmd[768];
            snprintf(cmd, sizeof(cmd),
                     "test -f \"$HOME/.config/systemd/user/%s\" && "
                     "systemctl --user daemon-reload && "
                     "systemctl --user enable %s >/dev/null 2>&1",
                     unit, unit);
            if (gw_run_capture(cmd, NULL, 0) != 0) {
                printf("  \xe2\x9a\xa0 Gateway service install did not complete.\n");
                printf("  You can retry manually: hermes gateway install\n");
                return false;
            }
        } else if (is_macos()) {
            char cmd[1024];
            char *sfx = gateway_service_suffix();
            snprintf(cmd, sizeof(cmd),
                     "launchctl bootstrap gui/$UID ~/Library/LaunchAgents/"
                     "ai.hermes.gateway%s.plist >/dev/null 2>&1",
                     sfx ? sfx : "");
            free(sfx);
            if (gw_run_capture(cmd, NULL, 0) != 0) {
                printf("  \xe2\x9a\xa0 Gateway service install did not complete.\n");
                printf("  You can retry manually: hermes gateway install\n");
                return false;
            }
        } else {
            /* Windows Scheduled Task registration unavailable in this C
             * runtime; fail soft like the Python generic except branch. */
            printf("  \xe2\x9a\xa0 Gateway service install did not complete.\n");
            printf("  You can retry manually: hermes gateway install\n");
            return false;
        }
    }

    /* Start it. */
    bool started = false;
    if (supports_systemd) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "systemctl --user start %s >/dev/null 2>&1",
                 unit);
        started = gw_run_capture(cmd, NULL, 0) == 0;
    } else if (is_macos()) {
        char cmd[512], label[256];
        char *sfx = gateway_service_suffix();
        snprintf(label, sizeof(label), "ai.hermes.gateway%s", sfx ? sfx : "");
        free(sfx);
        snprintf(cmd, sizeof(cmd),
                 "launchctl kickstart gui/$UID/%s >/dev/null 2>&1", label);
        started = gw_run_capture(cmd, NULL, 0) == 0;
    }

    if (started) {
        printf("  \xe2\x9c\x93 Gateway service running (cron jobs + messaging platforms).\n");
        return true;
    }
    printf("  \xe2\x9a\xa0 Gateway service could not be started.\n");
    printf("  You can retry manually: hermes gateway install\n");
    return false;
}

/* PoP: launchd_gateway_labels_for_install @ hermes_cli/gateway.py:launchd_gateway_labels_for_install */
/* launchd gateway labels for every profile of THIS install.
 *
 * Derived from the install's profile layout (rooted at the profiles root),
 * NOT by globbing ~/Library/LaunchAgents — that directory is shared per-user,
 * so a sandboxed home must never enumerate another install's fleet. Root
 * label first, then profile labels sorted by name; names that cannot map to
 * a service suffix are skipped. Caller frees each string + the array. */
char **launchd_gateway_labels_for_install(size_t *out_n)
{
    if (out_n) *out_n = 0;
    size_t cap = 8;
    char **labels = malloc(cap * sizeof(char *));
    if (!labels) return NULL;
    size_t n = 0;

    labels[n++] = strdup("ai.hermes.gateway");

    char **names = malloc(16 * sizeof(char *));
    size_t nc = 0, ncap = 16;
    char *root = profile_profiles_root();
    if (root && names) {
        DIR *d = opendir(root);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.') continue;
                /* ^[a-z0-9][a-z0-9_-]{0,63}$ — same naming rule as
                 * _profile_suffix; others never got a predictable label. */
                const char *s = e->d_name;
                size_t len = strlen(s), j;
                if (len == 0 || len > 64) continue;
                if (!((s[0] >= 'a' && s[0] <= 'z') ||
                      (s[0] >= '0' && s[0] <= '9'))) continue;
                for (j = 1; j < len; j++) {
                    char ch = s[j];
                    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                          ch == '_' || ch == '-')) break;
                }
                if (j != len) continue;
                if (nc == ncap) {
                    ncap *= 2;
                    names = realloc(names, ncap * sizeof(char *));
                    if (!names) break;
                }
                names[nc++] = strdup(s);
            }
            closedir(d);
        }
    }
    if (root) free(root);

    /* sorted(profile_labels) */
    for (size_t i = 0; names && i + 1 < nc; i++)
        for (size_t k = i + 1; k < nc; k++)
            if (strcmp(names[i], names[k]) > 0) {
                char *t = names[i]; names[i] = names[k]; names[k] = t;
            }
    for (size_t i = 0; names && i < nc; i++) {
        if (n == cap) {
            cap *= 2;
            char **nl = realloc(labels, cap * sizeof(char *));
            if (!nl) { free(names[i]); continue; }
            labels = nl;
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "ai.hermes.gateway-%s", names[i]);
        labels[n++] = strdup(buf);
        free(names[i]);
    }
    free(names);

    if (out_n) *out_n = n;
    return labels;
}

/* PoP: wait_for_launchd_gateway_supervision @ hermes_cli/gateway.py:wait_for_launchd_gateway_supervision */
/* Poll launchd until it is supervising a live gateway process.
 *
 * Restart requests are asynchronous (self-restart handoff, detached plist
 * reload), so "returned without raising" != "the service is up". Judge by a
 * live supervised PID in `launchctl list <label>` output, never an exit
 * code. label NULL → this install's default gateway label. */
bool wait_for_launchd_gateway_supervision(const char *label, double timeout,
                                          double poll_interval)
{
    char lbl[256];
    if (label && label[0]) {
        snprintf(lbl, sizeof(lbl), "%s", label);
    } else {
        char *sfx = gateway_service_suffix();
        snprintf(lbl, sizeof(lbl), "ai.hermes.gateway%s", sfx ? sfx : "");
        free(sfx);
    }

    struct timespec ts0;
    clock_gettime(CLOCK_MONOTONIC, &ts0);
    double start = ts0.tv_sec + ts0.tv_nsec / 1e9;
    double deadline = start +
        (timeout > 0.0 ? timeout : LAUNCHD_SUPERVISION_VERIFY_TIMEOUT);
    double poll = poll_interval > 0.01 ? poll_interval : 0.01;

    for (;;) {
        char cmd[512], out[512];
        snprintf(cmd, sizeof(cmd), "launchctl list %s 2>/dev/null", lbl);
        if (gw_run_capture(cmd, out, sizeof(out)) == 0 &&
            strstr(out, "\"PID\"") != NULL)
            return true;

        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double now = now_ts.tv_sec + now_ts.tv_nsec / 1e9;
        if (now >= deadline)
            return false;

        time_t secs = (time_t)poll;
        struct timespec ts = {secs, (long)((poll - (double)secs) * 1e9)};
        nanosleep(&ts, NULL);
    }
}

/* PoP: gateway_command @ hermes_cli/gateway.py:gateway_command */
/* Handle gateway subcommands, mapping the inner runner's failure sentinels:
 *   GW_ERR_USER_SYSTEMD_UNAVAILABLE   → unreachable D-Bus session guidance
 *   GW_ERR_SYSTEM_SCOPE_REQUIRES_ROOT → system-scope sudo hint
 * Both exit 1, matching the Python sys.exit(1) branches; anything else is
 * passed straight through. */
int gateway_command(int (*inner)(void *ctx, int argc, char **argv),
                    void *ctx, int argc, char **argv)
{
    int rc = inner(ctx, argc, argv);
    if (rc == GW_ERR_USER_SYSTEMD_UNAVAILABLE) {
        printf("\xe2\x9c\x97 User systemd not reachable:\n");
        printf("  Your session D-Bus may be unavailable (fresh SSH shell,\n"
               "  no linger, container). Enable lingering with\n"
               "  `sudo loginctl enable-linger $USER`, or run the gateway in\n"
               "  the foreground: hermes gateway run\n");
        exit(1);
    }
    if (rc == GW_ERR_SYSTEM_SCOPE_REQUIRES_ROOT) {
        printf("System-scope gateway actions require root. Re-run with sudo,\n"
               "or stay on the default user scope: hermes gateway install\n");
        exit(1);
    }
    return rc;
}
