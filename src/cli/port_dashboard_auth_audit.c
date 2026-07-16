/*
 * port_dashboard_auth_audit.c — C port of hermes_cli/dashboard_auth/audit.py
 *
 * Audit log for dashboard-auth events. Profile-aware location under
 * $SLERMES_HOME/logs/dashboard-auth.log. One JSON object per line;
 * token-like fields are stripped before serialisation. Minimal dependency
 * surface — no hermes_constants import (mirrors the Python, which avoids
 * an import cycle with the middleware that loads early).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* Field names that must never hit the log raw. Any kwarg matching one of
 * these is silently dropped. */
static const char *G_REDACTED_FIELDS[] = {
    "access_token", "refresh_token", "code", "code_verifier",
    "state", "ticket", "cookie", "Authorization", "authorization",
    NULL,
};

static pthread_mutex_t g_write_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct { const char *k; const char *v; } dash_audit_kv_t;

/* ================================================================
 *  _resolve_log_path
 * ================================================================ */
/* PoP: dash_audit_resolve_log_path @ hermes_cli/dashboard_auth/audit.py:_resolve_log_path */
/* "$SLERMES_HOME/logs/dashboard-auth.log" with the standard fallback:
 * env var wins, else ~/.slermes. Writes into a static buffer (do not
 * free). */
const char *dash_audit_resolve_log_path(void)
{
    static char buf[4096];
    const char *home = getenv("SLERMES_HOME");
    if (!home || !home[0]) {
        const char *h = getenv("HOME");
        home = h ? h : "~";
    }
    snprintf(buf, sizeof(buf), "%s/logs/dashboard-auth.log", home);
    return buf;
}

/* ================================================================
 *  audit_log
 * ================================================================ */
/* PoP: dash_audit_log @ hermes_cli/dashboard_auth/audit.py:audit_log */
/* Append one event to the audit log. Token-like fields (G_REDACTED_FIELDS)
 * are dropped. Missing log directory is created. Write failures are logged
 * at WARNING but never raise — auth must not fail because the logger broke.
 * Signature: dash_audit_log(event, key1, val1, key2, val2, ..., NULL). */
void dash_audit_log(const char *event, ...)
{
    if (!event) return;

    va_list ap;
    va_start(ap, event);
    dash_audit_kv_t kvs[64];
    int nkv = 0;
    const char *k;
    while ((k = va_arg(ap, const char *)) != NULL) {
        const char *v = va_arg(ap, const char *);
        if (nkv < 64) { kvs[nkv].k = k; kvs[nkv].v = v ? v : ""; nkv++; }
    }
    va_end(ap);

    char *obj = (char *)malloc(8192);
    if (!obj) return;
    int off = 0;
    off += snprintf(obj + off, 8192 - (size_t)off, "{\"ts\":\"");
    time_t now = time(NULL);
    struct tm tmutc;
    gmtime_r(&now, &tmutc);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmutc);
    off += snprintf(obj + off, 8192 - (size_t)off, "%s\",\"event\":\"%s\"", ts, event);
    for (int i = 0; i < nkv; i++) {
        int redacted = 0;
        for (int r = 0; G_REDACTED_FIELDS[r]; r++)
            if (strcmp(kvs[i].k, G_REDACTED_FIELDS[r]) == 0) { redacted = 1; break; }
        if (redacted) continue;
        off += snprintf(obj + off, 8192 - (size_t)off, ",\"%s\":\"", kvs[i].k);
        const char *s = kvs[i].v;
        for (const char *p = s; *p && off < 8191; p++) {
            if (*p == '"' || *p == '\\') { obj[off++] = '\\'; obj[off++] = *p; }
            else if (*p == '\n') { obj[off++] = '\\'; obj[off++] = 'n'; }
            else obj[off++] = *p;
        }
        off += snprintf(obj + off, 8192 - (size_t)off, "\"");
    }
    off += snprintf(obj + off, 8192 - (size_t)off, "}\n");
    obj[off] = '\0';

    const char *path = dash_audit_resolve_log_path();
    pthread_mutex_lock(&g_write_lock);
    char dircopy[4096];
    snprintf(dircopy, sizeof(dircopy), "%s", path);
    char *slash = strrchr(dircopy, '/');
    if (slash) { *slash = '\0'; mkdir(dircopy, 0755); }
    FILE *f = fopen(path, "a");
    if (f) { fputs(obj, f); fclose(f); }
    else {
        hermes_log(LOG_WARNING, "dash-auth", "audit log write failed: %s", path);
    }
    pthread_mutex_unlock(&g_write_lock);
    free(obj);
}
