/* tools_config_cua.c — faithful C11 port of the cua-driver install/status
 * surface of hermes_cli/tools_config.py, plus the two cua_backend.py
 * resolution helpers that surface delegates to.
 *
 * Reuses: hermes_json.h (libjson) only. No god header, no umbrella include.
 * Self-contained: the PATH search, bounded subprocess capture and semver
 * validation live here because nothing else in the tree exposes them.
 */
#define _GNU_SOURCE
#include "tools_config_cua.h"

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

/* ── module constants (mirror the Python module-level names) ───────────── */

/* PoP: cua_backend_candidate_cua_driver_commands @ tools/computer_use/cua_backend.py:_CUA_DRIVER_CMD_ENV */
static const char *const CUA_DRIVER_CMD_ENV = "HERMES_CUA_DRIVER_CMD";
/* PoP: cua_backend_candidate_cua_driver_commands @ tools/computer_use/cua_backend.py:_CUA_DRIVER_DEFAULT_CMD */
static const char *const CUA_DRIVER_DEFAULT_CMD = "cua-driver";
/* PoP: tools_config_cua_driver_env @ tools/computer_use/cua_backend.py:_CUA_TELEMETRY_ENV_VAR */
static const char *const CUA_TELEMETRY_ENV_VAR = "CUA_DRIVER_RS_TELEMETRY_ENABLED";

/* _CUA_DRIVER_RUNTIME_CONTRACT_MIN = (0, 20, 0) */
static const int CUA_CONTRACT_MIN[3] = { 0, 20, 0 };

/* _CUA_DRIVER_RUNTIME_CONTRACT_ARGS */
typedef struct { const char *command; const char *const *args; } contract_row_t;
static const char *const CONTRACT_MCP_ARGS[]   = { "--grant", "--socket", NULL };
static const char *const CONTRACT_SERVE_ARGS[] = { "--approve-capability-manifest",
                                                   "--capability-manifest",
                                                   "--embedded",
                                                   "--permission-mode",
                                                   "--socket", NULL };
static const char *const CONTRACT_STOP_ARGS[]  = { "--socket", NULL };
static const contract_row_t CUA_CONTRACT_ARGS[] = {
    { "mcp",   CONTRACT_MCP_ARGS },
    { "serve", CONTRACT_SERVE_ARGS },
    { "stop",  CONTRACT_STOP_ARGS },
};

/* ── small local utilities (self-contained, no cross-module coupling) ──── */

void tcc_free_list(char **list, size_t n) {
    if (!list) return;
    for (size_t i = 0; i < n; i++) free(list[i]);
    free(list);
}

static char *tcc_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *tcc_join(const char *a, const char *b) {
    /* os.path.join with a POSIX separator; b never starts with '/' here. */
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

static const char *tcc_home(void) {
    const char *h = getenv("HOME");
    return (h && *h) ? h : "";
}

/* os.path.expanduser("~/...") */
static char *tcc_expanduser(const char *path) {
    if (!path) return NULL;
    if (path[0] == '~' && (path[1] == '\0' || path[1] == '/'))
        return tcc_join(tcc_home(), path[1] ? path + 2 : "");
    return tcc_strdup(path);
}

/* Python: os.environ.get(key, "").strip() */
static char *tcc_env_stripped(const char *key) {
    const char *v = getenv(key);
    if (!v) return tcc_strdup("");
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    size_t n = strlen(v);
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' ||
                 v[n - 1] == '\n' || v[n - 1] == '\r')) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, v, n);
    out[n] = '\0';
    return out;
}

/* cua_backend.py:_has_path_separator */
static bool tcc_has_path_separator(const char *s) {
    return s && strchr(s, '/') != NULL;
}

/* shutil.which() — accepts an explicit path (executable check) or a bare
 * name resolved against PATH. Returns malloc'd resolved path or NULL. */
static char *tcc_which(const char *cmd) {
    if (!cmd || !*cmd) return NULL;
    if (tcc_has_path_separator(cmd)) {
        struct stat st;
        if (stat(cmd, &st) == 0 && S_ISREG(st.st_mode) &&
            access(cmd, X_OK) == 0)
            return tcc_strdup(cmd);
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
        if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) &&
            access(cand, X_OK) == 0)
            return cand;
        free(cand);
        if (!sep) break;
        p = sep + 1;
    }
    return NULL;
}

/* Bounded subprocess capture (subprocess.run(capture_output=True,
 * text=True, timeout=..., stdin=DEVNULL, env=...)). Returns the exit status,
 * -1 when the spawn itself failed, -2 on timeout. */
typedef struct { char *out; char *err; } tcc_capture_t;

static void tcc_append(char **buf, size_t *len, size_t *cap,
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

static int tcc_run_capture(const char *const *argv, char *const *envp,
                           double timeout_s, tcc_capture_t *cap) {
    cap->out = NULL;
    cap->err = NULL;
    int op[2], ep[2];
    if (pipe(op) != 0) return -1;
    if (pipe(ep) != 0) { close(op[0]); close(op[1]); return -1; }
    pid_t pid = fork();
    if (pid < 0) {
        close(op[0]); close(op[1]); close(ep[0]); close(ep[1]);
        return -1;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
        dup2(op[1], STDOUT_FILENO);
        dup2(ep[1], STDERR_FILENO);
        close(op[0]); close(op[1]); close(ep[0]); close(ep[1]);
        if (envp)
            execve(argv[0], (char *const *)argv, envp);
        else
            execv(argv[0], (char *const *)argv);
        _exit(127);
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
                if (is_out) tcc_append(&cap->out, &olen, &ocap, buf, (size_t)n);
                else        tcc_append(&cap->err, &elen, &ecap, buf, (size_t)n);
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
    if (!cap->out) cap->out = tcc_strdup("");
    if (!cap->err) cap->err = tcc_strdup("");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

/* ── cua_backend.py resolution helpers ────────────────────────────────── */

/* PoP: cua_backend_candidate_cua_driver_commands @ tools/computer_use/cua_backend.py:_candidate_cua_driver_commands */
char **cua_backend_candidate_cua_driver_commands(const char *override,
                                                 size_t *out_n) {
    char *configured;
    if (override != NULL) {
        /* (override if override is not None else env).strip() */
        const char *v = override;
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        size_t n = strlen(v);
        while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' ||
                     v[n - 1] == '\n' || v[n - 1] == '\r')) n--;
        configured = (char *)malloc(n + 1);
        if (!configured) { if (out_n) *out_n = 0; return NULL; }
        memcpy(configured, v, n);
        configured[n] = '\0';
    } else {
        configured = tcc_env_stripped(CUA_DRIVER_CMD_ENV);
        if (!configured) { if (out_n) *out_n = 0; return NULL; }
    }

    if (*configured) {
        /* An explicit override is authoritative. */
        char **list = (char **)calloc(1, sizeof *list);
        if (!list) { free(configured); if (out_n) *out_n = 0; return NULL; }
        list[0] = configured;
        if (out_n) *out_n = 1;
        return list;
    }
    free(configured);

    /* POSIX branch of the Python function (sys.platform != "win32"). */
    const char *home = tcc_home();
    char *cands[5];
    size_t n = 0;
    cands[n++] = tcc_strdup(CUA_DRIVER_DEFAULT_CMD);
    cands[n++] = tcc_join(home, ".local/bin/cua-driver");
    cands[n++] = tcc_join(home, ".cargo/bin/cua-driver");
    cands[n++] = tcc_strdup("/opt/homebrew/bin/cua-driver");
    cands[n++] = tcc_strdup("/usr/local/bin/cua-driver");
    char **list = (char **)calloc(n, sizeof *list);
    if (!list) { for (size_t i = 0; i < n; i++) free(cands[i]);
                 if (out_n) *out_n = 0; return NULL; }
    for (size_t i = 0; i < n; i++) list[i] = cands[i];
    if (out_n) *out_n = n;
    return list;
}

/* PoP: cua_backend_resolve_cua_driver_cmd @ tools/computer_use/cua_backend.py:resolve_cua_driver_cmd */
char *cua_backend_resolve_cua_driver_cmd(const char *override) {
    size_t n = 0;
    char **cands = cua_backend_candidate_cua_driver_commands(override, &n);
    char *result = NULL;
    for (size_t i = 0; i < n && !result; i++) {
        char *expanded = tcc_expanduser(cands[i]);
        if (!expanded) continue;
        if (tcc_has_path_separator(expanded)) {
            char *w = tcc_which(expanded);
            if (w) { free(w); result = expanded; expanded = NULL; }
        } else {
            char *resolved = tcc_which(expanded);
            if (resolved) result = resolved;
        }
        free(expanded);
    }
    tcc_free_list(cands, n);
    return result;
}

static json_t *contract_result(bool ready, const char *binary,
                               const char *version, const char *reason) {
    json_t *o = json_object();
    json_set(o, "ready", json_bool(ready));
    json_set(o, "binary", binary ? json_string(binary) : json_null());
    json_set(o, "version", version ? json_string(version) : json_null());
    json_set(o, "reason", json_string(reason ? reason : ""));
    return o;
}

/* Python: re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)(?:[-+].*)?", raw) */
static bool contract_parse_semver(const char *raw, int out[3]) {
    if (!raw) return false;
    const char *p = raw;
    if (*p == 'v') p++;
    for (int part = 0; part < 3; part++) {
        if (*p < '0' || *p > '9') return false;
        long v = 0;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
        out[part] = (int)v;
        if (part < 2) { if (*p != '.') return false; p++; }
    }
    if (*p == '\0') return true;
    return (*p == '-' || *p == '+');  /* (?:[-+].*)? */
}

/* PoP: cua_backend_cua_driver_runtime_contract_status @ tools/computer_use/cua_backend.py:cua_driver_runtime_contract_status */
json_t *cua_backend_cua_driver_runtime_contract_status(const char *binary) {
    char *resolved = (binary && *binary) ? tcc_strdup(binary)
                                         : cua_backend_resolve_cua_driver_cmd(NULL);
    if (!resolved || !*resolved) {
        free(resolved);
        return contract_result(false, NULL, NULL, "cua-driver is not installed");
    }

    size_t envn = 0;
    char **env = tools_config_cua_driver_env(&envn);
    const char *argv[3] = { resolved, "manifest", NULL };
    tcc_capture_t cap;
    /* timeout=5.0 on POSIX (15.0 only on win32). */
    int rc = tcc_run_capture(argv, (char *const *)env, 5.0, &cap);
    tcc_free_list(env, envn);

    if (rc == -1 || rc == -2) {
        char reason[256];
        snprintf(reason, sizeof reason, "manifest check failed: %s",
                 rc == -2 ? "timed out" : strerror(errno));
        free(cap.out); free(cap.err);
        json_t *r = contract_result(false, resolved, NULL, reason);
        free(resolved);
        return r;
    }

    if (rc != 0) {
        /* detail = (stderr or stdout or "manifest command failed").strip();
         * reason = detail.splitlines()[-1][:200] */
        const char *detail = (cap.err && *cap.err) ? cap.err
                            : ((cap.out && *cap.out) ? cap.out
                                                     : "manifest command failed");
        /* strip */
        const char *s = detail;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t sl = strlen(s);
        while (sl && (s[sl-1]==' '||s[sl-1]=='\t'||s[sl-1]=='\n'||s[sl-1]=='\r')) sl--;
        /* last line */
        size_t start = 0;
        for (size_t i = 0; i < sl; i++)
            if (s[i] == '\n') start = i + 1;
        size_t len = sl - start;
        if (len > 200) len = 200;
        char reason[201];
        memcpy(reason, s + start, len);
        reason[len] = '\0';
        free(cap.out); free(cap.err);
        json_t *r = contract_result(false, resolved, NULL, reason);
        free(resolved);
        return r;
    }

    json_t *manifest = json_parse(cap.out ? cap.out : "", NULL);
    free(cap.out); free(cap.err);
    if (!manifest || manifest->type != JSON_OBJECT) {
        if (manifest) json_free(manifest);
        json_t *r = contract_result(false, resolved, NULL,
                                    "driver manifest is missing or invalid");
        free(resolved);
        return r;
    }

    /* raw_version = str(manifest.get("binary_version") or "").strip() */
    const char *bv = json_get_str(manifest, "binary_version", "");
    char *raw_version = tcc_strdup(bv ? bv : "");
    {
        char *s = raw_version;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t n = strlen(s);
        while (n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) n--;
        memmove(raw_version, s, n);
        raw_version[n] = '\0';
    }

    int ver[3];
    if (!contract_parse_semver(raw_version, ver)) {
        json_t *r = contract_result(false, resolved,
                                    *raw_version ? raw_version : NULL,
                                    "driver manifest does not report a semantic version");
        free(raw_version); free(resolved); json_free(manifest);
        return r;
    }
    bool older = false;
    for (int i = 0; i < 3; i++) {
        if (ver[i] < CUA_CONTRACT_MIN[i]) { older = true; break; }
        if (ver[i] > CUA_CONTRACT_MIN[i]) break;
    }
    if (older) {
        json_t *r = contract_result(false, resolved, raw_version,
                                    "Hermes computer use requires cua-driver 0.20.0 or newer");
        free(raw_version); free(resolved); json_free(manifest);
        return r;
    }

    /* mcp_invocation.args must be a non-empty list of strings. */
    json_t *invocation = json_obj_get(manifest, "mcp_invocation");
    json_t *inv_args = (invocation && invocation->type == JSON_OBJECT)
                       ? json_obj_get(invocation, "args") : NULL;
    bool inv_ok = inv_args && inv_args->type == JSON_ARRAY && json_len(inv_args) > 0;
    if (inv_ok) {
        for (size_t i = 0; i < json_len(inv_args); i++) {
            json_t *a = json_get(inv_args, i);
            if (!a || a->type != JSON_STRING) { inv_ok = false; break; }
        }
    }
    if (!inv_ok) {
        json_t *r = contract_result(false, resolved, raw_version,
                                    "driver manifest does not provide an MCP launch command");
        free(raw_version); free(resolved); json_free(manifest);
        return r;
    }

    /* advertised = {name: {arg names}} from manifest["subcommands"] */
    json_t *subs = json_obj_get(manifest, "subcommands");
    char missing[512];
    size_t mlen = 0;
    missing[0] = '\0';
    for (size_t r = 0; r < sizeof CUA_CONTRACT_ARGS / sizeof *CUA_CONTRACT_ARGS; r++) {
        const contract_row_t *row = &CUA_CONTRACT_ARGS[r];
        /* locate the advertised command entry */
        json_t *entry_args = NULL;
        if (subs && subs->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(subs); i++) {
                json_t *cmd = json_get(subs, i);
                if (!cmd || cmd->type != JSON_OBJECT) continue;
                json_t *nm = json_obj_get(cmd, "name");
                if (!nm || nm->type != JSON_STRING || !nm->str_val) continue;
                if (strcmp(nm->str_val, row->command) != 0) continue;
                entry_args = json_obj_get(cmd, "args");
                break;
            }
        }
        /* sorted(required - advertised) — CONTRACT_*_ARGS are pre-sorted. */
        for (const char *const *req = row->args; *req; req++) {
            bool found = false;
            if (entry_args && entry_args->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len(entry_args) && !found; i++) {
                    json_t *a = json_get(entry_args, i);
                    if (!a || a->type != JSON_OBJECT) continue;
                    json_t *an = json_obj_get(a, "name");
                    if (an && an->type == JSON_STRING && an->str_val &&
                        strcmp(an->str_val, *req) == 0)
                        found = true;
                }
            }
            if (found) continue;
            int w = snprintf(missing + mlen, sizeof missing - mlen,
                             "%s%s %s", mlen ? ", " : "", row->command, *req);
            if (w > 0 && (size_t)w < sizeof missing - mlen) mlen += (size_t)w;
        }
    }
    if (mlen) {
        char reason[600];
        snprintf(reason, sizeof reason, "driver manifest is missing: %s", missing);
        json_t *r = contract_result(false, resolved, raw_version, reason);
        free(raw_version); free(resolved); json_free(manifest);
        return r;
    }

    json_t *r = contract_result(true, resolved, raw_version, "");
    free(raw_version); free(resolved); json_free(manifest);
    return r;
}

/* ── hermes_cli/tools_config.py cua surface ────────────────────────────── */

/* PoP: tools_config_cua_driver_cmd @ hermes_cli/tools_config.py:_cua_driver_cmd */
char *tools_config_cua_driver_cmd(void) {
    char *v = tcc_env_stripped(CUA_DRIVER_CMD_ENV);
    if (v && *v) return v;
    free(v);
    return tcc_strdup(CUA_DRIVER_DEFAULT_CMD);
}

/* PoP: tools_config_resolved_cua_driver_cmd @ hermes_cli/tools_config.py:_resolved_cua_driver_cmd */
char *tools_config_resolved_cua_driver_cmd(void) {
    return cua_backend_resolve_cua_driver_cmd(NULL);
}

/* PoP: tools_config_cua_install_home @ hermes_cli/tools_config.py:_cua_install_home */
char *tools_config_cua_install_home(void) {
    const char *env = getenv("CUA_DRIVER_RS_HOME");
    if (env && *env) return tcc_strdup(env);
    return tcc_join(tcc_home(), ".cua-driver");
}

/* PoP: tools_config_cua_install_lock_dir @ hermes_cli/tools_config.py:_cua_install_lock_dir */
char *tools_config_cua_install_lock_dir(void) {
    char *home = tools_config_cua_install_home();
    if (!home) return NULL;
    char *out = tcc_join(home, "packages/.install.lock.d");
    free(home);
    return out;
}

/* PoP: tools_config_cua_windows_install_lock_file @ hermes_cli/tools_config.py:_cua_windows_install_lock_file */
char *tools_config_cua_windows_install_lock_file(void) {
    char *home = tools_config_cua_install_home();
    if (!home) return NULL;
    char *out = tcc_join(home, "install.lock");
    free(home);
    return out;
}

/* PoP: tools_config_ps_single_quote @ hermes_cli/tools_config.py:_ps_single_quote */
char *tools_config_ps_single_quote(const char *value) {
    const char *v = value ? value : "";
    size_t quotes = 0;
    for (const char *p = v; *p; p++) if (*p == '\'') quotes++;
    char *out = (char *)malloc(strlen(v) + quotes + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = '\'';
    for (const char *p = v; *p; p++) {
        out[k++] = *p;
        if (*p == '\'') out[k++] = '\'';
    }
    out[k++] = '\'';
    out[k] = '\0';
    return out;
}

/* PoP: tools_config_cua_version_summary @ hermes_cli/tools_config.py:_cua_version_summary */
char *tools_config_cua_version_summary(const char *raw, size_t limit) {
    if (limit == 0) limit = 120;  /* Python keyword default */
    const char *p = raw ? raw : "";
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        /* line.strip() */
        const char *s = p;
        size_t l = len;
        while (l && (*s == ' ' || *s == '\t' || *s == '\r')) { s++; l--; }
        while (l && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]=='\r')) l--;
        if (l) {
            if (l > limit) l = limit;
            char *out = (char *)malloc(l + 1);
            if (!out) return NULL;
            memcpy(out, s, l);
            out[l] = '\0';
            return out;
        }
        if (!nl) break;
        p = nl + 1;
    }
    return tcc_strdup("");
}

/* PoP: tools_config_cua_install_target_writable @ hermes_cli/tools_config.py:_cua_install_target_writable */
bool tools_config_cua_install_target_writable(void) {
#if defined(__APPLE__)
    const char *applications_dir = "/Applications";
    struct stat st;
    if (stat(applications_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return true;
    return access(applications_dir, W_OK) == 0;
#else
    /* sys.platform != "darwin" → True */
    return true;
#endif
}

/* tools/computer_use/cua_backend.py:_cua_telemetry_disabled — user opt-in via
 * computer_use.cua_telemetry; disabled (i.e. True) by default. */
static bool tcc_cua_telemetry_disabled(void) {
    const char *v = getenv("HERMES_CUA_TELEMETRY");
    if (!v || !*v) return true;
    return !(strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 ||
             strcasecmp(v, "yes") == 0 || strcasecmp(v, "on") == 0);
}

/* PoP: tools_config_cua_driver_env @ hermes_cli/tools_config.py:_cua_driver_env */
char **tools_config_cua_driver_env(size_t *out_n) {
    extern char **environ;
    size_t base = 0;
    while (environ[base]) base++;
    bool inject = tcc_cua_telemetry_disabled();
    /* dict(os.environ) then env[VAR] = "0" — replace an existing entry. */
    size_t cap = base + 2;
    char **out = (char **)calloc(cap, sizeof *out);
    if (!out) { if (out_n) *out_n = 0; return NULL; }
    size_t n = 0;
    size_t klen = strlen(CUA_TELEMETRY_ENV_VAR);
    for (size_t i = 0; i < base; i++) {
        if (inject && strncmp(environ[i], CUA_TELEMETRY_ENV_VAR, klen) == 0 &&
            environ[i][klen] == '=')
            continue;  /* superseded below */
        out[n++] = tcc_strdup(environ[i]);
    }
    if (inject) {
        char buf[256];
        snprintf(buf, sizeof buf, "%s=0", CUA_TELEMETRY_ENV_VAR);
        out[n++] = tcc_strdup(buf);
    }
    out[n] = NULL;
    if (out_n) *out_n = n;
    return out;
}

/* _CUA_DRIVER_CONTRACT_CACHE: dict = {} */
static struct {
    bool valid;
    char *path;
    long long mtime_ns;
    long long size;
    double checked_at;
    json_t *state;
} CUA_DRIVER_CONTRACT_CACHE;

static double tcc_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* PoP: tools_config_cua_driver_contract_status @ hermes_cli/tools_config.py:_cua_driver_contract_status */
json_t *tools_config_cua_driver_contract_status(const char *binary) {
    char *resolved = (binary && *binary) ? tcc_strdup(binary)
                                         : tools_config_resolved_cua_driver_cmd();
    if (!resolved || !*resolved) {
        free(resolved);
        return cua_backend_cua_driver_runtime_contract_status(NULL);
    }

    struct stat st;
    if (stat(resolved, &st) != 0) {
        json_t *r = cua_backend_cua_driver_runtime_contract_status(resolved);
        free(resolved);
        return r;
    }
    long long mtime_ns = (long long)st.st_mtim.tv_sec * 1000000000LL +
                         (long long)st.st_mtim.tv_nsec;
    long long size = (long long)st.st_size;

    double now = tcc_monotonic();
    if (CUA_DRIVER_CONTRACT_CACHE.valid &&
        CUA_DRIVER_CONTRACT_CACHE.path &&
        strcmp(CUA_DRIVER_CONTRACT_CACHE.path, resolved) == 0 &&
        CUA_DRIVER_CONTRACT_CACHE.mtime_ns == mtime_ns &&
        CUA_DRIVER_CONTRACT_CACHE.size == size &&
        now - CUA_DRIVER_CONTRACT_CACHE.checked_at < 30.0) {
        json_t *copy = json_copy(CUA_DRIVER_CONTRACT_CACHE.state);
        free(resolved);
        return copy;
    }

    json_t *state = cua_backend_cua_driver_runtime_contract_status(resolved);
    free(CUA_DRIVER_CONTRACT_CACHE.path);
    if (CUA_DRIVER_CONTRACT_CACHE.state) json_free(CUA_DRIVER_CONTRACT_CACHE.state);
    CUA_DRIVER_CONTRACT_CACHE.valid = true;
    CUA_DRIVER_CONTRACT_CACHE.path = resolved;   /* owned by the cache now */
    CUA_DRIVER_CONTRACT_CACHE.mtime_ns = mtime_ns;
    CUA_DRIVER_CONTRACT_CACHE.size = size;
    CUA_DRIVER_CONTRACT_CACHE.checked_at = now;
    CUA_DRIVER_CONTRACT_CACHE.state = json_copy(state);
    return state;
}

/* PoP: tools_config_cua_driver_autostart_registered_windows @ hermes_cli/tools_config.py:_cua_driver_autostart_registered_windows */
bool tools_config_cua_driver_autostart_registered_windows(void) {
#if defined(_WIN32)
    const char *argv[5] = { "schtasks.exe", "/Query", "/TN",
                            "cua-driver-serve", NULL };
    tcc_capture_t cap;
    int rc = tcc_run_capture(argv, NULL, 10.0, &cap);
    free(cap.out);
    free(cap.err);
    return rc == 0;
#else
    /* sys.platform != "win32" → False */
    return false;
#endif
}

/* PoP: tools_config_cua_driver_install_ready @ hermes_cli/tools_config.py:_cua_driver_install_ready */
bool tools_config_cua_driver_install_ready(void) {
    json_t *status = tools_config_cua_driver_contract_status(NULL);
    bool ready = json_get_bool(status, "ready", false);
    json_free(status);
    if (!ready) return false;
#if defined(_WIN32)
    return tools_config_cua_driver_autostart_registered_windows();
#else
    return true;
#endif
}

/* PoP: tools_config_post_setup_already_installed @ hermes_cli/tools_config.py:_post_setup_already_installed */
bool tools_config_post_setup_already_installed(const char *post_setup_key) {
    /* _POST_SETUP_INSTALLED = {"cua_driver": _cua_driver_install_ready} */
    if (post_setup_key && strcmp(post_setup_key, "cua_driver") == 0)
        return tools_config_cua_driver_install_ready();
    /* No install-state check registered → assume satisfied. */
    return true;
}

/* PoP: tools_config_post_setup_no_window_flags @ hermes_cli/tools_config.py:_post_setup_no_window_flags */
int tools_config_post_setup_no_window_flags(bool streams_to_console) {
#if defined(_WIN32)
    const int CREATE_NO_WINDOW = 0x08000000;  /* windows_hide_flags() */
    int flags = CREATE_NO_WINDOW;
    if (streams_to_console && isatty(STDOUT_FILENO)) return 0;
    return flags;
#else
    (void)streams_to_console;
    /* windows_hide_flags() is 0 on POSIX → return 0. */
    return 0;
#endif
}
