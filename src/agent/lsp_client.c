/*
 * lsp_client.c — port of agent/lsp/client.py.
 *
 * One lsp_client_t == one (language_server, workspace_root) pair. Owns a
 * child process (fork/execvp, pipes), drives the JSON-RPC exchange on a
 * dedicated reader thread, and exposes document sync + diagnostics with
 * version-based freshness (no timestamps, no clock races).
 *
 * Python async -> C: reader loop is a pthread; per-request futures are a
 * pending table keyed by request id; the push wakeup is a condvar +
 * monotonic counter.
 *
 */
#define _POSIX_C_SOURCE 200809L
#include "lsp_common.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>

extern char **environ;

#define LSP_INITIALIZE_TIMEOUT_MS 45000
#define LSP_SHUTDOWN_GRACE_MS 1000
#define LSP_CONTENT_MODIFIED_MAX_RETRIES 3

/* ── document state + pending + client struct ───────────────────────── */
typedef struct {
    int id;
    bool done;
    bool is_error;
    char *result_json;    /* owned (result or error body) */
    int err_code;
    char *err_message;    /* owned */
    char *err_data;       /* owned, may be NULL */
} pending_t;

typedef struct {
    char *path;           /* owned */
    int version;
    char *text;
    char **push;
    size_t push_n, push_cap;
    int push_version;
    int pull_version;
    bool seed_seen;
} doc_state2_t;

struct lsp_client {
    char *server_id;
    char *workspace_root;
    char **command;       /* NULL-terminated argv (owned) */
    char **env;           /* NULL-terminated "K=V" (owned, optional) */
    char *cwd;
    char *init_options;   /* owned JSON (optional) */

    pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;

    pthread_t reader_thread;
    pthread_t stderr_thread;
    bool threads_started;

    int next_id;
    pending_t *pending;   /* dynamic array */
    size_t pending_n, pending_cap;
    pthread_mutex_t pend_lock;

    doc_state2_t *docs;   /* dynamic array keyed by path */
    size_t docs_n, docs_cap;
    pthread_mutex_t doc_lock;

    unsigned long push_counter;
    pthread_cond_t push_cond;
    pthread_mutex_t push_lock;

    char *state;          /* owned string */
    char *init_result;    /* owned JSON (optional) */
    int sync_kind;
    bool stopping;

    lsp_server_request_handler req_handler;
    void *req_user;
    lsp_notification_handler notif_handlers[8];
    char *notif_methods[8];
    size_t notif_n;
    pthread_mutex_t notif_lock;
};

/* ── helpers ────────────────────────────────────────────────────────── */
static char *xstrdup_lsp(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: file_uri @ agent/lsp/client.py:file_uri */
static char *file_uri(const char *path)
{
    char abs[4096];
    if (path[0] != '/') {
        char cwd[4096]; if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
        snprintf(abs, sizeof(abs), "%s/%s", cwd, path);
    } else {
        strncpy(abs, path, sizeof(abs) - 1); abs[sizeof(abs)-1] = '\0';
    }
    size_t need = 7 + strlen(abs) * 3 + 1;
    char *out = malloc(need);
    size_t o = 0;
    memcpy(out, "file://", 7); o = 7;
    for (const char *p = abs; *p; p++) {
        if (*p == ' ') { strcpy(out + o, "%20"); o += 3; }
        else out[o++] = *p;
    }
    out[o] = '\0';
    return out;
}

static pending_t *pending_get(lsp_client_t *c, int id)
{
    for (size_t i = 0; i < c->pending_n; i++)
        if (c->pending[i].id == id) return &c->pending[i];
    return NULL;
}

static void pending_free(pending_t *p)
{
    free(p->result_json);
    free(p->err_message);
    free(p->err_data);
    memset(p, 0, sizeof(*p));
}

static doc_state2_t *doc_find(lsp_client_t *c, const char *path)
{
    for (size_t i = 0; i < c->docs_n; i++)
        if (strcmp(c->docs[i].path, path) == 0) return &c->docs[i];
    return NULL;
}

static doc_state2_t *doc_create(lsp_client_t *c, const char *path)
{
    if (c->docs_n == c->docs_cap) {
        c->docs_cap = c->docs_cap ? c->docs_cap * 2 : 8;
        c->docs = realloc(c->docs, c->docs_cap * sizeof(*c->docs));
    }
    doc_state2_t *d = &c->docs[c->docs_n++];
    memset(d, 0, sizeof(*d));
    d->path = xstrdup_lsp(path);
    d->version = 0;
    d->push_version = -1;
    d->pull_version = -1;
    return d;
}

/* ── low-level write ────────────────────────────────────────────────── */
static int send_raw(lsp_client_t *c, const char *json)
{
    char *framed = lsp_encode_message(json);
    if (!framed) return -1;
    size_t len = strlen(framed);
    size_t wrote = 0;
    while (wrote < len) {
        ssize_t w = write(c->stdin_fd, framed + wrote, len - wrote);
        if (w < 0) { if (errno == EINTR) continue; free(framed); return -1; }
        wrote += (size_t)w;
    }
    free(framed);
    return 0;
}

/* ── stderr drain ───────────────────────────────────────────────────── */
/* PoP: stderr_thread_fn @ agent/lsp/client.py:_drain_stderr */
static void *stderr_thread_fn(void *arg)
{
    lsp_client_t *c = arg;
    char buf[4096];
    ssize_t r;
    while ((r = read(c->stderr_fd, buf, sizeof(buf) - 1)) > 0) {
        buf[r] = '\0'; /* dropped (debug only) */
    }
    return NULL;
}

/* ── reader loop ───────────────────────────────────────────────────── */
static void dispatch_response(lsp_client_t *c, int id, const char *json);
static void dispatch_notification(lsp_client_t *c, const char *method, const char *json);

/* PoP: reader_thread_fn @ agent/lsp/client.py:_reader_loop */
static void *reader_thread_fn(void *arg)
{
    lsp_client_t *c = arg;
    for (;;) {
        lsp_protocol_error_t perr;
        char *msg = lsp_read_message(c->stdout_fd, &perr);
        if (!msg) {
            if (perr.message) free(perr.message);
            break; /* EOF or fatal framing error */
        }
        int id = 0;
        char *key = NULL;
        lsp_msg_kind_t kind = lsp_classify_message(msg, &key, &id);
        if (kind == LSP_MSG_RESPONSE) {
            dispatch_response(c, id, msg);
        } else if (kind == LSP_MSG_REQUEST) {
            /* server → client request: invoke handler, send response */
            char *params = NULL;
            json_t *m = json_parse(msg, NULL);
            if (m) { json_t *p = json_obj_get(m, "params"); if (p) params = json_dumps(p, 0); json_free(m); }
            int err_code = LSP_ERR_METHOD_NOT_FOUND;
            char *err_msg = xstrdup_lsp("method not found");
            char *err_data = NULL;
            char *result = NULL;
            if (c->req_handler && key) {
                free(err_msg); err_msg = NULL;
                result = c->req_handler(key, params, &err_code, &err_msg, c->req_user);
            }
            char *resp;
            if (result) resp = lsp_make_response(id, result);
            else resp = lsp_make_error_response(id, err_code, err_msg ? err_msg : "error", err_data);
            send_raw(c, resp);
            free(resp); free(result); free(err_msg); free(err_data); free(params); free(key);
        } else if (kind == LSP_MSG_NOTIFICATION) {
            dispatch_notification(c, key ? key : "", msg);
            free(key);
        } else {
            free(key);
        }
        free(msg);
    }
    /* wake pending so they fail fast */
    pthread_mutex_lock(&c->pend_lock);
    for (size_t i = 0; i < c->pending_n; i++) {
        pending_t *p = &c->pending[i];
        if (!p->done) { p->done = true; p->is_error = true; p->err_code = -1; p->err_message = xstrdup_lsp("server connection closed"); }
    }
    pthread_mutex_unlock(&c->pend_lock);
    return NULL;
}

/* PoP: dispatch_response @ agent/lsp/client.py:_dispatch_response */
static void dispatch_response(lsp_client_t *c, int id, const char *json)
{
    pthread_mutex_lock(&c->pend_lock);
    pending_t *p = pending_get(c, id);
    if (p) {
        p->done = true;
        json_t *m = json_parse(json, NULL);
        if (m) {
            json_t *err = json_obj_get(m, "error");
            if (err) {
                p->is_error = true;
                json_t *code = json_obj_get(err, "code"); if (code && json_is_number(code)) p->err_code = (int)json_number_value(code);
                json_t *msg = json_obj_get(err, "message"); if (msg && json_is_string(msg)) p->err_message = xstrdup_lsp(json_string_value(msg));
                json_t *data = json_obj_get(err, "data"); if (data) p->err_data = json_dumps(data, 0);
            } else {
                json_t *res = json_obj_get(m, "result");
                p->result_json = res ? json_dumps(res, 0) : xstrdup_lsp("null");
            }
            json_free(m);
        }
    }
    pthread_mutex_unlock(&c->pend_lock);
}

/* PoP: dispatch_notification @ agent/lsp/client.py:_dispatch_notification */
static void dispatch_notification(lsp_client_t *c, const char *method, const char *json)
{
    /* textDocument/publishDiagnostics -> update doc push store */
    if (strcmp(method, "textDocument/publishDiagnostics") == 0) {
        json_t *m = json_parse(json, NULL);
        if (m) {
            json_t *params = json_obj_get(m, "params");
            if (params) {
                json_t *uri = json_obj_get(params, "uri");
                json_t *diags = json_obj_get(params, "diagnostics");
                if (uri && json_is_string(uri) && diags && json_is_array(diags)) {
                    /* uri -> path: strip file:// */
                    const char *u = json_string_value(uri);
                    const char *path = u;
                    if (strncmp(u, "file://", 7) == 0) path = u + 7;
                    /* un-percent-encode spaces minimally */
                    char dec[8192]; size_t di = 0;
                    for (const char *p = path; *p && di < sizeof(dec)-1; ) {
                        if (*p == '%' && p[1] && p[2]) {
                            int v = 0; sscanf(p+1, "%2x", &v); dec[di++] = (char)v; p += 3;
                        } else dec[di++] = *p++;
                    }
                    dec[di] = '\0';
                    pthread_mutex_lock(&c->doc_lock);
                    doc_state2_t *d = doc_find(c, dec);
                    if (d) {
                        for (size_t i = 0; i < d->push_n; i++) free(d->push[i]);
                        d->push_n = 0;
                        size_t n = json_len(diags);
                        for (size_t i = 0; i < n; i++) {
                            json_t *dg = json_get(diags, i);
                            if (!dg) continue;
                            char *s = json_dumps(dg, 0);
                            if (d->push_n == d->push_cap) { d->push_cap = d->push_cap?d->push_cap*2:8; d->push = realloc(d->push, d->push_cap*sizeof(char*)); }
                            d->push[d->push_n++] = s;
                        }
                        d->push_version = d->version;
                        d->seed_seen = true;
                    }
                    pthread_mutex_unlock(&c->doc_lock);
                    /* wake waiters */
                    pthread_mutex_lock(&c->push_lock);
                    c->push_counter++;
                    pthread_cond_broadcast(&c->push_cond);
                    pthread_mutex_unlock(&c->push_lock);
                }
            }
            json_free(m);
        }
        return;
    }
    /* other notifications: invoke registered handler if any */
    pthread_mutex_lock(&c->notif_lock);
    for (size_t i = 0; i < c->notif_n; i++) {
        if (strcmp(c->notif_methods[i], method) == 0) {
            c->notif_handlers[i](method, json, c->req_user);
            break;
        }
    }
    pthread_mutex_unlock(&c->notif_lock);
}

/* ── request send (blocking with timeout) ───────────────────────────── */
/* PoP: send_request_blocking @ agent/lsp/client.py:_send_request */
static char *send_request_blocking(lsp_client_t *c, const char *method,
                                   const char *params_json, int timeout_ms, int *err_code)
{
    int id;
    pthread_mutex_lock(&c->pend_lock);
    id = c->next_id++;
    if (c->pending_n == c->pending_cap) { c->pending_cap = c->pending_cap?c->pending_cap*2:16; c->pending = realloc(c->pending, c->pending_cap*sizeof(pending_t)); }
    pending_t *p = &c->pending[c->pending_n++];
    memset(p, 0, sizeof(*p)); p->id = id;
    pthread_mutex_unlock(&c->pend_lock);

    char *req = lsp_make_request(id, method, params_json);
    send_raw(c, req);
    free(req);

    /* wait for completion (use a condvar-less poll; acceptable for LSP) */
    /* We piggyback on push_cond timing but really need a per-request cond.
     * Simpler: spin with usleep bounded by timeout. */
    struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 5 * 1000 * 1000; /* 5ms */
    int waited = 0;
    while (waited < timeout_ms) {
        pthread_mutex_lock(&c->pend_lock);
        bool done = p->done;
        pthread_mutex_unlock(&c->pend_lock);
        if (done) break;
        nanosleep(&ts, NULL);
        waited += 5;
    }

    pthread_mutex_lock(&c->pend_lock);
    char *out = NULL;
    if (p->done) {
        if (p->is_error) { if (err_code) *err_code = p->err_code; out = xstrdup_lsp(p->err_message ? p->err_message : "request error"); }
        else out = xstrdup_lsp(p->result_json ? p->result_json : "null");
    } else {
        if (err_code) *err_code = -2;
        out = xstrdup_lsp("timeout");
    }
    pending_free(p);
    /* remove from pending array */
    for (size_t i = 0; i < c->pending_n; i++) if (c->pending[i].id == id) { c->pending[i] = c->pending[c->pending_n-1]; c->pending_n--; break; }
    pthread_mutex_unlock(&c->pend_lock);
    return out;
}

/* ── spawn ──────────────────────────────────────────────────────────── */
/* PoP: spawn_server @ agent/lsp/client.py:_spawn */
static int spawn_server(lsp_client_t *c, char **err_out)
{
    int in_pipe[2], out_pipe[2], err_pipe[2];
    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        if (err_out) *err_out = xstrdup_lsp("pipe() failed");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) { if (err_out) *err_out = xstrdup_lsp("fork() failed"); return -1; }
    if (pid == 0) {
        /* child */
        dup2(in_pipe[0], 0);
        dup2(out_pipe[1], 1);
        dup2(err_pipe[1], 2);
        close(in_pipe[1]); close(out_pipe[0]); close(err_pipe[0]);
        close(in_pipe[0]); close(out_pipe[1]); close(err_pipe[1]);
        setsid(); /* own process group (start_new_session=True) */
        /* build env */
        char **envp = c->env ? c->env : environ;
        environ = envp;
        execvp(c->command[0], c->command);
        _exit(127);
    }
    /* parent */
    close(in_pipe[0]); close(out_pipe[1]); close(err_pipe[1]);
    c->pid = pid;
    c->stdin_fd = in_pipe[1];
    c->stdout_fd = out_pipe[0];
    c->stderr_fd = err_pipe[0];
    return 0;
}

/* ── create/destroy ─────────────────────────────────────────────────── */
/* PoP: lsp_client_create @ agent/lsp/client.py:__init__ */
lsp_client_t *lsp_client_create(const char *server_id, const char *workspace_root,
                                char **command, char **env, const char *cwd,
                                const char *init_options_json)
{
    lsp_client_t *c = calloc(1, sizeof(*c));
    c->server_id = xstrdup_lsp(server_id);
    c->workspace_root = xstrdup_lsp(workspace_root);
    /* copy command */
    size_t n = 0; while (command && command[n]) n++;
    c->command = calloc(n + 1, sizeof(char *));
    for (size_t i = 0; i < n; i++) c->command[i] = xstrdup_lsp(command[i]);
    if (env) { size_t m = 0; while (env[m]) m++; c->env = calloc(m + 1, sizeof(char *)); for (size_t i = 0; i < m; i++) c->env[i] = xstrdup_lsp(env[i]); }
    c->cwd = xstrdup_lsp(cwd ? cwd : workspace_root);
    c->init_options = xstrdup_lsp(init_options_json ? init_options_json : "{}");
    c->state = xstrdup_lsp("stopped");
    c->sync_kind = 1;
    c->next_id = 1;
    pthread_mutex_init(&c->pend_lock, NULL);
    pthread_mutex_init(&c->doc_lock, NULL);
    pthread_mutex_init(&c->push_lock, NULL);
    pthread_cond_init(&c->push_cond, NULL);
    pthread_mutex_init(&c->notif_lock, NULL);
    return c;
}

void lsp_client_destroy(lsp_client_t *c)
{
    if (!c) return;
    lsp_client_shutdown(c);
    if (c->threads_started) {
        pthread_join(c->reader_thread, NULL);
        pthread_join(c->stderr_thread, NULL);
    }
    for (size_t i = 0; i < c->docs_n; i++) {
        free(c->docs[i].path); free(c->docs[i].text);
        for (size_t j = 0; j < c->docs[i].push_n; j++) free(c->docs[i].push[j]);
        free(c->docs[i].push);
    }
    free(c->docs);
    for (size_t i = 0; i < c->pending_n; i++) pending_free(&c->pending[i]);
    free(c->pending);
    for (size_t i = 0; i < c->notif_n; i++) free(c->notif_methods[i]);
    free(c->command[0]); for (size_t i = 1; c->command[i]; i++) free(c->command[i]); free(c->command);
    if (c->env) { for (size_t i = 0; c->env[i]; i++) free(c->env[i]); free(c->env); }
    free(c->server_id); free(c->workspace_root); free(c->cwd); free(c->init_options);
    free(c->state); free(c->init_result);
    pthread_mutex_destroy(&c->pend_lock); pthread_mutex_destroy(&c->doc_lock);
    pthread_mutex_destroy(&c->push_lock); pthread_cond_destroy(&c->push_cond);
    pthread_mutex_destroy(&c->notif_lock);
    free(c);
}

/* ── lifecycle ──────────────────────────────────────────────────────── */
/* PoP: lsp_client_start @ agent/lsp/client.py:start */
int lsp_client_start(lsp_client_t *c, char **err_out)
{
    if (strcmp(c->state, "running") == 0 || strcmp(c->state, "starting") == 0) return 0;
    free(c->state); c->state = xstrdup_lsp("starting");
    if (spawn_server(c, err_out) != 0) { free(c->state); c->state = xstrdup_lsp("error"); return -1; }

    /* stderr drain + reader threads */
    if (pthread_create(&c->stderr_thread, NULL, stderr_thread_fn, c) == 0 &&
        pthread_create(&c->reader_thread, NULL, reader_thread_fn, c) == 0) {
        c->threads_started = true;
    }

    /* initialize handshake */
    char *root_uri = file_uri(c->workspace_root);
    char init_params[8192];
    snprintf(init_params, sizeof(init_params),
        "{\"rootUri\":\"%s\",\"rootPath\":\"%s\",\"processId\":%d,"
        "\"workspaceFolders\":[{\"name\":\"workspace\",\"uri\":\"%s\"}],"
        "\"initializationOptions\":%s,"
        "\"capabilities\":{"
        "\"window\":{\"workDoneProgress\":true},"
        "\"workspace\":{\"configuration\":true,\"workspaceFolders\":true,"
        "\"didChangeWatchedFiles\":{\"dynamicRegistration\":true},"
        "\"diagnostics\":{\"refreshSupport\":false}},"
        "\"textDocument\":{\"synchronization\":{\"dynamicRegistration\":false,"
        "\"didOpen\":true,\"didChange\":true,\"didSave\":true},"
        "\"diagnostic\":{\"dynamicRegistration\":true,\"relatedDocumentSupport\":true},"
        "\"publishDiagnostics\":{\"relatedInformation\":true,\"tagSupport\":{\"valueSet\":[1,2]},"
        "\"versionSupport\":true},\"hover\":{\"contentFormat\":[\"markdown\",\"plaintext\"]},"
        "\"definition\":{\"linkSupport\":true}},\"general\":{\"positionEncodings\":[\"utf-16\"]}}}",
        root_uri, c->workspace_root, (int)getpid(), root_uri, c->init_options);
    free(root_uri);
    int err_code = 0;
    char *result = send_request_blocking(c, "initialize", init_params, LSP_INITIALIZE_TIMEOUT_MS, &err_code);
    if (!result || err_code != 0) {
        if (err_out) *err_out = result ? result : xstrdup_lsp("initialize failed");
        else free(result);
        free(c->state); c->state = xstrdup_lsp("error");
        return -1;
    }
    c->init_result = result;
    /* extract sync kind */
    json_t *m = json_parse(result, NULL);
    if (m) {
        json_t *caps = json_obj_get(m, "capabilities");
        if (caps) {
            json_t *sync = json_obj_get(caps, "textDocumentSync");
            if (sync && json_is_number(sync)) c->sync_kind = (int)json_number_value(sync);
        }
        json_free(m);
    }
    /* initialized notification */
    char *initd = lsp_make_notification("initialized", "{}");
    send_raw(c, initd); free(initd);
    if (strcmp(c->init_options, "{}") != 0) {
        char cfg[4096]; snprintf(cfg, sizeof(cfg), "{\"settings\":%s}", c->init_options);
        char *dc = lsp_make_notification("workspace/didChangeConfiguration", cfg);
        send_raw(c, dc); free(dc);
    }
    free(c->state); c->state = xstrdup_lsp("running");
    return 0;
}

/* PoP: lsp_client_shutdown @ agent/lsp/client.py:shutdown */
int lsp_client_shutdown(lsp_client_t *c)
{
    if (c->stopping) return 0;
    c->stopping = true;
    if (strcmp(c->state, "running") == 0) {
        int ec = 0;
        char *r = send_request_blocking(c, "shutdown", "null", 2000, &ec);
        free(r);
        char *ex = lsp_make_notification("exit", "null");
        send_raw(c, ex); free(ex);
    }
    free(c->state); c->state = xstrdup_lsp("stopped");
    /* SIGTERM then SIGKILL after grace */
    if (c->pid > 0) {
        kill(c->pid, SIGTERM);
        struct timespec g = {0, LSP_SHUTDOWN_GRACE_MS * 1000 * 1000}; nanosleep(&g, NULL);
        kill(c->pid, SIGKILL);
        waitpid(c->pid, NULL, 0);
        c->pid = 0;
    }
    return 0;
}

/* PoP: lsp_client_is_running @ agent/lsp/client.py:is_running */
bool lsp_client_is_running(lsp_client_t *c)
{
    return strcmp(c->state, "running") == 0 && c->pid > 0;
}
/* PoP: lsp_client_state @ agent/lsp/client.py:state */
const char *lsp_client_state(lsp_client_t *c) { return c->state; }

/* ── document sync ──────────────────────────────────────────────────── */
/* PoP: lsp_client_open_file @ agent/lsp/client.py:open_file */
int lsp_client_open_file(lsp_client_t *c, const char *path, const char *text)
{
    pthread_mutex_lock(&c->doc_lock);
    doc_state2_t *d = doc_find(c, path);
    if (!d) d = doc_create(c, path);
    d->version = 0;
    free(d->text); d->text = xstrdup_lsp(text ? text : "");
    pthread_mutex_unlock(&c->doc_lock);

    char *uri = file_uri(path);
    char params[16384];
    snprintf(params, sizeof(params),
        "{\"textDocument\":{\"uri\":\"%s\",\"languageId\":\"plaintext\",\"version\":%d,\"text\":\"%s\"}}",
        uri, 0, text ? text : "");
    free(uri);
    char *n = lsp_make_notification("textDocument/didOpen", params);
    send_raw(c, n); free(n);
    /* touch-file dance: workspace/didChangeWatchedFiles */
    char wf[4096];
    snprintf(wf, sizeof(wf),
        "{\"changes\":[{\"uri\":\"%s\",\"type\":1}]}", file_uri(path));
    char *wn = lsp_make_notification("workspace/didChangeWatchedFiles", wf);
    send_raw(c, wn); free(wn);
    return 0;
}

int lsp_client_change_file(lsp_client_t *c, const char *path, const char *text)
{
    pthread_mutex_lock(&c->doc_lock);
    doc_state2_t *d = doc_find(c, path);
    if (!d) d = doc_create(c, path);
    d->version++;
    free(d->text); d->text = xstrdup_lsp(text ? text : "");
    int ver = d->version;
    pthread_mutex_unlock(&c->doc_lock);

    char *uri = file_uri(path);
    char params[16384];
    snprintf(params, sizeof(params),
        "{\"textDocument\":{\"uri\":\"%s\",\"version\":%d},"
        "\"contentChanges\":[{\"text\":\"%s\"}]}",
        uri, ver, text ? text : "");
    free(uri);
    char *n = lsp_make_notification("textDocument/didChange", params);
    send_raw(c, n); free(n);
    char wf[4096];
    snprintf(wf, sizeof(wf),
        "{\"changes\":[{\"uri\":\"%s\",\"type\":2}]}", file_uri(path));
    char *wn = lsp_make_notification("workspace/didChangeWatchedFiles", wf);
    send_raw(c, wn); free(wn);
    return ver;
}

/* ── diagnostics ────────────────────────────────────────────────────── */
/* PoP: lsp_client_diagnostics_for @ agent/lsp/client.py:diagnostics_for */
char *lsp_client_diagnostics_for(lsp_client_t *c, const char *path)
{
    pthread_mutex_lock(&c->doc_lock);
    doc_state2_t *d = doc_find(c, path);
    if (!d || d->push_version < d->version || d->push_n == 0) {
        pthread_mutex_unlock(&c->doc_lock);
        return NULL; /* not fresh */
    }
    /* build JSON array */
    size_t cap = 2;
    for (size_t i = 0; i < d->push_n; i++) cap += strlen(d->push[i]) + 2;
    char *out = malloc(cap);
    size_t o = 0; out[o++] = '[';
    for (size_t i = 0; i < d->push_n; i++) {
        if (i) out[o++] = ',';
        size_t l = strlen(d->push[i]);
        memcpy(out + o, d->push[i], l); o += l;
    }
    out[o++] = ']'; out[o] = '\0';
    pthread_mutex_unlock(&c->doc_lock);
    return out;
}

/* PoP: lsp_client_wait_for_diagnostics @ agent/lsp/client.py:wait_for_diagnostics */
int lsp_client_wait_for_diagnostics(lsp_client_t *c, const char *path,
                                    int version, int timeout_ms)
{
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (long)timeout_ms * 1000 * 1000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec += ts.tv_nsec / 1000000000; ts.tv_nsec %= 1000000000; }
    /* check freshness first */
    pthread_mutex_lock(&c->doc_lock);
    doc_state2_t *d = doc_find(c, path);
    bool fresh = d && d->push_version >= version;
    pthread_mutex_unlock(&c->doc_lock);
    if (fresh) return 0;
    pthread_mutex_lock(&c->push_lock);
    int rc = pthread_cond_timedwait(&c->push_cond, &c->push_lock, &ts);
    pthread_mutex_unlock(&c->push_lock);
    return rc == 0 ? 0 : -1;
}

/* ── handler registration ───────────────────────────────────────────── */
void lsp_client_set_request_handler(lsp_client_t *c, lsp_server_request_handler h, void *user)
{ c->req_handler = h; c->req_user = user; }

void lsp_client_set_notification_handler(lsp_client_t *c, const char *method,
                                         lsp_notification_handler h, void *user)
{
    pthread_mutex_lock(&c->notif_lock);
    if (c->notif_n < 8) {
        c->notif_methods[c->notif_n] = xstrdup_lsp(method);
        c->notif_handlers[c->notif_n] = h;
        c->notif_n++;
        c->req_user = user;
    }
    pthread_mutex_unlock(&c->notif_lock);
}
