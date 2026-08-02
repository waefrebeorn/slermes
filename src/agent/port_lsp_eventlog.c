/*
 * port_lsp_eventlog.c — C port of agent/lsp/eventlog.py.
 * Leveled LSP event logging with once-per-key announce buckets.
 * PoP-annotated per function; self-contained.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define EL_MAX_KEYS 256

static char *g_announced_active[EL_MAX_KEYS];
static char *g_announced_no_root[EL_MAX_KEYS];
static char *g_announced_unavailable[EL_MAX_KEYS];
static char *g_announced_no_server[EL_MAX_KEYS];
static int g_na = 0, g_nn = 0, g_nu = 0, g_ns = 0;

static bool el_announce_once(char **bucket, int *count, const char *key) {
    for (int i = 0; i < *count; i++)
        if (bucket[i] && strcmp(bucket[i], key) == 0) return false;
    if (*count < EL_MAX_KEYS) bucket[(*count)++] = strdup(key);
    return true;
}

static void el_emit(const char *server_id, const char *level, const char *message) {
    fprintf(stderr, "lsp[%s] %s: %s\n", server_id ? server_id : "?", level, message);
}

/* PoP: lsp_log_short_path @ agent/lsp/eventlog.py:_short_path */
char *lsp_log_short_path(const char *file_path) {
    /* Render relative to cwd when sensible, else absolute. */
    if (!file_path || !*file_path) return file_path ? strdup(file_path) : NULL;
    char cwd[2048];
    if (!getcwd(cwd, sizeof(cwd))) return strdup(file_path);
    size_t cl = strlen(cwd);
    if (strncmp(file_path, cwd, cl) == 0 && file_path[cl] == '/') {
        return strdup(file_path + cl + 1);
    }
    return strdup(file_path);
}

/* PoP: lsp_log_emit @ agent/lsp/eventlog.py:_emit */
void lsp_log_emit(const char *server_id, const char *level, const char *message) {
    /* Python: event_log.log(level, "lsp[%s] %s", server_id, message). */
    char line[2048];
    snprintf(line, sizeof(line), "lsp[%s] %s", server_id ? server_id : "", message ? message : "");
    if (level && strcmp(level, "ERROR") == 0) fprintf(stderr, "%s\n", line);
    else fprintf(stdout, "%s\n", line);
}

/* PoP: lsp_log_clean @ agent/lsp/eventlog.py:log_clean */
void lsp_log_clean(const char *server_id, const char *file_path) {
    char *sp = lsp_log_short_path(file_path);
    char msg[512];
    snprintf(msg, sizeof(msg), "clean (%s)", sp ? sp : "");
    el_emit(server_id, "DEBUG", msg);
    free(sp);
}

/* PoP: lsp_log_disabled @ agent/lsp/eventlog.py:log_disabled */
void lsp_log_disabled(const char *server_id, const char *reason, const char *file_path) {
    char *sp = lsp_log_short_path(file_path);
    char msg[1024];
    snprintf(msg, sizeof(msg), "skipped: %s (%s)", reason ? reason : "", sp ? sp : "");
    el_emit(server_id, "DEBUG", msg);
    free(sp);
}

/* PoP: lsp_log_active @ agent/lsp/eventlog.py:log_active */
void lsp_log_active(const char *server_id, const char *workspace_root) {
    char key[512];
    snprintf(key, sizeof(key), "%s|%s", server_id ? server_id : "", workspace_root ? workspace_root : "");
    if (el_announce_once(g_announced_active, &g_na, key)) {
        char msg[512];
        snprintf(msg, sizeof(msg), "active for %s", workspace_root ? workspace_root : "");
        el_emit(server_id, "INFO", msg);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "active for %s", workspace_root ? workspace_root : "");
        el_emit(server_id, "DEBUG", msg);
    }
}

/* PoP: lsp_log_diagnostics @ agent/lsp/eventlog.py:log_diagnostics */
void lsp_log_diagnostics(const char *server_id, long long count, const char *file_path) {
    char *sp = lsp_log_short_path(file_path);
    char msg[512];
    snprintf(msg, sizeof(msg), "%lld diags (%s)", count, sp ? sp : "");
    el_emit(server_id, "INFO", msg);
    free(sp);
}

/* PoP: lsp_log_no_project_root @ agent/lsp/eventlog.py:log_no_project_root */
void lsp_log_no_project_root(const char *server_id, const char *file_path) {
    char key[512];
    snprintf(key, sizeof(key), "%s|%s", server_id ? server_id : "", file_path ? file_path : "");
    char *sp = lsp_log_short_path(file_path);
    char msg[512];
    snprintf(msg, sizeof(msg), "no project root for %s", sp ? sp : "");
    if (el_announce_once(g_announced_no_root, &g_nn, key))
        el_emit(server_id, "INFO", msg);
    else
        el_emit(server_id, "DEBUG", msg);
    free(sp);
}

/* PoP: lsp_log_server_unavailable @ agent/lsp/eventlog.py:log_server_unavailable */
void lsp_log_server_unavailable(const char *server_id, const char *binary_or_pkg) {
    char key[512];
    snprintf(key, sizeof(key), "%s|%s", server_id ? server_id : "", binary_or_pkg ? binary_or_pkg : "");
    char msg[512];
    snprintf(msg, sizeof(msg), "server binary unavailable: %s", binary_or_pkg ? binary_or_pkg : "");
    if (el_announce_once(g_announced_unavailable, &g_nu, key))
        el_emit(server_id, "WARNING", msg);
    else
        el_emit(server_id, "DEBUG", msg);
}

/* PoP: lsp_log_no_server_configured @ agent/lsp/eventlog.py:log_no_server_configured */
void lsp_log_no_server_configured(const char *server_id) {
    if (el_announce_once(g_announced_no_server, &g_ns, server_id ? server_id : ""))
        el_emit(server_id, "WARNING", "no server configured");
}

/* PoP: lsp_log_timeout @ agent/lsp/eventlog.py:log_timeout */
void lsp_log_timeout(const char *server_id, const char *kind, const char *file_path) {
    char *sp = lsp_log_short_path(file_path);
    char msg[512];
    snprintf(msg, sizeof(msg), "%s timed out for %s", kind ? kind : "request", sp ? sp : "");
    el_emit(server_id, "WARNING", msg);
    free(sp);
}

/* PoP: lsp_log_server_error @ agent/lsp/eventlog.py:log_server_error */
void lsp_log_server_error(const char *server_id, const char *file_path, const char *error) {
    char *sp = lsp_log_short_path(file_path);
    char msg[1024];
    snprintf(msg, sizeof(msg), "unexpected error for %s: %s", sp ? sp : "", error ? error : "");
    el_emit(server_id, "WARNING", msg);
    free(sp);
}
