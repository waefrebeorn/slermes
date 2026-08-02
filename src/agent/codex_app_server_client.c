/*
 * codex_app_server_client.c — JSON-RPC 2.0 client for `codex app-server`.
 *
 * Spawns `codex app-server` as a subprocess, speaks newline-delimited
 * JSON-RPC 2.0 over stdio. Threaded reader dispatches replies to
 * pending requests and routes notifications + server requests to queues.
 *
 * Maps to Python agent/transports/codex_app_server.py (399 lines).
 * Port of Python: codex_app_server.py — CodexClient class methods:
 *   new, initialize, close, free, request, notify, respond, respond_error,
 *   take_notification, take_server_request, is_alive, stderr_tail, last_error
 */

#include "codex_app_server_client.h"
#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#define MAX_PENDING 64
#define MAX_QUEUE  256
#define MAX_STDERR 500
#define READ_BUF  65536

/* Pending request */
typedef struct {
    int  id;
    char method[128];
    double sent_at;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    char *response;     /* NULL until reply arrives */
    bool  replied;
} pending_req_t;

/* Queue entry */
typedef struct {
    char *json;
} queue_entry_t;

/* Notification/server-request queue */
typedef struct {
    queue_entry_t entries[MAX_QUEUE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} msg_queue_t;

struct codex_client_t {
    pid_t pid;
    int   stdin_fd;     /* write to server */
    int   stdout_fd;    /* read from server */
    int   stderr_fd;    /* read server stderr */

    int next_id;
    pending_req_t pending[MAX_PENDING];
    pthread_mutex_t pending_mutex;

    msg_queue_t notifications;
    msg_queue_t server_requests;

    char *stderr_lines[MAX_STDERR];
    int   stderr_count;
    int   stderr_head;
    pthread_mutex_t stderr_mutex;

    pthread_t reader_thread;
    pthread_t stderr_thread;
    bool reader_running;
    bool closed;

    char last_error[512];
};

/* ---- queue helpers ---- */

static void queue_init(msg_queue_t *q) {
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

static void queue_free(msg_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % MAX_QUEUE;
        free(q->entries[idx].json);
    }
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

static bool queue_push(msg_queue_t *q, const char *json) {
    pthread_mutex_lock(&q->mutex);
    if (q->count >= MAX_QUEUE) {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }
    int idx = (q->head + q->count) % MAX_QUEUE;
    q->entries[idx].json = json ? strdup(json) : NULL;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

/* Returns malloc'd string or NULL. Caller frees. */
static char *queue_pop(msg_queue_t *q, double timeout_sec) {
    pthread_mutex_lock(&q->mutex);
    if (q->count == 0) {
        if (timeout_sec <= 0) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        }
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (long)timeout_sec;
        ts.tv_nsec += (long)((timeout_sec - (long)timeout_sec) * 1e9);
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        int rc = pthread_cond_timedwait(&q->cond, &q->mutex, &ts);
        if (rc == ETIMEDOUT || q->count == 0) {
            pthread_mutex_unlock(&q->mutex);
            return NULL;
        }
    }
    int idx = q->head;
    char *json = q->entries[idx].json;
    q->entries[idx].json = NULL;
    q->head = (q->head + 1) % MAX_QUEUE;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return json;
}

/* ---- stderr ring buffer ---- */

static void stderr_add(codex_client_t *c, const char *line) {
    pthread_mutex_lock(&c->stderr_mutex);
    if (c->stderr_count < MAX_STDERR) {
        c->stderr_lines[c->stderr_count++] = strdup(line);
    } else {
        free(c->stderr_lines[c->stderr_head]);
        c->stderr_lines[c->stderr_head] = strdup(line);
        c->stderr_head = (c->stderr_head + 1) % MAX_STDERR;
    }
    pthread_mutex_unlock(&c->stderr_mutex);
}

/* Port of Python gateway/platforms/email.py:_dispatch_message(). */
/* ---- reader thread ---- */

static void dispatch_message(codex_client_t *c, json_node_t *msg);

static void *reader_thread(void *arg) {
    codex_client_t *c = (codex_client_t *)arg;
    char buf[READ_BUF];
    int pos = 0;

    while (!c->closed) {
        ssize_t n = read(c->stdout_fd, buf + pos, sizeof(buf) - pos - 1);
        if (n <= 0) break;
        pos += (int)n;
        buf[pos] = '\0';

        /* Process complete lines */
        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            if (start[0] != '\0') {
                json_node_t *parsed = json_parse(start, NULL);
                if (parsed) {
                    dispatch_message(c, parsed);
                    json_free(parsed);
                } else {
                    char err_msg[256];
                    snprintf(err_msg, sizeof(err_msg), "<non-json on stdout> %.200s", start);
                    stderr_add(c, err_msg);
                }
            }
            start = nl + 1;
        }
        /* Move remaining to front */
        int remaining = (int)(start - buf);
        if (remaining > 0 && remaining < pos) {
            memmove(buf, start, pos - remaining);
            pos -= remaining;
        } else {
            pos = 0;
        }
        buf[pos] = '\0';
    }
    return NULL;
}

static void *stderr_thread(void *arg) {
    codex_client_t *c = (codex_client_t *)arg;
    char buf[READ_BUF];
    int pos = 0;

    while (!c->closed) {
        ssize_t n = read(c->stderr_fd, buf + pos, sizeof(buf) - pos - 1);
        if (n <= 0) break;
        pos += (int)n;
        buf[pos] = '\0';

        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            if (start[0] != '\0') {
                stderr_add(c, start);
            }
            start = nl + 1;
        }
        int remaining = (int)(start - buf);
        if (remaining > 0 && remaining < pos) {
            memmove(buf, start, pos - remaining);
            pos -= remaining;
        } else {
            pos = 0;
        }
        buf[pos] = '\0';
    }
    return NULL;
}

/* ---- dispatch ---- */

static void dispatch_message(codex_client_t *c, json_node_t *msg) {
    /* Check for id + result/error = reply */
    json_node_t *id_node = json_obj_get(msg, "id");
    json_node_t *result_node = json_obj_get(msg, "result");
    json_node_t *error_node = json_obj_get(msg, "error");
    json_node_t *method_node = json_obj_get(msg, "method");

    if (id_node && (result_node || error_node) && !method_node) {
        /* Reply to a pending request */
        int rid = (int)json_get_num(id_node, "", 0);
        pthread_mutex_lock(&c->pending_mutex);
        for (int i = 0; i < MAX_PENDING; i++) {
            if (c->pending[i].id == rid && !c->pending[i].replied) {
                /* Serialize the full message as JSON */
                char *json_str = json_serialize(msg);
                c->pending[i].response = json_str;
                c->pending[i].replied = true;
                pthread_cond_signal(&c->pending[i].cond);
                break;
            }
        }
        pthread_mutex_unlock(&c->pending_mutex);
        return;
    }

    if (id_node && method_node) {
        /* Server-initiated request */
        char *json_str = json_serialize(msg);
        queue_push(&c->server_requests, json_str);
        free(json_str);
        return;
    }

    if (method_node && !id_node) {
        /* Notification */
        char *json_str = json_serialize(msg);
        queue_push(&c->notifications, json_str);
        free(json_str);
        return;
    }
}

/* ---- public API ---- */

codex_client_t *codex_client_new(const char *codex_bin,
                                  const char *codex_home,
                                  const char **extra_args,
                                  int extra_args_count) {
    codex_client_t *c = (codex_client_t *)calloc(1, sizeof(codex_client_t));
    if (!c) return NULL;

    queue_init(&c->notifications);
    queue_init(&c->server_requests);
    pthread_mutex_init(&c->pending_mutex, NULL);
    pthread_mutex_init(&c->stderr_mutex, NULL);
    c->next_id = 1;

    for (int i = 0; i < MAX_PENDING; i++) {
        c->pending[i].id = 0;
        c->pending[i].response = NULL;
        c->pending[i].replied = false;
        pthread_mutex_init(&c->pending[i].mutex, NULL);
        pthread_cond_init(&c->pending[i].cond, NULL);
    }

    /* Build command args */
    int total_args = 2 + extra_args_count + 1; /* codex app-server [extra...] NULL */
    char **args = (char **)calloc((size_t)total_args, sizeof(char *));
    if (!args) { free(c); return NULL; }
    args[0] = strdup(codex_bin ? codex_bin : "codex");
    args[1] = strdup("app-server");
    for (int i = 0; i < extra_args_count; i++) {
        args[2 + i] = strdup(extra_args[i]);
    }
    args[total_args - 1] = NULL;

    /* Build env */
    /* We set RUST_LOG=warn and optionally CODEX_HOME via setenv in child */

    /* Create pipes */
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        snprintf(c->last_error, sizeof(c->last_error), "pipe() failed: %s", strerror(errno));
        free(args);
        free(c);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        snprintf(c->last_error, sizeof(c->last_error), "fork() failed: %s", strerror(errno));
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        free(args);
        free(c);
        return NULL;
    }

    if (pid == 0) {
        /* Child */
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Set RUST_LOG=warn */
        setenv("RUST_LOG", "warn", 1);
        if (codex_home) setenv("CODEX_HOME", codex_home, 1);

        execvp(args[0], args);
        _exit(127);
    }

    /* Parent */
    for (int i = 0; i < total_args; i++) free(args[i]);
    free(args);

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    c->pid = pid;
    c->stdin_fd = stdin_pipe[1];
    c->stdout_fd = stdout_pipe[0];
    c->stderr_fd = stderr_pipe[0];

    /* Start reader threads */
    c->reader_running = true;
    pthread_create(&c->reader_thread, NULL, reader_thread, c);
    pthread_create(&c->stderr_thread, NULL, stderr_thread, c);

    return c;
}

int codex_client_initialize(codex_client_t *c,
                            const char *client_name,
                            const char *client_title,
                            const char *client_version,
                            double timeout_sec) {
    /* Build initialize params */
    char params[1024];
    snprintf(params, sizeof(params),
             "{\"clientInfo\":{\"name\":\"%s\",\"title\":\"%s\",\"version\":\"%s\"},\"capabilities\":{}}",
             client_name ? client_name : "hermes",
             client_title ? client_title : "Hermes Agent",
             client_version ? client_version : "0.1");

    char *resp = codex_client_request(c, "initialize", params, timeout_sec);
    if (!resp) {
        snprintf(c->last_error, sizeof(c->last_error), "initialize failed or timed out");
        return -1;
    }
    free(resp);

    /* Send initialized notification */
    codex_client_notify(c, "initialized", "{}");
    return 0;
}

void codex_client_close(codex_client_t *c) {
    if (!c || c->closed) return;
    c->closed = true;

    /* Close stdin to signal EOF to server */
    if (c->stdin_fd > 0) {
        close(c->stdin_fd);
        c->stdin_fd = 0;
    }

    /* Terminate subprocess */
    if (c->pid > 0) {
        kill(c->pid, SIGTERM);
        usleep(100000); /* 100ms grace */
        int status;
        pid_t w = waitpid(c->pid, &status, WNOHANG);
        if (w == 0) {
            kill(c->pid, SIGKILL);
            waitpid(c->pid, &status, 0);
        }
        c->pid = 0;
    }

    /* Join reader threads */
    if (c->reader_running) {
        /* Close stdout_fd to unblock reader */
        if (c->stdout_fd > 0) { close(c->stdout_fd); c->stdout_fd = 0; }
        if (c->stderr_fd > 0) { close(c->stderr_fd); c->stderr_fd = 0; }
        pthread_join(c->reader_thread, NULL);
        pthread_join(c->stderr_thread, NULL);
        c->reader_running = false;
    }
}

void codex_client_free(codex_client_t *c) {
    if (!c) return;
    codex_client_close(c);
    queue_free(&c->notifications);
    queue_free(&c->server_requests);
    pthread_mutex_destroy(&c->pending_mutex);
    pthread_mutex_destroy(&c->stderr_mutex);
    for (int i = 0; i < MAX_PENDING; i++) {
        pthread_mutex_destroy(&c->pending[i].mutex);
        pthread_cond_destroy(&c->pending[i].cond);
        free(c->pending[i].response);
    }
    for (int i = 0; i < c->stderr_count; i++) {
        free(c->stderr_lines[i]);
    }
    free(c);
}

static int take_id(codex_client_t *c) {
    pthread_mutex_lock(&c->pending_mutex);
    int id = c->next_id++;
    /* Find free slot */
    for (int i = 0; i < MAX_PENDING; i++) {
        if (c->pending[i].id == 0 || c->pending[i].replied) {
            c->pending[i].id = id;
            c->pending[i].replied = false;
            c->pending[i].response = NULL;
            c->pending[i].sent_at = 0;
            break;
        }
    }
    pthread_mutex_unlock(&c->pending_mutex);
    return id;
}

char *codex_client_request(codex_client_t *c,
                           const char *method,
                           const char *params_json,
                           double timeout_sec) {
    if (!c || c->closed) return NULL;

    int id = take_id(c);

    /* Build JSON-RPC request */
    char req[65536];
    if (params_json && params_json[0]) {
        snprintf(req, sizeof(req),
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":%s}",
                 id, method, params_json);
    } else {
        snprintf(req, sizeof(req),
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\",\"params\":{}}",
                 id, method);
    }

    /* Send */
    size_t len = strlen(req);
    ssize_t written = write(c->stdin_fd, req, len);
    if (written != (ssize_t)len) {
        snprintf(c->last_error, sizeof(c->last_error), "write to stdin failed: %s", strerror(errno));
        return NULL;
    }
    write(c->stdin_fd, "\n", 1);

    /* Wait for reply */
    char *result = NULL;
    pthread_mutex_lock(&c->pending_mutex);
    for (int i = 0; i < MAX_PENDING; i++) {
        if (c->pending[i].id == id) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (long)timeout_sec;
            ts.tv_nsec += (long)((timeout_sec - (long)timeout_sec) * 1e9);
            if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }

            while (!c->pending[i].replied && !c->closed) {
                int rc = pthread_cond_timedwait(&c->pending[i].cond, &c->pending_mutex, &ts);
                if (rc == ETIMEDOUT) break;
            }

            if (c->pending[i].replied) {
                result = c->pending[i].response;
                c->pending[i].response = NULL;
            } else {
                snprintf(c->last_error, sizeof(c->last_error),
                         "method '%s' timed out after %.1fs", method, timeout_sec);
            }
            c->pending[i].id = 0;
            break;
        }
    }
    pthread_mutex_unlock(&c->pending_mutex);

    return result;
}

int codex_client_notify(codex_client_t *c,
                        const char *method,
                        const char *params_json) {
    if (!c || c->closed) return -1;

    char req[65536];
    if (params_json && params_json[0]) {
        snprintf(req, sizeof(req),
                 "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
                 method, params_json);
    } else {
        snprintf(req, sizeof(req),
                 "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":{}}",
                 method);
    }

    size_t len = strlen(req);
    ssize_t written = write(c->stdin_fd, req, len);
    if (written != (ssize_t)len) return -1;
    write(c->stdin_fd, "\n", 1);
    return 0;
}
/* PoP: codex_client_respond @ agent/transports/codex_app_server.py:respond */

int codex_client_respond(codex_client_t *c,
                         int request_id,
                         const char *result_json) {
    if (!c || c->closed) return -1;

    char req[65536];
    snprintf(req, sizeof(req),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}",
             request_id, result_json ? result_json : "{}");

    size_t len = strlen(req);
    ssize_t written = write(c->stdin_fd, req, len);
    if (written != (ssize_t)len) return -1;
    write(c->stdin_fd, "\n", 1);
    return 0;
}
/* PoP: codex_client_respond_error @ agent/transports/codex_app_server.py:respond_error */

int codex_client_respond_error(codex_client_t *c,
                                const char *request_id_str,
                                int error_code,
                                const char *error_message) {
    if (!c || c->closed) return -1;

    char req[65536];
    /* id may be a string — preserve the original format */
    char id_buf[128];
    if (request_id_str && request_id_str[0]) {
        /* Check if it looks numeric */
        char *end;
        strtol(request_id_str, &end, 10);
        if (*end == '\0') {
            snprintf(id_buf, sizeof(id_buf), "%s", request_id_str);
        } else {
            snprintf(id_buf, sizeof(id_buf), "\"%s\"", request_id_str);
        }
    } else {
        snprintf(id_buf, sizeof(id_buf), "0");
    }

    snprintf(req, sizeof(req),
             "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
             id_buf, error_code, error_message ? error_message : "Unknown error");

    size_t len = strlen(req);
    ssize_t written = write(c->stdin_fd, req, len);
    if (written != (ssize_t)len) return -1;
    write(c->stdin_fd, "\n", 1);
    return 0;
}

/* PoP: codex_client_take_notification @ agent/transports/codex_app_server.py:take_notification */
char *codex_client_take_notification(codex_client_t *c, double timeout_sec) {
    if (!c) return NULL;
    return queue_pop(&c->notifications, timeout_sec);
}

/* PoP: codex_client_take_server_request @ agent/transports/codex_app_server.py:take_server_request */
char *codex_client_take_server_request(codex_client_t *c, double timeout_sec) {
    if (!c) return NULL;
    return queue_pop(&c->server_requests, timeout_sec);
}
/* PoP: codex_client_is_alive @ agent/transports/codex_app_server.py:is_alive */

bool codex_client_is_alive(codex_client_t *c) {
    if (!c || c->pid <= 0) return false;
    int status;
    pid_t w = waitpid(c->pid, &status, WNOHANG);
    return (w == 0); /* 0 = still running */
}

/* PoP: codex_client_stderr_tail @ agent/transports/codex_app_server.py:stderr_tail */
char *codex_client_stderr_tail(codex_client_t *c, int n_lines) {
    if (!c) return NULL;
    pthread_mutex_lock(&c->stderr_mutex);
    int count = c->stderr_count < n_lines ? c->stderr_count : n_lines;
    size_t total = 0;
    for (int i = 0; i < count; i++) {
        int idx = (c->stderr_head + c->stderr_count - count + i) % MAX_STDERR;
        total += strlen(c->stderr_lines[idx]) + 1;
    }
    char *result = (char *)malloc(total + 1);
    if (!result) { pthread_mutex_unlock(&c->stderr_mutex); return NULL; }
    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        int idx = (c->stderr_head + c->stderr_count - count + i) % MAX_STDERR;
        if (i > 0) strcat(result, "\n");
        strcat(result, c->stderr_lines[idx]);
    }
    pthread_mutex_unlock(&c->stderr_mutex);
    return result;
}

const char *codex_client_last_error(codex_client_t *c) {
    return c ? c->last_error : "null client";
}
