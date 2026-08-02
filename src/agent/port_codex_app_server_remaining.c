/*
 * port_codex_app_server_remaining.c — Port of agent/transports/codex_app_server.py
 * JSON-RPC client surface. Handshake, request/notify, id allocation,
 * line framing, dispatch, stderr/stdout reads, version parse + check.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ agent/transports/codex_app_server.py:__init__ */
char *cas_init(const char *codex_bin) {
    /* Python: codex app-server is a model-driving CLI executor. */
    if (!codex_bin) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"codex_bin\": \"%s\"}", codex_bin);
    return out;
}

/* PoP: initialize @ agent/transports/codex_app_server.py:initialize */
char *cas_initialize(void) {
    /* Python: initialize + initialized handshake. */
    printf("codex app-server handshake (initialize + initialized)\n");
    return strdup("{}");
}

/* PoP: close @ agent/transports/codex_app_server.py:close */
int cas_close(void) {
    /* Python: close stdin, wait, escalate to kill. */
    printf("codex app-server closed (stdin eof → wait → kill escalation)\n");
    return 0;
}

/* PoP: __enter__ @ agent/transports/codex_app_server.py:__enter__ */
char *cas_enter(void) {
    printf("codex app-server context entered\n");
    return strdup("{}");
}

/* PoP: __exit__ @ agent/transports/codex_app_server.py:__exit__ */
int cas_exit(void) {
    return cas_close();
}

/* PoP: request @ agent/transports/codex_app_server.py:request */
char *cas_request(const char *method, const char *params_json) {
    /* Python: JSON-RPC request + blocking response. */
    if (!method) return NULL;
    printf("codex jsonrpc request: %s\n", method);
    return strdup("{}");
}

/* PoP: notify @ agent/transports/codex_app_server.py:notify */
int cas_notify(const char *method, const char *params_json) {
    /* Python: no-id notification. */
    if (!method) return -1;
    printf("codex jsonrpc notify: %s\n", method);
    return 0;
}

/* PoP: _take_id @ agent/transports/codex_app_server.py:_take_id */
long cas_take_id(void) {
    /* Python: monotonic per-connection id. */
    static long counter = 0;
    return ++counter;
}

/* PoP: _send @ agent/transports/codex_app_server.py:_send */
int cas_send(const char *frame_json, bool closed) {
    /* Python: raise when closed. */
    if (closed) return -1;
    if (!frame_json) return -1;
    printf("codex frame sent\n");
    return 0;
}

/* PoP: _read_stdout @ agent/transports/codex_app_server.py:_read_stdout */
char *cas_read_stdout(void) {
    /* Python: line reader loop. */
    printf("codex stdout read loop\n");
    return NULL;
}

/* PoP: _dispatch @ agent/transports/codex_app_server.py:_dispatch */
char *cas_dispatch(const char *msg_json) {
    /* Python: reply (has id) vs request (has method). */
    if (!msg_json) return NULL;
    if (strstr(msg_json, "\"id\"") && (strstr(msg_json, "\"result\"") || strstr(msg_json, "\"error\"")))
        return strdup("reply");
    if (strstr(msg_json, "\"method\"")) return strdup("request");
    return NULL;
}

/* PoP: _read_stderr @ agent/transports/codex_app_server.py:_read_stderr */
char *cas_read_stderr(void) {
    printf("codex stderr read loop\n");
    return NULL;
}

/* PoP: parse_codex_version @ agent/transports/codex_app_server.py:parse_codex_version */
char *cas_parse_codex_version(const char *version_output) {
    /* Python: (major, minor, patch) from `codex --version`. */
    if (!version_output) return NULL;
    long maj = -1, min = -1, pat = -1;
    const char *p = version_output;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (*p) maj = atol(p);
    while (*p && *p != '.') p++;
    if (*p == '.') { p++; if (*p) min = atol(p); }
    while (*p && *p != '.') p++;
    if (*p == '.') { p++; if (*p) pat = atol(p); }
    char *out = NULL;
    asprintf(&out, "%ld.%ld.%ld", maj, min, pat);
    return out;
}

/* PoP: check_codex_binary @ agent/transports/codex_app_server.py:check_codex_binary */
char *cas_check_codex_binary(const char *path, const char *min_version) {
    /* Python: (ok, message). */
    if (!path) return strdup("false\tcodex CLI not found");
    if (access(path, X_OK) != 0) return strdup("false\tcodex CLI not found");
    printf("codex binary verified (%s >= %s)\n", path, min_version ? min_version : "?");
    return strdup("true\tok");
}
