/*
 * control_socket.c — C11 port of gateway/control_socket.py
 *
 * Gateway-owned control socket: POSIX unix-domain transport, v1 wire
 * contract (one JSON request line per connection, one JSON response line,
 * server closes). All failures are non-fatal; consumers fall back to the
 * scan layer (gateway_state.json / process scans).
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "control_socket.h"
#include "gateway_status.h"
#include "hermes_crypto.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#define CONTROL_SOCKET_FILENAME "gateway.sock"
#define CONTROL_SOCKET_POINTER_FILENAME "gateway.sock.path"

static char *cs_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static char *cs_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

static double cs_now_monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}


/* PoP: control_socket_home_hash @gateway/control_socket.py:_home_hash */
char *control_socket_home_hash(const char *home)
{
    if (!home || !home[0]) return NULL;
    char resolved[4096];
    if (realpath(home, resolved) == NULL) {
        /* resolve(strict=False): keep the given path when resolution fails. */
        snprintf(resolved, sizeof(resolved), "%s", home);
    }
    unsigned char digest[32];
    crypto_sha256((const unsigned char *)resolved, strlen(resolved), digest);
    char *full = crypto_hex_encode(digest, 32);
    if (!full) return NULL;
    char *out = realloc(full, 17);
    if (!out) { free(full); return NULL; }
    out[16] = '\0';
    return out;
}

/* PoP: windows_pipe_name @gateway/control_socket.py:windows_pipe_name */
char *control_socket_windows_pipe_name(const char *home)
{
    char *h = control_socket_home_hash(home);
    if (!h) return NULL;
    char *out = cs_fmt("\\\\.\\pipe\\hermes-gateway-%s", h);
    free(h);
    return out;
}

/* PoP: _pointer_path @gateway/control_socket.py:_pointer_path */
char *control_socket_pointer_path(const char *home)
{
    if (!home) return NULL;
    return cs_fmt("%s/%s", home, CONTROL_SOCKET_POINTER_FILENAME);
}

/* PoP: _default_socket_path @gateway/control_socket.py:_default_socket_path */
char *control_socket_default_socket_path(const char *home)
{
    if (!home) return NULL;
    return cs_fmt("%s/%s", home, CONTROL_SOCKET_FILENAME);
}

/* PoP: _fallback_socket_path @gateway/control_socket.py:_fallback_socket_path */
char *control_socket_fallback_socket_path(const char *home)
{
    char *h = control_socket_home_hash(home);
    if (!h) return NULL;
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    char *cand = cs_fmt("%s/hermes-gw-%s.sock", tmp, h);
    if (cand && strlen(cand) <= (size_t)CONTROL_SOCKET_MAX_UNIX_PATH) {
        free(h);
        return cand;
    }
    free(cand);
    cand = cs_fmt("/tmp/hermes-gw-%s.sock", h);
    free(h);
    return cand; /* returned even when too long — bind fails non-fatally */
}

/* PoP: resolve_server_socket_path @gateway/control_socket.py:resolve_server_socket_path */
char *control_socket_resolve_server_socket_path(const char *home,
                                                char **out_pointer_file)
{
    if (out_pointer_file) *out_pointer_file = NULL;
    char *direct = control_socket_default_socket_path(home);
    if (!direct) return NULL;
    if (strlen(direct) <= (size_t)CONTROL_SOCKET_MAX_UNIX_PATH)
        return direct;
    free(direct);
    char *fallback = control_socket_fallback_socket_path(home);
    char *pointer = control_socket_pointer_path(home);
    if (out_pointer_file) *out_pointer_file = pointer;
    else free(pointer);
    return fallback;
}

/* PoP: resolve_client_socket_path @gateway/control_socket.py:resolve_client_socket_path */
char *control_socket_resolve_client_socket_path(const char *home)
{
    char *direct = control_socket_default_socket_path(home);
    if (direct && access(direct, F_OK) == 0)
        return direct;
    free(direct);
    char *pointer = control_socket_pointer_path(home);
    if (!pointer) return NULL;
    FILE *fp = fopen(pointer, "r");
    free(pointer);
    if (!fp) return NULL;
    char target[4096] = {0};
    if (!fgets(target, sizeof(target), fp)) { fclose(fp); return NULL; }
    fclose(fp);
    size_t n = strlen(target);
    while (n && (target[n - 1] == '\n' || target[n - 1] == '\r' ||
                 target[n - 1] == ' ' || target[n - 1] == '\t'))
        target[--n] = '\0';
    if (n == 0) return NULL;
    struct stat st;
    if (stat(target, &st) != 0) return NULL;
    return cs_strdup(target);
}

/* argv mirror for supervisor detection (set by main()); defaults to empty. */
int g_argc_shared = 0;
char **g_argv_shared = NULL;

void control_socket_set_process_argv(int argc, char **argv)
{
    g_argc_shared = argc;
    g_argv_shared = argv;
}

/* PoP: _detect_supervisor @gateway/control_socket.py:_detect_supervisor */
const char *control_socket_detect_supervisor(void)
{
    const char *invocation = getenv("INVOCATION_ID");
    if (invocation && invocation[0]) return "systemd";
#if __APPLE__
    const char *xpc = getenv("XPC_SERVICE_NAME");
    if ((xpc && strncmp(xpc, "ai.hermes", 9) == 0) ||
        getenv("LAUNCHD_SOCKET"))
        return "launchd";
#endif
    const char *desktop = getenv("HERMES_DESKTOP_MANAGED");
    if (desktop && desktop[0]) return "desktop";
    /* "--external-supervisor" in sys.argv */
    for (int i = 0; i < g_argc_shared; i++) {
        if (strcmp(g_argv_shared[i], "--external-supervisor") == 0)
            return "external";
    }
    return "manual";
}

/* argv mirror for supervisor detection (set by main()); defaults to empty. */
/* Code identity fields stamped by the runtime status writer (#91283).
 * The C tree carries them inside gateway_state.json; read them from there
 * (best-effort — a missing stamp yields absent keys). */
static void cs_add_code_identity_fields(json_t *payload, json_t *runtime)
{
    if (!payload) return;
    if (runtime) {
        json_t *sha = json_obj_get(runtime, "code_sha");
        if (sha && sha->type == JSON_STRING && sha->str_val && sha->str_val[0])
            json_set(payload, "code_sha", json_string(sha->str_val));
        json_t *ver = json_obj_get(runtime, "code_version");
        if (ver && ver->type == JSON_STRING && ver->str_val && ver->str_val[0])
            json_set(payload, "code_version", json_string(ver->str_val));
    }
}

/* PoP: build_identify_payload @gateway/control_socket.py:build_identify_payload */
json_t *control_socket_build_identify_payload(void)
{
    json_t *payload = json_object();
    if (!payload) return NULL;
    json_set(payload, "protocol", json_int(CONTROL_PROTOCOL_VERSION));
    json_set(payload, "kind", json_string("gateway"));
    json_set(payload, "pid", json_int((int)getpid()));
    long start_ticks = gwstatus_get_process_start_time(getpid());
    if (start_ticks > 0)
        json_set(payload, "start_time", json_number((double)start_ticks));
    char home[1024];
    extern void hermes_get_home(char *, size_t);
    hermes_get_home(home, sizeof(home));
    json_set(payload, "hermes_home", json_string(home));
    char *label = _profile_label_for_home(home, NULL);
    if (label) {
        json_set(payload, "profile", json_string(label));
        free(label);
    } else {
        json_set(payload, "profile", json_string("default"));
    }
    json_set(payload, "supervisor", json_string(control_socket_detect_supervisor()));

    json_t *runtime = NULL;
    char *status_path = cs_fmt("%s/gateway_state.json", home);
    if (status_path) {
        char *raw = gwstatus_read_runtime_status(status_path);
        if (raw) {
            runtime = json_parse(raw, NULL);
            free(raw);
        }
        free(status_path);
    }
    cs_add_code_identity_fields(payload, runtime);
    if (runtime) {
        json_t *served = json_obj_get(runtime, "served_profiles");
        if (served && served->type == JSON_ARRAY && json_len(served) > 0) {
            json_t *copy = json_copy(served);
            if (copy) json_set(payload, "served_profiles", copy);
        }
        json_free(runtime);
    }
    return payload;
}

/* PoP: build_status_payload @gateway/control_socket.py:build_status_payload */
json_t *control_socket_build_status_payload(void)
{
    json_t *payload = NULL;
    char home[1024];
    extern void hermes_get_home(char *, size_t);
    hermes_get_home(home, sizeof(home));
    char *status_path = cs_fmt("%s/gateway_state.json", home);
    if (status_path) {
        char *raw = gwstatus_read_runtime_status(status_path);
        if (raw) {
            payload = json_parse(raw, NULL);
            free(raw);
        }
        free(status_path);
    }
    if (!payload) payload = json_object();
    if (!payload) return NULL;
    json_set(payload, "protocol", json_int(CONTROL_PROTOCOL_VERSION));
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    json_set(payload, "answered_at",
             json_number((double)ts.tv_sec + (double)ts.tv_nsec / 1e9));
    json_set(payload, "answering_pid", json_int((int)getpid()));
    return payload;
}

/* ------------------------------------------------------------------ */
/* Server                                                              */
/* ------------------------------------------------------------------ */

struct control_socket_server {
    char *home;
    control_socket_handlers_t handlers;
    char *bind_path;
    char *pointer_file;
    volatile int running;
    int listen_fd;
    pthread_t thread;
    int thread_started;
};

static json_t *cs_dispatch_verb(control_socket_server_t *srv, const char *verb)
{
    control_socket_verb_fn fn = NULL;
    if (strcmp(verb, "identify") == 0)
        fn = srv->handlers.identify ? srv->handlers.identify
                                    : (control_socket_verb_fn)control_socket_build_identify_payload;
    else if (strcmp(verb, "status") == 0)
        fn = srv->handlers.status ? srv->handlers.status
                                  : (control_socket_verb_fn)control_socket_build_status_payload;
    if (!fn) return NULL;
    return fn(srv->handlers.ctx);
}

/* Sorted copy of the supported verbs for error envelopes. */
static void cs_append_supported_verbs(json_t *response)
{
    static const char *verbs[] = {"identify", "status"};
    json_t *arr = json_array();
    if (!arr) return;
    json_append(arr, json_string(verbs[0]));
    json_append(arr, json_string(verbs[1]));
    json_set(response, "supported_verbs", arr);
}

/* PoP: handle_request_line @gateway/control_socket.py:handle_request_line */
char *control_socket_handle_request_line(control_socket_server_t *srv,
                                         const char *raw, size_t raw_len)
{
    json_t *response = NULL;
    json_t *request_id = NULL;
    int have_id = 0;
    char *request_id_json = NULL;

    do {
        char *text = malloc(raw_len + 1);
        if (!text) break;
        memcpy(text, raw, raw_len);
        text[raw_len] = '\0';
        json_t *request = json_parse(text, NULL);
        free(text);
        if (!request || request->type != JSON_OBJECT) {
            if (request) json_free(request);
            response = json_object();
            if (!response) break;
            json_set(response, "ok", json_bool(false));
            json_set(response, "error",
                     json_string("ValueError: request must be a JSON object"));
            json_set(response, "protocol", json_int(CONTROL_PROTOCOL_VERSION));
            break;
        }
        json_t *id_node = json_obj_get(request, "id");
        if (id_node) {
            request_id = json_copy(id_node);
            have_id = 1;
        }
        json_t *verb_node = json_obj_get(request, "verb");
        const char *verb = (verb_node && verb_node->type == JSON_STRING)
                               ? verb_node->str_val : NULL;
        if (!verb) {
            response = json_object();
            json_set(response, "ok", json_bool(false));
            json_set(response, "error", json_string("KeyError: 'verb'"));
            json_set(response, "protocol", json_int(CONTROL_PROTOCOL_VERSION));
        } else {
            json_t *result = cs_dispatch_verb(srv, verb);
            if (!result) {
                response = json_object();
                json_set(response, "ok", json_bool(false));
                char *msg = cs_fmt("unknown verb: '%s'", verb);
                json_set(response, "error",
                         json_string(msg ? msg : "unknown verb"));
                free(msg);
                json_set(response, "protocol", json_int(CONTROL_PROTOCOL_VERSION));
                cs_append_supported_verbs(response);
            } else {
                response = json_object();
                json_set(response, "ok", json_bool(true));
                json_set(response, "protocol", json_int(CONTROL_PROTOCOL_VERSION));
                json_set(response, "result", result);
            }
        }
        json_free(request);
    } while (0);

    if (have_id && request_id && response)
        json_set(response, "id", request_id);
    else if (request_id)
        json_free(request_id);

    char *encoded = response ? json_serialize(response) : NULL;
    if (response) json_free(response);
    if (!encoded) {
        static const char fallback[] =
            "{\"ok\": false, \"error\": \"response serialization failed\"}";
        encoded = cs_strdup(fallback);
        if (!encoded) return NULL;
    }
    if (strlen(encoded) > (size_t)CONTROL_SOCKET_MAX_RESPONSE_BYTES) {
        free(encoded);
        encoded = cs_strdup("{\"ok\": false, \"error\": \"response too large\"}");
        if (!encoded) return NULL;
    }
    char *line = cs_fmt("%s\n", encoded ? encoded : "");
    free(encoded);
    return line;
}

/* Also carries the asyncio.Protocol.data_received body: buffer the single
 * bounded request line, dispatch, answer, close. */
/* PoP: cs_serve_connection @gateway/control_socket.py:_handle_connection */
/* PoP: cs_serve_connection @gateway/control_socket.py:data_received */
static void cs_serve_connection(control_socket_server_t *srv, int fd)
{
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char buf[CONTROL_SOCKET_MAX_REQUEST_BYTES + 1];
    size_t total = 0;
    ssize_t got;
    while (total < (size_t)CONTROL_SOCKET_MAX_REQUEST_BYTES &&
           (got = read(fd, buf + total, sizeof(buf) - 1 - total)) > 0) {
        total += (size_t)got;
        if (memchr(buf, '\n', total)) break;
    }
    if (total == 0 || total > (size_t)CONTROL_SOCKET_MAX_REQUEST_BYTES) {
        close(fd);
        return;
    }
    char *response = control_socket_handle_request_line(srv, buf, total);
    if (response) {
        size_t len = strlen(response);
        size_t off = 0;
        while (off < len) {
            ssize_t w = write(fd, response + off, len - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
        free(response);
    }
    close(fd);
}

/* Accept side of the protocol lifecycle: each accepted fd is the transport
 * handed to the per-connection handler. */
/* PoP: cs_accept_loop @gateway/control_socket.py:connection_made */
static void *cs_accept_loop(void *arg)
{
    control_socket_server_t *srv = arg;
    while (srv->running) {
        int fd = accept(srv->listen_fd, NULL, NULL);
        if (fd < 0) {
            if (!srv->running) break;
            continue;
        }
        cs_serve_connection(srv, fd);
    }
    return NULL;
}

/* PoP: GatewayControlServer.__init__ @ gateway/control_socket.py:GatewayControlServer.__init__ */
control_socket_server_t *control_socket_server_new(
    const char *home, const control_socket_handlers_t *handlers)
{
    if (!home || !home[0]) return NULL;
    control_socket_server_t *srv = calloc(1, sizeof(*srv));
    if (!srv) return NULL;
    srv->home = cs_strdup(home);
    srv->listen_fd = -1;
    if (handlers) srv->handlers = *handlers;
    if (!srv->home) { free(srv); return NULL; }
    return srv;
}

/* PoP: _start_posix @gateway/control_socket.py:_start_posix */
static int cs_start_posix(control_socket_server_t *srv)
{
    char *pointer_file = NULL;
    char *bind_path =
        control_socket_resolve_server_socket_path(srv->home, &pointer_file);
    if (!bind_path) return 0;
    /* Clear a stale socket left by a crashed predecessor. */
    unlink(bind_path);
    mode_t old_umask = umask(0177);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        umask(old_umask);
        free(bind_path);
        free(pointer_file);
        return 0;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, bind_path, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(fd, 8) != 0) {
        umask(old_umask);
        close(fd);
        free(bind_path);
        free(pointer_file);
        return 0;
    }
    umask(old_umask);
    chmod(bind_path, 0600);
    srv->listen_fd = fd;
    srv->bind_path = bind_path;
    srv->running = 1;
    if (pointer_file) {
        FILE *fp = fopen(pointer_file, "w");
        if (fp) {
            fprintf(fp, "%s", bind_path);
            fclose(fp);
        }
        srv->pointer_file = pointer_file;
    } else {
        free(pointer_file);
    }
    if (pthread_create(&srv->thread, NULL, cs_accept_loop, srv) != 0) {
        srv->running = 0;
        return 0;
    }
    srv->thread_started = 1;
    return 1;
}

/* PoP: start @gateway/control_socket.py:start */
/* The proactor named-pipe transport exists only on win32; POSIX builds
 * faithfully report "not started" and consumers use the scan layer. */
/* PoP: cs_start_windows @gateway/control_socket.py:_start_windows */
static int cs_start_windows(control_socket_server_t *srv)
{
    (void)srv;
    return 0;
}

/* PoP: control_socket_server_start @gateway/control_socket.py:start */
int control_socket_server_start(control_socket_server_t *srv)
{
    if (!srv) return 0;
#ifdef _WIN32
    return cs_start_windows(srv);
#else
    return cs_start_posix(srv);
#endif
}

/* PoP: cleanup_files @gateway/control_socket.py:cleanup_files */
void control_socket_server_cleanup_files(control_socket_server_t *srv)
{
    if (!srv) return;
    if (srv->bind_path) unlink(srv->bind_path);
    if (srv->pointer_file) unlink(srv->pointer_file);
}

/* PoP: stop @gateway/control_socket.py:stop */
void control_socket_server_stop(control_socket_server_t *srv)
{
    if (!srv) return;
    srv->running = 0;
    if (srv->listen_fd >= 0) {
        shutdown(srv->listen_fd, SHUT_RDWR);
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    if (srv->thread_started) {
        pthread_join(srv->thread, NULL);
        srv->thread_started = 0;
    }
    control_socket_server_cleanup_files(srv);
}

void control_socket_server_free(control_socket_server_t *srv)
{
    if (!srv) return;
    control_socket_server_stop(srv);
    free(srv->home);
    free(srv->bind_path);
    free(srv->pointer_file);
    free(srv);
}

/* ------------------------------------------------------------------ */
/* Client (synchronous)                                                */
/* ------------------------------------------------------------------ */

/* PoP: cs_query_unix_socket @gateway/control_socket.py:_query_unix_socket */
static char *cs_query_unix_socket(const char *home, const char *request,
                                  double timeout)
{
    char *path = control_socket_resolve_client_socket_path(home);
    if (!path) return NULL;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { free(path); return NULL; }
    struct timeval tv = {.tv_sec = (long)timeout,
                         .tv_usec = (long)((timeout - (double)(long)timeout) * 1e6)};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    free(path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return NULL;
    }
    size_t req_len = strlen(request), off = 0;
    while (off < req_len) {
        ssize_t w = write(fd, request + off, req_len - off);
        if (w <= 0) { close(fd); return NULL; }
        off += (size_t)w;
    }

    double deadline = cs_now_monotonic() + timeout;
    size_t cap = CONTROL_SOCKET_MAX_RESPONSE_BYTES + 64;
    char *acc = malloc(cap);
    if (!acc) { close(fd); return NULL; }
    size_t total = 0;
    int saw_newline = 0;
    while (total < cap) {
        if (cs_now_monotonic() >= deadline) break;
        ssize_t r = read(fd, acc + total, cap - total);
        if (r <= 0) break;
        total += (size_t)r;
        if (memchr(acc + total - (size_t)(r < 65536 ? r : 65536), '\n',
                   (size_t)r))
            { saw_newline = 1; break; }
    }
    close(fd);
    if (total == 0) { free(acc); return NULL; }
    (void)saw_newline;
    char *nl = memchr(acc, '\n', total);
    size_t line_len = nl ? (size_t)(nl - acc) : total;
    acc[line_len] = '\0';
    return acc; /* malloc'd response line (no trailing newline) */
}

/* Named-pipe client: full transport on win32; POSIX builds faithfully
 * answer NULL so callers fall back to the scan layer. */
/* PoP: cs_query_windows_pipe @gateway/control_socket.py:_query_windows_pipe */
static char *cs_query_windows_pipe(const char *home, const char *request,
                                   double timeout)
{
#ifdef _WIN32
    char *pipe_name = control_socket_windows_pipe_name(home);
    if (!pipe_name) return NULL;
    /* Convert \\.\pipe\name → \\.\pipe\name (CreateFile opens it directly). */
    HANDLE handle = INVALID_HANDLE_VALUE;
    double deadline = cs_now_monotonic() + timeout;
    while (handle == INVALID_HANDLE_VALUE) {
        handle = CreateFileA(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                             OPEN_EXISTING, 0, NULL);
        if (handle != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_PIPE_BUSY) { free(pipe_name); return NULL; }
        if (cs_now_monotonic() >= deadline) { free(pipe_name); return NULL; }
        Sleep(50); /* pipe busy: brief retry window */
    }
    free(pipe_name);
    DWORD written = 0;
    if (!WriteFile(handle, request, (DWORD)strlen(request), &written, NULL)) {
        CloseHandle(handle);
        return NULL;
    }
    size_t cap = CONTROL_SOCKET_MAX_RESPONSE_BYTES + 64;
    char *acc = malloc(cap);
    if (!acc) { CloseHandle(handle); return NULL; }
    size_t total = 0;
    double deadline2 = cs_now_monotonic() + timeout;
    while (total < cap && cs_now_monotonic() < deadline2) {
        DWORD got = 0;
        if (!ReadFile(handle, acc + total, (DWORD)(cap - total), &got, NULL)
            || got == 0)
            break;
        total += got;
        if (memchr(acc + total - got, '\n', got)) break;
    }
    CloseHandle(handle);
    if (total == 0) { free(acc); return NULL; }
    char *nl = memchr(acc, '\n', total);
    size_t line_len = nl ? (size_t)(nl - acc) : total;
    acc[line_len] = '\0';
    return acc;
#else
    (void)home; (void)request; (void)timeout;
    return NULL; /* no named pipes on POSIX — scan-layer fallback */
#endif
}

/* PoP: control_socket_query_gateway_control @gateway/control_socket.py:query_gateway_control */
json_t *control_socket_query_gateway_control(const char *home, const char *verb,
                                             double timeout)
{
    if (!home || !verb || timeout <= 0) return NULL;
    char *req = cs_fmt("{\"verb\": \"%s\", \"id\": 1, \"protocol\": %d}\n",
                       verb, CONTROL_PROTOCOL_VERSION);
    if (!req) return NULL;
#ifdef _WIN32
    char *raw_line = cs_query_windows_pipe(home, req, timeout);
#else
    char *raw_line = cs_query_unix_socket(home, req, timeout);
#endif
    free(req);
    if (!raw_line) return NULL;
    json_t *response = json_parse(raw_line, NULL);
    free(raw_line);
    if (!response || response->type != JSON_OBJECT) {
        if (response) json_free(response);
        return NULL;
    }
    json_t *ok = json_obj_get(response, "ok");
    if (!ok || ok->type != JSON_BOOL || !ok->bool_val) {
        json_free(response);
        return NULL;
    }
    json_t *result = json_obj_get(response, "result");
    if (!result || result->type != JSON_OBJECT) {
        json_free(response);
        return NULL;
    }
    json_t *out = json_copy(result);
    json_free(response);
    return out;
}

/* PoP: identify_gateway @gateway/control_socket.py:identify_gateway */
json_t *control_socket_identify_gateway(const char *home, double timeout)
{
    return control_socket_query_gateway_control(
        home, "identify",
        timeout > 0 ? timeout : CONTROL_SOCKET_DEFAULT_CLIENT_TIMEOUT);
}
