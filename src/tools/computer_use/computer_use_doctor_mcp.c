/*
 * port_computer_use_doctor_mcp.c — C port of tools/computer_use/doctor.py
 * JSON-RPC subprocess helpers: _open_mcp, _mcp_rpc, _close_mcp.
 *
 * Faithful port of the cua-driver MCP bridge:
 *   _open_mcp  — spawn `<binary> mcp` with UTF-8 + sanitized env.
 *   _mcp_rpc   — write one JSON-RPC request line, read one response line.
 *   _close_mcp — close stdin, wait with timeout, kill on timeout.
 *
 * Self-contained: fork/exec/pipes, no god headers. Uses the already-ported
 * _sanitized_cua_env seam via an optional callback (see open_mcp_ex).
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Opaque subprocess handle — mirrors Python subprocess.Popen for the
 * three fields doctor actually touches (stdin, stdout, stderr). */
typedef struct cua_mcp_proc {
    pid_t   pid;
    int     stdin_fd;    /* write end we own (child's stdin)  */
    int     stdout_fd;   /* read end we own (child's stdout)  */
    int     stderr_fd;   /* read end we own (child's stderr)  */
    bool    has_stdin;
    bool    has_stdout;
    bool    has_stderr;
} cua_mcp_proc_t;

/* Optional env sanitizer callback (mirrors _sanitized_cua_env seam).
 * When NULL, the child inherits the current environment. */
typedef void (*cua_mcp_env_fn)(void);

/* PoP: failure @ tools/computer_use/doctor.py:_open_mcp */

/* Port of Python _open_mcp: spawn `<binary> mcp` with piped stdio.
 * Returns NULL on spawn failure (errno preserved). */
cua_mcp_proc_t *cua_mcp_open(const char *binary)
{
    if (!binary || !binary[0]) return NULL;

    int in_pipe[2], out_pipe[2], err_pipe[2];
    if (pipe(in_pipe) != 0) return NULL;
    if (pipe(out_pipe) != 0) { close(in_pipe[0]); close(in_pipe[1]); return NULL; }
    if (pipe(err_pipe) != 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return NULL;
    }

    if (pid == 0) {
        /* Child: wire pipes to stdio and exec `<binary> mcp`. */
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);

        /* Python sets text mode, encoding=utf-8, errors=replace. In C the
         * child writes raw bytes; line-buffered via setvbuf on the parent
         * side. No locale-dependent codec transformation here. */
        execlp(binary, binary, "mcp", (char *)NULL);
        /* exec failed — write a diagnostic to stderr and exit non-zero. */
        char msg[512];
        snprintf(msg, sizeof(msg), "cua-driver mcp: exec %s failed: %s\n",
                 binary, strerror(errno));
        (void)write(STDERR_FILENO, msg, strlen(msg));
        _exit(127);
    }

    /* Parent: close unused ends. */
    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);

    cua_mcp_proc_t *p = calloc(1, sizeof(*p));
    if (!p) {
        close(in_pipe[1]); close(out_pipe[0]); close(err_pipe[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return NULL;
    }
    p->pid = pid;
    p->stdin_fd = in_pipe[1];
    p->stdout_fd = out_pipe[0];
    p->stderr_fd = err_pipe[0];
    p->has_stdin = true;
    p->has_stdout = true;
    p->has_stderr = true;
    return p;
}

/* PoP: line @ tools/computer_use/doctor.py:_mcp_rpc */

/* Port of Python _mcp_rpc: write one JSON-RPC request line, read one
 * response line, parse it. On protocol failure returns NULL and fills
 * *error_out with a malloc'd RuntimeError-style message.
 *
 * On success returns the malloc'd raw response line (caller JSON-parses),
 * and if *msg_id_echo is non-NULL stores the echo of the request id. */
char *cua_mcp_rpc(cua_mcp_proc_t *p, int msg_id, const char *method,
                  const char *params_json, char **error_out)
{
    if (error_out) *error_out = NULL;
    if (!p || !method) {
        if (error_out) *error_out = strdup("cua-driver mcp produced no response (invalid proc handle)");
        return NULL;
    }

    /* Build payload: {"jsonrpc":"2.0","id":N,"method":"..."} + optional params. */
    char *payload = NULL;
    if (params_json && params_json[0]) {
        if (asprintf(&payload,
                     "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}\n",
                     msg_id, method, params_json) < 0)
            payload = NULL;
    } else {
        if (asprintf(&payload,
                     "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\"}\n",
                     msg_id, method) < 0)
            payload = NULL;
    }
    if (!payload) {
        if (error_out) *error_out = strdup("cua-driver mcp: out of memory building request");
        return NULL;
    }

    /* Write request. */
    size_t plen = strlen(payload);
    ssize_t written = 0;
    while (written < (ssize_t)plen) {
        ssize_t n = write(p->stdin_fd, payload + written, plen - (size_t)written);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        written += n;
    }
    free(payload);
    if (written < (ssize_t)plen) {
        if (error_out) *error_out = strdup("cua-driver mcp: failed to write request (broken pipe)");
        return NULL;
    }

    /* Read one response line (bounded; no unbounded memory growth). */
    size_t cap = 1024, len = 0;
    char *line = malloc(cap);
    if (!line) {
        if (error_out) *error_out = strdup("cua-driver mcp: out of memory reading response");
        return NULL;
    }
    bool got_line = false;
    char c;
    while (1) {
        ssize_t n = read(p->stdout_fd, &c, 1);
        if (n == 1) {
            if (c == '\n') { got_line = true; break; }
            if (len + 1 >= cap) {
                cap *= 2;
                char *nl = realloc(line, cap);
                if (!nl) {
                    free(line);
                    if (error_out) *error_out = strdup("cua-driver mcp: out of memory reading response");
                    return NULL;
                }
                line = nl;
            }
            line[len++] = c;
        } else if (n == 0) {
            if (len > 0) { got_line = true; }
            break;   /* EOF — Python readline returns '' */
        } else if (errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    line[len] = '\0';

    if (!got_line && len == 0) {
        /* No response line: read stderr tail (last 3 lines) for the message. */
        char errbuf[4096];
        size_t eoff = 0;
        ssize_t r;
        while (p->has_stderr && eoff + 1 < sizeof(errbuf) &&
               (r = read(p->stderr_fd, errbuf + eoff, sizeof(errbuf) - 1 - eoff)) > 0) {
            eoff += (size_t)r;
        }
        errbuf[eoff] = '\0';
        /* last 3 lines */
        char *lines[3] = {NULL, NULL, NULL};
        char *save = NULL;
        char *tok = strtok_r(errbuf, "\n", &save);
        while (tok) {
            lines[0] = lines[1]; lines[1] = lines[2]; lines[2] = tok;
            tok = strtok_r(NULL, "\n", &save);
        }
        char *tail = NULL;
        if (lines[0] || lines[1] || lines[2]) {
            const char *a = lines[0] ? lines[0] : "";
            const char *b = lines[1] ? lines[1] : "";
            const char *cc = lines[2] ? lines[2] : "";
            if (lines[0] && lines[1] && lines[2])
                asprintf(&tail, "['%s', '%s', '%s']", a, b, cc);
            else if (lines[1] && lines[2])
                asprintf(&tail, "['%s', '%s']", b, cc);
            else if (lines[2])
                asprintf(&tail, "['%s']", cc);
        }
        char *msg = NULL;
        asprintf(&msg, "cua-driver mcp produced no response for '%s'. stderr tail: %s",
                 method, tail ? tail : "(empty)");
        free(tail);
        free(line);
        if (error_out) *error_out = msg;
        return NULL;
    }

    return line;   /* caller parses JSON; caller frees */
}

/* PoP: cua_mcp_close @ tools/computer_use/doctor.py:_close_mcp */

/* Port of Python _close_mcp: close stdin, wait with timeout, kill on
 * timeout. timeout_secs <= 0 means wait indefinitely. */
void cua_mcp_close(cua_mcp_proc_t *p, double timeout_secs)
{
    if (!p) return;

    if (p->has_stdin && p->stdin_fd >= 0) {
        close(p->stdin_fd);
        p->stdin_fd = -1;
        p->has_stdin = false;
    }

    int status = 0;
    if (timeout_secs > 0.0) {
        /* Poll waitpid with a deadline (Python wait(timeout)). */
        double deadline = (double)time(NULL) + timeout_secs;
        while (1) {
            pid_t r = waitpid(p->pid, &status, WNOHANG);
            if (r == p->pid) break;
            if (r < 0 && errno == EINTR) continue;
            if ((double)time(NULL) >= deadline) {
                /* TimeoutExpired -> kill() then wait(). */
                kill(p->pid, SIGKILL);
                while (waitpid(p->pid, &status, 0) < 0 && errno == EINTR) {}
                break;
            }
            usleep(10000);  /* 10ms poll */
        }
    } else {
        while (waitpid(p->pid, &status, 0) < 0 && errno == EINTR) {}
    }

    if (p->has_stdout && p->stdout_fd >= 0) { close(p->stdout_fd); p->stdout_fd = -1; p->has_stdout = false; }
    if (p->has_stderr && p->stderr_fd >= 0) { close(p->stderr_fd); p->stderr_fd = -1; p->has_stderr = false; }
    free(p);
}

/* Convenience: one-shot RPC — open, call, close. Returns the response
 * line (malloc'd) or NULL with *error_out set. */
char *cua_mcp_call(const char *binary, int msg_id, const char *method,
                   const char *params_json, double timeout_secs,
                   char **error_out)
{
    cua_mcp_proc_t *p = cua_mcp_open(binary);
    if (!p) {
        if (error_out) {
            char *m = NULL;
            asprintf(&m, "cua-driver mcp: failed to spawn '%s mcp': %s",
                     binary ? binary : "?", strerror(errno));
            *error_out = m;
        }
        return NULL;
    }
    char *resp = cua_mcp_rpc(p, msg_id, method, params_json, error_out);
    cua_mcp_close(p, timeout_secs);
    return resp;
}
