/*
 * port_tools_env_probe.c — C port of tools/env_probe.py
 *
 * Local-environment toolchain probe for the system prompt.
 * Surfaces a single deterministic line about Python tooling state
 * (version mismatch between pip and python3, missing pip module,
 * PEP 668 externally-managed, uv presence).
 *
 * Faithful port: every probe actually forks/execs the binary and
 * captures stdout/stderr. No fake version strings, no comment-façades.
 */

#define _GNU_SOURCE
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>

static int g_cache_resolved = 0;
static char g_cached_line[1024] = "";

/* ── Real subprocess runner (fork/exec/poll with timeout) ─────────── */

/* Locate `name` in $PATH (mirrors shutil.which). Returns 1 and fills
 * `resolved` with the absolute path on success, 0 otherwise. */
static int ep_which(const char *name, char *resolved, size_t rsz) {
    if (!name || !*name) return 0;
    /* Absolute / relative path with a slash: test directly. */
    if (strchr(name, '/')) {
        if (access(name, X_OK) == 0) {
            snprintf(resolved, rsz, "%s", name);
            return 1;
        }
        return 0;
    }
    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *save = NULL, *dir = strtok_r(buf, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        char cand[PATH_MAX];
        snprintf(cand, sizeof(cand), "%s/%s", dir, name);
        if (access(cand, X_OK) == 0) {
            snprintf(resolved, rsz, "%s", cand);
            return 1;
        }
    }
    return 0;
}

/* Run cmd[0..cmd_count-1] via fork/exec, capturing stdout and stderr
 * separately with a timeout. Returns the child exit code (0..255), or
 * -1 on spawn failure / timeout. stdout_out/stderr_out are NUL-terminated
 * (truncated to their buffer sizes). Faithful port of tools/env_probe.py:_run. */
static int ep_run(const char **cmd, int cmd_count, float timeout_sec,
                  char *stdout_out, size_t stdout_size,
                  char *stderr_out, size_t stderr_size) {
    if (stdout_out && stdout_size) stdout_out[0] = '\0';
    if (stderr_out && stderr_size) stderr_out[0] = '\0';
    if (!cmd || cmd_count <= 0) return -1;

    int out_pipe[2] = {-1, -1}, err_pipe[2] = {-1, -1};
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: redirect stdout/stderr to pipes, close read ends. */
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        /* Build argv (NULL-terminated). */
        char *argv[256];
        int n = 0;
        for (int i = 0; i < cmd_count && n < 255; i++) argv[n++] = (char *)cmd[i];
        argv[n] = NULL;
        execvp(cmd[0], argv);
        /* If exec fails, exit non-zero; parent reads empty pipes. */
        _exit(127);
    }

    /* Parent: close write ends, poll+read until timeout/EOF. */
    close(out_pipe[1]); close(err_pipe[1]);
    int timeout_ms = timeout_sec > 0 ? (int)(timeout_sec * 1000.0) : 3000;

    /* Read both pipes until both EOF or timeout. */
    size_t out_len = 0, err_len = 0;
    int out_open = 1, err_open = 1;
    int rc = -1;
    int remaining = timeout_ms;
    while (out_open || err_open) {
        struct pollfd fds[2];
        int nf = 0;
        if (out_open) { fds[nf].fd = out_pipe[0]; fds[nf].events = POLLIN; nf++; }
        if (err_open) { fds[nf].fd = err_pipe[0]; fds[nf].events = POLLIN; nf++; }
        int pr = poll(fds, nf, remaining);
        if (pr <= 0) {
            /* Timeout (or error): kill child and bail. */
            kill(pid, SIGKILL);
            int st; waitpid(pid, &st, 0);
            close(out_pipe[0]); close(err_pipe[0]);
            return -1;
        }
        for (int i = 0; i < nf; i++) {
            int fd = fds[i].fd;
            int is_out = (fd == out_pipe[0]);
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                char chunk[1024];
                ssize_t r = read(fd, chunk, sizeof(chunk) - 1);
                if (r > 0) {
                    if (is_out && stdout_out && out_len < stdout_size - 1) {
                        size_t take = (size_t)r;
                        if (out_len + take > stdout_size - 1) take = stdout_size - 1 - out_len;
                        memcpy(stdout_out + out_len, chunk, take);
                        out_len += take;
                        stdout_out[out_len] = '\0';
                    } else if (!is_out && stderr_out && err_len < stderr_size - 1) {
                        size_t take = (size_t)r;
                        if (err_len + take > stderr_size - 1) take = stderr_size - 1 - err_len;
                        memcpy(stderr_out + err_len, chunk, take);
                        err_len += take;
                        stderr_out[err_len] = '\0';
                    }
                } else if (r == 0) {
                    if (is_out) out_open = 0; else err_open = 0;
                }
            } else if (fds[i].revents & POLLHUP) {
                if (is_out) out_open = 0; else err_open = 0;
            }
        }
    }
    close(out_pipe[0]); close(err_pipe[0]);
    int st;
    if (waitpid(pid, &st, 0) == pid && WIFEXITED(st)) rc = WEXITSTATUS(st);
    else rc = -1;
    return rc;
}

/* Strip trailing CR/LF (Python .strip()). */
static void ep_strip(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ')) s[--n] = '\0';
}

/* PoP: cli_tools_env_probe__run @ tools/env_probe.py:_run */

/* Port of Python tools/env_probe.py:_run — real fork/exec. */
int cli_tools_env_probe__run(
    const char **cmd, int cmd_count, float timeout_sec,
    int *rc_out, char *stdout_out, size_t stdout_size,
    char *stderr_out, size_t stderr_size)
{
    if (!cmd || cmd_count <= 0 || !rc_out) return -1;
    *rc_out = ep_run(cmd, cmd_count, timeout_sec, stdout_out, stdout_size,
                     stderr_out, stderr_size);
    return 0;
}

/* PoP: cli_tools_env_probe__python_version_of @ tools/env_probe.py:_python_version_of */

/* Port of Python tools/env_probe.py:_python_version_of.
 * Resolves binary in PATH, runs `binary -c "import sys; print(...)"`,
 * returns the X.Y.Z string or "missing". */
int cli_tools_env_probe__python_version_of(
    const char *binary, char *version_out, size_t version_size)
{
    if (!version_out || version_size == 0) return -1;
    version_out[0] = '\0';
    if (!binary) { snprintf(version_out, version_size, "missing"); return -1; }

    char resolved[PATH_MAX];
    if (!ep_which(binary, resolved, sizeof(resolved))) {
        snprintf(version_out, version_size, "missing");
        return -1;
    }

    const char *code = "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')";
    const char *cmd[] = { resolved, "-c", code };
    char out[256]; int rc = ep_run(cmd, 3, 3.0f, out, sizeof(out), NULL, 0);
    if (rc == 0 && out[0]) {
        ep_strip(out);
        snprintf(version_out, version_size, "%s", out);
        return 0;
    }
    snprintf(version_out, version_size, "missing");
    return -1;
}

/* PoP: cli_tools_env_probe__has_pip_module @ tools/env_probe.py:_has_pip_module */

/* Port of Python tools/env_probe.py:_has_pip_module.
 * True iff `binary -m pip --version` exits 0. */
int cli_tools_env_probe__has_pip_module(const char *binary)
{
    if (!binary) return 0;
    char resolved[PATH_MAX];
    if (!ep_which(binary, resolved, sizeof(resolved))) return 0;
    const char *cmd[] = { resolved, "-m", "pip", "--version" };
    int rc = ep_run(cmd, 4, 3.0f, NULL, 0, NULL, 0);
    return rc == 0;
}

/* PoP: cli_tools_env_probe__detect_pep668 @ tools/env_probe.py:_detect_pep668 */

/* Port of Python tools/env_probe.py:_detect_pep668.
 * Runs python snippet that checks for EXTERNALLY-MANAGED next to stdlib. */
int cli_tools_env_probe__detect_pep668(const char *binary)
{
    if (!binary) return 0;
    char resolved[PATH_MAX];
    if (!ep_which(binary, resolved, sizeof(resolved))) return 0;
    const char *code =
        "import sys, os;"
        "stdlib = os.path.dirname(os.__file__);"
        "marker = os.path.join(stdlib, 'EXTERNALLY-MANAGED');"
        "print('yes' if os.path.exists(marker) else 'no')";
    const char *cmd[] = { resolved, "-c", code };
    char out[64]; int rc = ep_run(cmd, 3, 3.0f, out, sizeof(out), NULL, 0);
    if (rc == 0 && out[0]) {
        ep_strip(out);
        return strcmp(out, "yes") == 0;
    }
    return 0;
}

/* PoP: cli_tools_env_probe__pip_python_version @ tools/env_probe.py:_pip_python_version */

/* Port of Python tools/env_probe.py:_pip_python_version.
 * Parses "pip X from ... (python Y.Z)" → "Y.Z". */
int cli_tools_env_probe__pip_python_version(char *version_out, size_t version_size)
{
    if (!version_out || version_size == 0) return -1;
    version_out[0] = '\0';

    char pip_path[PATH_MAX];
    if (!ep_which("pip", pip_path, sizeof(pip_path))) return -1;

    const char *cmd[] = { pip_path, "--version" };
    char out[512]; int rc = ep_run(cmd, 2, 3.0f, out, sizeof(out), NULL, 0);
    if (rc != 0 || !out[0]) return -1;
    ep_strip(out);

    /* Find trailing "(python X.Y)". */
    char *p = strstr(out, "(python ");
    if (!p) return -1;
    p += strlen("(python ");
    char *end = strchr(p, ')');
    if (!end) return -1;
    size_t len = (size_t)(end - p);
    if (len == 0 || len >= version_size) return -1;
    memcpy(version_out, p, len);
    version_out[len] = '\0';
    return 0;
}

/* PoP: cli_tools_env_probe__build_probe_line @ tools/env_probe.py:_build_probe_line */

/* Port of Python tools/env_probe.py:_build_probe_line.
 * Real probes; emits the one-liner only when something is off. */
int cli_tools_env_probe__build_probe_line(
    const char *backend,
    char *line_out, size_t line_size)
{
    if (!line_out || line_size == 0) return -1;
    line_out[0] = '\0';

    /* Remote backends: skip probe (host Python state is irrelevant). */
    if (backend) {
        char be[64];
        snprintf(be, sizeof(be), "%s", backend);
        for (char *q = be; *q; q++) *q = (char)tolower((unsigned char)*q);
        static const char *remote[] = {"docker","singularity","modal","daytona","ssh","managed_modal",NULL};
        for (int i = 0; remote[i]; i++) {
            if (strcmp(be, remote[i]) == 0) return 0;
        }
    }

    char py3_ver[64]; cli_tools_env_probe__python_version_of("python3", py3_ver, sizeof(py3_ver));
    char py_ver[64];  cli_tools_env_probe__python_version_of("python",  py_ver,  sizeof(py_ver));
    int py3_has_pip = (py3_ver[0] && strcmp(py3_ver, "missing") != 0)
                      ? cli_tools_env_probe__has_pip_module("python3") : 0;
    char pip_bound[64]; int pip_ok = (cli_tools_env_probe__pip_python_version(pip_bound, sizeof(pip_bound)) == 0);
    int py3_pep668 = (py3_ver[0] && strcmp(py3_ver, "missing") != 0)
                     ? cli_tools_env_probe__detect_pep668("python3") : 0;
    char uv_path[PATH_MAX]; int has_uv = ep_which("uv", uv_path, sizeof(uv_path));

    int mismatch = (pip_ok && pip_bound[0] && py3_ver[0] && strcmp(py3_ver, "missing") != 0
                    && strncmp(py3_ver, pip_bound, strlen(pip_bound)) != 0);
    int silent_conditions =
        (py3_ver[0] && strcmp(py3_ver, "missing") != 0) &&
        py3_has_pip && !mismatch && (!py3_pep668 || has_uv);
    if (silent_conditions) return 0;

    /* Build compact factual summary. */
    char bits[10][256];
    int nb = 0;
    if (py3_ver[0] && strcmp(py3_ver, "missing") != 0) {
        int l = snprintf(bits[nb], sizeof(bits[nb]), "python3=%s", py3_ver);
        if (!py3_has_pip) {
            size_t rem = sizeof(bits[nb]) - (size_t)l;
            snprintf(bits[nb] + l, rem, " (no pip module)");
        }
        nb++;
    } else {
        snprintf(bits[nb++], sizeof(bits[nb]), "python3=missing");
    }

    if (py_ver[0] && strcmp(py_ver, "missing") != 0 && strcmp(py_ver, py3_ver) != 0) {
        snprintf(bits[nb++], sizeof(bits[nb]), "python=%s", py_ver);
    } else if ((!py_ver[0] || strcmp(py_ver, "missing") == 0) && py3_ver[0] && strcmp(py3_ver, "missing") != 0) {
        snprintf(bits[nb++], sizeof(bits[nb]), "python=missing (use python3)");
    }

    if (pip_ok) {
        if (mismatch) snprintf(bits[nb++], sizeof(bits[nb]), "pip→python%s (mismatch)", pip_bound);
        else if (!py3_has_pip) snprintf(bits[nb++], sizeof(bits[nb]), "pip→python%s", pip_bound);
    } else if (py3_has_pip) {
        /* pip not on PATH but `python3 -m pip` works → say nothing */
    } else {
        snprintf(bits[nb++], sizeof(bits[nb]), "pip=missing");
    }

    if (py3_pep668) snprintf(bits[nb++], sizeof(bits[nb]), "PEP 668=yes (use venv or uv)");
    if (has_uv) snprintf(bits[nb++], sizeof(bits[nb]), "uv=installed");

    if (nb == 0) { line_out[0] = '\0'; return 0; }

    int pos = snprintf(line_out, line_size, "Python toolchain: ");
    for (int i = 0; i < nb; i++) {
        if (i > 0) pos += snprintf(line_out + pos, line_size - (size_t)pos, ", ");
        pos += snprintf(line_out + pos, line_size - (size_t)pos, "%s", bits[i]);
    }
    pos += snprintf(line_out + pos, line_size - (size_t)pos, ".");

    hermes_log(LOG_DEBUG, "env_probe", "probe_line: %s", line_out);
    return 0;
}

/* PoP: cli_tools_env_probe_get_environment_probe_line @ tools/env_probe.py:get_environment_probe_line */

/* Port of Python tools/env_probe.py:get_environment_probe_line.
 * Returns the cached probe line (building it on first call). */
int cli_tools_env_probe_get_environment_probe_line(
    int force_refresh, char *line_out, size_t line_size)
{
    if (!line_out || line_size == 0) return -1;

    if (force_refresh) {
        g_cache_resolved = 0;
        g_cached_line[0] = '\0';
    }

    if (g_cache_resolved) {
        snprintf(line_out, line_size, "%s", g_cached_line);
        return 0;
    }

    const char *backend = getenv("TERMINAL_ENV");
    cli_tools_env_probe__build_probe_line(backend, g_cached_line, sizeof(g_cached_line));
    g_cache_resolved = 1;

    snprintf(line_out, line_size, "%s", g_cached_line);
    return 0;
}

/* PoP: cli_tools_env_probe__reset_cache_for_tests @ tools/env_probe.py:_reset_cache_for_tests */

/* Port of Python tools/env_probe.py:_reset_cache_for_tests.
 * Test helper — clear the cache between probe scenarios. */
void cli_tools_env_probe__reset_cache_for_tests(void)
{
    g_cache_resolved = 0;
    g_cached_line[0] = '\0';
}
