/*
 * session_context.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Session context helpers
 *  Port of Python gateway/session_context.py.
 * ================================================================ */

/* Set a session context variable by setting both the env var and a thread-local.
 * Port of Python gateway/session_context.py set_current_session_id(). */
static pthread_key_t g_session_key;
static pthread_once_t g_session_once = PTHREAD_ONCE_INIT;

static void session_key_destructor(void *value) {
    free(value);
}


static void session_key_init(void) {
    pthread_key_create(&g_session_key, session_key_destructor);
}


/* Set the active session ID in both env and thread-local storage.
 * Port of Python gateway/session_context.py set_current_session_id().
 * AG26: Port of Python gateway/session_context.py:set_current_session_id().
 */
void set_current_session_id(const char *session_id) {
    if (!session_id) return;
    pthread_once(&g_session_once, session_key_init);
    setenv("HERMES_SESSION_ID", session_id, 1);
    char *old = pthread_getspecific(g_session_key);
    free(old);
    pthread_setspecific(g_session_key, strdup(session_id));
}


/* Set all session context variables.
 * Port of Python gateway/session_context.py set_session_vars().
 * AG26: Port of Python gateway/session_context.py:set_session_vars().
 */
void set_session_vars(const char *platform, const char *chat_id,
                       const char *chat_name, const char *thread_id,
                       const char *user_id, const char *user_name,
                       const char *session_key) {
    if (platform && *platform) setenv("HERMES_SESSION_PLATFORM", platform, 1);
    if (chat_id && *chat_id) setenv("HERMES_SESSION_CHAT_ID", chat_id, 1);
    if (chat_name && *chat_name) setenv("HERMES_SESSION_CHAT_NAME", chat_name, 1);
    if (thread_id && *thread_id) setenv("HERMES_SESSION_THREAD_ID", thread_id, 1);
    if (user_id && *user_id) setenv("HERMES_SESSION_USER_ID", user_id, 1);
    if (user_name && *user_name) setenv("HERMES_SESSION_USER_NAME", user_name, 1);
    if (session_key && *session_key) setenv("HERMES_SESSION_KEY", session_key, 1);
}


/* Clear all session context variables.
 * Port of Python gateway/session_context.py clear_session_vars().
 * AG26: Port of Python gateway/session_context.py:clear_session_vars().
 */
void clear_session_vars(void) {
    unsetenv("HERMES_SESSION_PLATFORM");
    unsetenv("HERMES_SESSION_CHAT_ID");
    unsetenv("HERMES_SESSION_CHAT_NAME");
    unsetenv("HERMES_SESSION_THREAD_ID");
    unsetenv("HERMES_SESSION_USER_ID");
    unsetenv("HERMES_SESSION_USER_NAME");
    unsetenv("HERMES_SESSION_KEY");
    unsetenv("HERMES_SESSION_MESSAGE_ID");
    pthread_once(&g_session_once, session_key_init);
    char *old = pthread_getspecific(g_session_key);
    free(old);
    pthread_setspecific(g_session_key, NULL);
}


/* PoP: reset_session_vars @ gateway/session_context.py:reset_session_vars */
/* Reset every session context variable to the "never bound in this context"
 * state (Python's _UNSET sentinel). Distinct from clear_session_vars(), which
 * marks vars "explicitly cleared" (""); in the C env-var model both collapse to
 * unsetenv (env vars carry no _UNSET/"" distinction), but reset additionally
 * restores the async-delivery capability to unset and clears the session cwd —
 * the freshly-spawned-task baseline that prevents cross-session ContextVar
 * inheritance leaks (see the Python docstring / test_session_context_inheritance). */
void reset_session_vars(void) {
    unsetenv("HERMES_SESSION_PLATFORM");
    unsetenv("HERMES_SESSION_CHAT_ID");
    unsetenv("HERMES_SESSION_CHAT_NAME");
    unsetenv("HERMES_SESSION_THREAD_ID");
    unsetenv("HERMES_SESSION_USER_ID");
    unsetenv("HERMES_SESSION_USER_NAME");
    unsetenv("HERMES_SESSION_KEY");
    unsetenv("HERMES_SESSION_MESSAGE_ID");
    pthread_once(&g_session_once, session_key_init);
    char *old = pthread_getspecific(g_session_key);
    free(old);
    pthread_setspecific(g_session_key, NULL);
    /* _SESSION_ASYNC_DELIVERY.set(_UNSET) — restore "never bound here". */
    gw_session_reset_async_delivery();
    /* clear_session_cwd() — drop any inherited per-session working directory. */
    clear_session_cwd();
}


/* Read a session context variable with env fallback.
 * Port of Python gateway/session_context.py get_session_env().
 * AG26: Port of Python gateway/session_context.py:get_session_env().
 * Returns thread-local value if set, else getenv(), else default.
 * Returns a malloc'd string (caller must free). */
char *get_session_env(const char *name, const char *default_value) {
    if (!name || !*name) return default_value ? strdup(default_value) : strdup("");

    /* Check thread-local for HERMES_SESSION_ID */
    if (strcmp(name, "HERMES_SESSION_ID") == 0) {
        pthread_once(&g_session_once, session_key_init);
        char *val = pthread_getspecific(g_session_key);
        if (val) return strdup(val);
    }

    const char *env = getenv(name);
    if (env) return strdup(env);

    return default_value ? strdup(default_value) : strdup("");
}

/* ================================================================
 *  Async-delivery capability (ContextVar _SESSION_ASYNC_DELIVERY)
 *  Referenced by helpers.c reset_session_vars() via
 *  gw_session_reset_async_delivery(). Kept here so the split
 *  session_context module owns its lifecycle.
 * ================================================================ */

static int _session_async_delivery = -1; /* -1 = UNSET */

static int _session_context_engaged = 0;

void gw_session_set_async_delivery(int supported)
{
    _session_async_delivery = supported ? 1 : 0;
    _session_context_engaged = 1;
}

void gw_session_reset_async_delivery(void)
{
    _session_async_delivery = -1;
}

/* PoP: gw_session_context_engaged @ gateway/session_context.py:session_context_engaged */
/* True if any session has been bound via set_session_vars in this process
 * (mirrors Python _session_context_engaged process-global flag). */
int gw_session_context_engaged(void)
{
    return _session_context_engaged;
}

/* PoP: gw_session_async_delivery_supported @ gateway/session_context.py:async_delivery_supported */
/* Whether the current session can deliver a background completion later.
 * Returns true unless the active session was bound by a stateless adapter
 * (value explicitly set false). -1 (UNSET) means "never bound" -> true. */
int gw_session_async_delivery_supported(void)
{
    if (_session_async_delivery < 0) return 1;
    return _session_async_delivery ? 1 : 0;
}
