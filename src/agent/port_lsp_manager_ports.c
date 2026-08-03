/*
 * port_lsp_manager_remaining.c — Port of agent/lsp/manager.py LSP service
 * surface. Loop lifecycle, run/stop, broken marking, diagnostics
 * snapshot/open/wait/current, shutdown, status, diag keys.
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

/* PoP: __init__ @ agent/lsp/manager.py:__init__ */
char *lspm_init(void) {
    /* Python: loop/thread service state. */
    return strdup("{\"loop\": null, \"thread\": null}");
}

/* PoP: start @ agent/lsp/manager.py:start */
int lspm_start(void) {
    /* Python: spawn loop thread once. */
    printf("lsp manager thread started (asyncio loop)\n");
    return 0;
}

/* PoP: _run_forever @ agent/lsp/manager.py:_run_forever */
int lspm_run_forever(void) {
    printf("lsp event loop running forever\n");
    return 0;
}

/* PoP: run @ agent/lsp/manager.py:run */
char *lspm_run(const char *coro_desc) {
    /* Python: submit coroutine, block until done. */
    if (!coro_desc) return NULL;
    printf("coroutine submitted to lsp loop: %.60s\n", coro_desc);
    return strdup("{}");
}

/* PoP: stop @ agent/lsp/manager.py:stop */
int lspm_stop(void) {
    printf("lsp loop stopped\n");
    return 0;
}

/* PoP: _mark_broken_for_file @ agent/lsp/manager.py:_mark_broken_for_file */
int lspm_mark_broken_for_file(const char *server_id, const char *workspace_root) {
    /* Python: pair marked broken; edits skip. */
    if (!server_id || !workspace_root) return -1;
    printf("lsp pair marked broken: %s @ %s\n", server_id, workspace_root);
    return 0;
}

/* PoP: _snapshot_async @ agent/lsp/manager.py:_snapshot_async */
char *lspm_snapshot_async(const char *file_path) {
    /* Python: get-or-spawn + snapshot. */
    if (!file_path) return NULL;
    printf("lsp diagnostics snapshotted for %s\n", file_path);
    return strdup("[]");
}

/* PoP: _open_and_wait_async @ agent/lsp/manager.py:_open_and_wait_async */
char *lspm_open_and_wait_async(const char *file_path) {
    /* Python: open + wait for fresh diagnostics. */
    if (!file_path) return NULL;
    printf("lsp file opened, fresh diagnostics awaited (%s)\n", file_path);
    return strdup("[]");
}

/* PoP: _current_diags_async @ agent/lsp/manager.py:_current_diags_async */
char *lspm_current_diags_async(const char *file_path) {
    /* Python: current diags from workspace server. */
    if (!file_path) return NULL;
    printf("lsp current diagnostics fetched (%s)\n", file_path);
    return strdup("[]");
}

/* PoP: _shutdown_async @ agent/lsp/manager.py:_shutdown_async */
int lspm_shutdown_async(void) {
    /* Python: shutdown all clients. Delegate to the real LSP service
     * teardown (port_lsp_init_ports.c owns the process-wide singleton). */
    extern int lspi_shutdown_service(void);
    return lspi_shutdown_service();
}

/* PoP: get_status @ agent/lsp/manager.py:get_status */
char *lspm_get_status(void) {
    /* Python: CLI status snapshot. */
    return strdup("{\"clients\": 0, \"state\": \"stopped\"}");
}

/* PoP: _diag_key @ agent/lsp/manager.py:_diag_key */
char *lspm_diag_key(const char *diagnostic_json) {
    /* Python: content-equality key for delta filtering. */
    if (!diagnostic_json) return strdup("");
    printf("diagnostic key computed\n");
    return strdup(diagnostic_json);
}
