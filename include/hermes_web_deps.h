#ifndef HERMES_WEB_DEPS_H
#define HERMES_WEB_DEPS_H

/*
 * hermes_web_deps.h — Late-binding dependency seam for dashboard routers.
 * Port of hermes_cli/web_deps.py.
 * C11, no C++.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Late-binding proxy ──────────────────────────────────── */

/* A late-binding proxy that resolves a named function/symbol at call time.
 * In C this is simply a function pointer or a lazy-init function. */

/* PoP: late @ hermes_cli/web_deps.py:late */
/* Return a pointer to a named function from the server module, or NULL.
 * This replaces Python's `late(name)` which returns a callable proxy. */
typedef void *(*web_deps_fn_t)(void *arg);
web_deps_fn_t web_deps_late(const char *name);

/* PoP: late_attr @ hermes_cli/web_deps.py:late_attr */
/* Read a named attribute from the server module right now. */
void *web_deps_late_attr(const char *name);

/* PoP: LateState @ hermes_cli/web_deps.py:LateState */
/* Live proxy for module-level state. Thin wrapper around an attribute lookup
 * on the server module, resolved at every access. */

typedef struct web_deps_late_state_s web_deps_late_state_t;

/* Create a LateState proxy for a named module attribute. */
web_deps_late_state_t *web_deps_late_state_create(const char *name);
void web_deps_late_state_free(web_deps_late_state_t *ls);

/* Accessors (resolve on every call) */
void *web_deps_late_state_getattr(web_deps_late_state_t *ls, const char *attr);

/* PoP: get_session_token @ hermes_cli/web_deps.py:get_session_token */
/* Current dashboard session token. */
const char *web_deps_get_session_token(void);

/* PoP: get_dashboard_health @ hermes_cli/web_deps.py:get_dashboard_health */
/* The DASHBOARD_HEALTH singleton owned by web_server. */
void *web_deps_get_dashboard_health(void);

/* PoP: has_valid_session_token @ hermes_cli/web_deps.py:has_valid_session_token */
/* Late-bound alias for web_server._has_valid_session_token. */
bool web_deps_has_valid_session_token(void *request);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WEB_DEPS_H */