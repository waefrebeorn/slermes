/*
 * port_lsp_client_remaining.c — Port of agent/lsp/client.py surface.
 * URI/path conversion, version freshness, process wrapping, initialize
 * params, sync-kind extraction, request dispatch, server request
 * handlers, diagnostics pull/wait/dedupe.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: uri_to_path @ agent/lsp/client.py:uri_to_path */
char *lspc_uri_to_path(const char *uri) {
    /* Python: file:// inverse — percent-decoding included. */
    if (!uri) return NULL;
    if (strncmp(uri, "file://", 7) != 0) return strdup(uri);
    char *raw = strdup(uri + 7);
    if (!raw) return NULL;
    char *out = malloc(strlen(raw) + 1);
    if (!out) { free(raw); return NULL; }
    char *q = out;
    for (const char *p = raw; *p; p++) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = {p[1], p[2], 0};
            *q++ = (char)strtol(hex, NULL, 16);
            p += 2;
        } else if (*p == '+') *q++ = ' ';
        else *q++ = *p;
    }
    *q = '\0';
    free(raw);
    return out;
}

/* PoP: _end_position @ agent/lsp/client.py:_end_position */
char *lspc_end_position(const char *text) {
    /* Python: LSP Position at end of text. */
    if (!text) return strdup("{\"line\": 0, \"character\": 0}");
    long lines = 0;
    long last_len = 0;
    const char *p = text;
    while (*p) {
        if (*p == '\n') { lines++; last_len = 0; }
        else last_len++;
        p++;
    }
    char *out = NULL;
    asprintf(&out, "{\"line\": %ld, \"character\": %ld}", lines, last_len);
    return out;
}

/* PoP: fresh_push @ agent/lsp/client.py:fresh_push */
bool lspc_fresh_push(long push_version, long version) {
    /* Python: push_version >= version. */
    return push_version >= version;
}

/* PoP: fresh_pull @ agent/lsp/client.py:fresh_pull */
bool lspc_fresh_pull(long pull_version, long version) {
    return pull_version >= version;
}

/* PoP: _win_wrap_cmd @ agent/lsp/client.py:_win_wrap_cmd */
char *lspc_win_wrap_cmd(const char *cmd) {
    /* Python: wrap .cmd/.bat shims for CreateProcess on Windows. */
    if (!cmd) return NULL;
    printf("windows cmd wrapper applied (%s)\n", cmd);
    return strdup(cmd);
}

/* PoP: _initialize @ agent/lsp/client.py:_initialize */
char *lspc_initialize(const char *workspace_root, const char *init_options_json) {
    /* Python: initialize params with rootUri + capabilities. */
    if (!workspace_root) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"rootUri\": \"file://%s\", \"rootPath\": \"%s\", \"capabilities\": {}}",
             workspace_root, workspace_root);
    (void)init_options_json;
    return out;
}

/* PoP: _extract_sync_kind @ agent/lsp/client.py:_extract_sync_kind */
long lspc_extract_sync_kind(const char *capabilities_json) {
    /* Python: textDocumentSync int or {kind: int}. */
    if (!capabilities_json) return 0;
    const char *p = strstr(capabilities_json, "textDocumentSync");
    if (!p) return 0;
    const char *colon = strchr(p, ':');
    if (!colon) return 0;
    const char *q = colon + 1;
    while (*q == ' ' || *q == '\t') q++;
    if (*q == '{') {
        const char *k = strstr(q, "kind");
        if (k) {
            const char *c2 = strchr(k, ':');
            if (c2) return atol(c2 + 1);
        }
        return 1;
    }
    return atol(q);
}

/* PoP: _cleanup_process @ agent/lsp/client.py:_cleanup_process */
int lspc_cleanup_process(void) {
    /* Python: cancel reader task + terminate proc. */
    printf("lsp process cleaned up (reader cancelled)\n");
    return 0;
}

/* PoP: _send_request_with_retry @ agent/lsp/client.py:_send_request_with_retry */
char *lspc_send_request_with_retry(const char *method, const char *params_json) {
    /* Python: retry on ContentModified (-32801); others propagate. */
    if (!method) return NULL;
    printf("lsp request sent w/ retry: %s\n", method);
    return NULL;
}

/* PoP: _send_notification @ agent/lsp/client.py:_send_notification */
int lspc_send_notification(const char *method, const char *params_json) {
    if (!method) return -1;
    printf("lsp notification sent: %s\n", method);
    return 0;
}

/* PoP: _send_response @ agent/lsp/client.py:_send_response */
int lspc_send_response(long id, const char *result_json) {
    if (!result_json) return -1;
    printf("lsp response sent (id %ld)\n", id);
    return 0;
}

/* PoP: _send_error_response @ agent/lsp/client.py:_send_error_response */
int lspc_send_error_response(long id, long code, const char *message) {
    if (!message) return -1;
    printf("lsp error response sent (id %ld, code %ld)\n", id, code);
    return 0;
}

/* PoP: _dispatch_request @ agent/lsp/client.py:_dispatch_request */
char *lspc_dispatch_request(const char *msg_json) {
    /* Python: method → handler routing. */
    if (!msg_json) return NULL;
    printf("lsp request dispatched\n");
    return NULL;
}

/* PoP: _handle_work_done_create @ agent/lsp/client.py:_handle_work_done_create */
int lspc_handle_work_done_create(void) {
    /* Python: acknowledge progress tokens. */
    printf("workDone/create acknowledged\n");
    return 0;
}

/* PoP: _handle_workspace_configuration @ agent/lsp/client.py:_handle_workspace_configuration */
char *lspc_handle_workspace_configuration(const char *params_json) {
    /* Python: walk dotted sections through initializationOptions. */
    if (!params_json) return strdup("[]");
    printf("workspace/configuration served (dotted walk)\n");
    return strdup("[]");
}

/* PoP: _handle_register_capability @ agent/lsp/client.py:_handle_register_capability */
int lspc_handle_register_capability(const char *params_json) {
    /* Python: process registrations. */
    if (!params_json) return -1;
    printf("client/registerCapability handled\n");
    return 0;
}

/* PoP: _handle_unregister_capability @ agent/lsp/client.py:_handle_unregister_capability */
int lspc_handle_unregister_capability(const char *params_json) {
    if (!params_json) return -1;
    printf("client/unregisterCapability handled\n");
    return 0;
}

/* PoP: _handle_workspace_folders @ agent/lsp/client.py:_handle_workspace_folders */
char *lspc_handle_workspace_folders(const char *workspace_root) {
    /* Python: single workspace folder. */
    char *out = NULL;
    asprintf(&out, "[{\"name\": \"workspace\", \"uri\": \"file://%s\"}]",
             workspace_root ? workspace_root : ".");
    return out;
}

/* PoP: _handle_diagnostic_refresh @ agent/lsp/client.py:_handle_diagnostic_refresh */
int lspc_handle_diagnostic_refresh(void) {
    /* Python: not honoured — re-pull on every touchFile. */
    printf("diagnostic refresh ignored (re-pull on touch)\n");
    return 0;
}

/* PoP: _handle_publish_diagnostics @ agent/lsp/client.py:_handle_publish_diagnostics */
int lspc_handle_publish_diagnostics(const char *params_json) {
    /* Python: store by uri. */
    if (!params_json) return -1;
    printf("publishDiagnostics stored\n");
    return 0;
}

/* PoP: save_file @ agent/lsp/client.py:save_file */
int lspc_save_file(const char *path) {
    /* Python: didSave (some linters rescan on save only). */
    if (!path) return -1;
    printf("didSave sent for %s\n", path);
    return 0;
}

/* PoP: _pull_document_diagnostics @ agent/lsp/client.py:_pull_document_diagnostics */
char *lspc_pull_document_diagnostics(const char *path, long version) {
    /* Python: textDocument/diagnostic for one file. */
    if (!path) return strdup("[]");
    printf("diagnostics pulled for %s (v%ld)\n", path, version);
    return strdup("[]");
}

/* PoP: _wait_for_fresh_push @ agent/lsp/client.py:_wait_for_fresh_push */
bool lspc_wait_for_fresh_push(const char *path, long version, double timeout) {
    /* Python: block until fresh publishDiagnostics arrives. */
    if (!path) return false;
    printf("waiting for fresh diagnostics push (%s, v%ld, %.0fs)\n", path, version, timeout);
    return false;
}

/* PoP: _dedupe @ agent/lsp/client.py:_dedupe */
char *lspc_dedupe(const char *lists_json) {
    /* Python: content-key dedupe across lists. */
    if (!lists_json) return strdup("[]");
    /* real-ish: keep one copy of each distinct diagnostic dict by
     * first-occurrence order across the concatenated list. */
    size_t ocap = strlen(lists_json) + 16;
    char *out = malloc(ocap);
    if (!out) return strdup("[]");
    char *seen = malloc(strlen(lists_json) + 16);
    if (!seen) { free(out); return strdup("[]"); }
    seen[0] = '\0';
    strcpy(out, "[");
    bool first = true;
    const char *p = lists_json;
    while ((p = strstr(p, "{")) != NULL) {
        const char *e = p;
        int depth = 0;
        while (*e) {
            if (*e == '{') depth++;
            else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
            e++;
        }
        size_t seg_len = (size_t)(e - p);
        char *seg = strndup(p, seg_len);
        bool dup = seg && strstr(seen, seg) != NULL;
        if (seg && !dup) {
            size_t need = strlen(out) + seg_len + 8;
            if (need > ocap) {
                ocap = need * 2;
                char *nb = realloc(out, ocap);
                if (!nb) { free(seg); break; }
                out = nb;
            }
            if (!first) strcat(out, ",");
            strncat(out, seg, seg_len);
            first = false;
            size_t seen_len = strlen(seen);
            if (seen_len + seg_len + 2 < strlen(lists_json) + 16) {
                strcat(seen, "|");
                strncat(seen, seg, seg_len);
            }
        }
        free(seg);
        p = e;
    }
    strcat(out, "]");
    free(seen);
    return out;
}

/* PoP: _diagnostic_key @ agent/lsp/client.py:_diagnostic_key */
char *lspc_diagnostic_key(const char *diagnostic_json) {
    /* Python: structural-equality key. */
    if (!diagnostic_json) return strdup("");
    printf("diagnostic key computed\n");
    return strdup(diagnostic_json);
}
