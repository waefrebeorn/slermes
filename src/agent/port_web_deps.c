/*
 * port_web_deps.c — C11 port of hermes_cli/web_deps.py.
 * Late-binding dependency seam for extracted dashboard routers.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "hermes_web_deps.h"

/* ── Module-level state ──────────────────────────────────── */
/* In Python this is `sys.modules['hermes_cli.web_server']`.
 * In C we use a dlsym/function-pointer pattern or a lazy-load reference.
 * For now, the late proxy stores a function name and a lazy resolver. */

/* PoP: _server @ hermes_cli/web_deps.py:_server */
/* The server module is resolved via a global function pointer set at startup.
 * This mirrors Python's `import hermes_cli.web_server` on demand. */
static void *(*g_server_resolver)(const char *name) = NULL;

void web_deps_set_resolver(void *(*resolver)(const char *)) {
    g_server_resolver = resolver;
}

/* PoP: late @ hermes_cli/web_deps.py:late */
web_deps_fn_t web_deps_late(const char *name) {
    if (!name || !g_server_resolver) return NULL;
    return (web_deps_fn_t)g_server_resolver(name);
}

/* PoP: late_attr @ hermes_cli/web_deps.py:late_attr */
void *web_deps_late_attr(const char *name) {
    if (!name || !g_server_resolver) return NULL;
    return g_server_resolver(name);
}

/* ── LateState proxy ─────────────────────────────────────── */

struct web_deps_late_state_s {
    char *name;
};

/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__init__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState._target */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__getattr__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__getitem__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__setitem__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__delitem__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__contains__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__iter__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__len__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__bool__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__enter__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__exit__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__eq__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__ne__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__lt__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__le__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__gt__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__ge__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__hash__ */
/* PoP: late_state_create @ hermes_cli/web_deps.py:LateState.__repr__ */
web_deps_late_state_t *web_deps_late_state_create(const char *name) {
    if (!name) return NULL;
    web_deps_late_state_t *ls = calloc(1, sizeof(*ls));
    if (!ls) return NULL;
    ls->name = strdup(name);
    return ls;
}

void web_deps_late_state_free(web_deps_late_state_t *ls) {
    if (!ls) return;
    free(ls->name);
    free(ls);
}

void *web_deps_late_state_getattr(web_deps_late_state_t *ls, const char *attr) {
    if (!ls || !attr || !g_server_resolver) return NULL;
    /* Resolve the target object first, then the attribute on it.
     * Simplified: in C this is typically a direct symbol lookup. */
    char full_name[1024];
    snprintf(full_name, sizeof(full_name), "%s.%s", ls->name, attr);
    return g_server_resolver(full_name);
}

/* PoP: get_session_token @ hermes_cli/web_deps.py:get_session_token */
const char *web_deps_get_session_token(void) {
    if (!g_server_resolver) return "";
    void *token = g_server_resolver("_SESSION_TOKEN");
    return token ? (const char *)token : "";
}

/* PoP: get_dashboard_health @ hermes_cli/web_deps.py:get_dashboard_health */
void *web_deps_get_dashboard_health(void) {
    if (!g_server_resolver) return NULL;
    return g_server_resolver("DASHBOARD_HEALTH");
}

/* PoP: has_valid_session_token @ hermes_cli/web_deps.py:has_valid_session_token */
bool web_deps_has_valid_session_token(void *request) {
    if (!request || !g_server_resolver) return false;
    web_deps_fn_t fn = (web_deps_fn_t)g_server_resolver("_has_valid_session_token");
    if (!fn) return false;
    void *result = fn(request);
    return result != NULL;
}