/*
 * port_dashboard_auth_registry.c — C port of
 * hermes_cli/dashboard_auth/registry.py
 *
 * Module-level registry for DashboardAuthProvider instances. Thread-safe via
 * a mutex (mirrors the Python `_lock`). Providers register themselves at
 * startup; the auth-gate middleware consults the list-/get-provider
 * accessors. Only the fields the middleware actually reads are tracked.
 */

#include "hermes_dash_auth_registry.h"
#include "hermes_logger.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define DASH_AUTH_MAX_PROVIDERS 64

static dash_auth_provider_t g_providers[DASH_AUTH_MAX_PROVIDERS];
static int                   g_nproviders = 0;
static pthread_mutex_t       g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ================================================================
 *  register_provider
 * ================================================================ */
/* PoP: dash_auth_register_provider @ hermes_cli/dashboard_auth/registry.py:register_provider */
bool dash_auth_register_provider(const dash_auth_provider_t *provider)
{
    if (!provider || !provider->name) return false;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_nproviders; i++) {
        if (strcmp(g_providers[i].name, provider->name) == 0) {
            pthread_mutex_unlock(&g_lock);
            return false;   /* already registered (ValueError in Python) */
        }
    }
    if (g_nproviders >= DASH_AUTH_MAX_PROVIDERS) {
        pthread_mutex_unlock(&g_lock);
        return false;
    }
    dash_auth_provider_t *dst = &g_providers[g_nproviders++];
    dst->name            = provider->name;
    dst->display_name    = provider->display_name;
    dst->supports_token  = provider->supports_token;
    dst->supports_session= provider->supports_session;
    pthread_mutex_unlock(&g_lock);
    hermes_log(LOG_INFO, "dash-auth", "registered provider %s (%s)",
               provider->name, provider->display_name ? provider->display_name : "");
    return true;
}

/* ================================================================
 *  get_provider
 * ================================================================ */
/* PoP: dash_auth_get_provider @ hermes_cli/dashboard_auth/registry.py:get_provider */
const dash_auth_provider_t *dash_auth_get_provider(const char *name)
{
    if (!name) return NULL;
    pthread_mutex_lock(&g_lock);
    const dash_auth_provider_t *found = NULL;
    for (int i = 0; i < g_nproviders; i++) {
        if (strcmp(g_providers[i].name, name) == 0) { found = &g_providers[i]; break; }
    }
    pthread_mutex_unlock(&g_lock);
    return found;
}

/* ================================================================
 *  list_providers
 * ================================================================ */
/* PoP: dash_auth_list_providers @ hermes_cli/dashboard_auth/registry.py:list_providers */
dash_auth_provider_t **dash_auth_list_providers(int *out_n)
{
    pthread_mutex_lock(&g_lock);
    dash_auth_provider_t **arr =
        (dash_auth_provider_t **)malloc((size_t)(g_nproviders + 1) * sizeof(void *));
    int n = 0;
    for (int i = 0; i < g_nproviders; i++)
        arr[n++] = &g_providers[i];
    arr[n] = NULL;
    pthread_mutex_unlock(&g_lock);
    if (out_n) *out_n = n;
    return arr;
}

/* ================================================================
 *  list_token_providers
 * ================================================================ */
/* PoP: dash_auth_list_token_providers @ hermes_cli/dashboard_auth/registry.py:list_token_providers */
dash_auth_provider_t **dash_auth_list_token_providers(int *out_n)
{
    pthread_mutex_lock(&g_lock);
    dash_auth_provider_t **arr =
        (dash_auth_provider_t **)malloc((size_t)(g_nproviders + 1) * sizeof(void *));
    int n = 0;
    for (int i = 0; i < g_nproviders; i++) {
        if (g_providers[i].supports_token)
            arr[n++] = &g_providers[i];
    }
    arr[n] = NULL;
    pthread_mutex_unlock(&g_lock);
    if (out_n) *out_n = n;
    return arr;
}

/* ================================================================
 *  list_session_providers
 * ================================================================ */
/* PoP: dash_auth_list_session_providers @ hermes_cli/dashboard_auth/registry.py:list_session_providers */
dash_auth_provider_t **dash_auth_list_session_providers(int *out_n)
{
    pthread_mutex_lock(&g_lock);
    dash_auth_provider_t **arr =
        (dash_auth_provider_t **)malloc((size_t)(g_nproviders + 1) * sizeof(void *));
    int n = 0;
    for (int i = 0; i < g_nproviders; i++) {
        if (g_providers[i].supports_session)
            arr[n++] = &g_providers[i];
    }
    arr[n] = NULL;
    pthread_mutex_unlock(&g_lock);
    if (out_n) *out_n = n;
    return arr;
}

/* ================================================================
 *  clear_providers (test-only)
 * ================================================================ */
/* PoP: dash_auth_clear_providers @ hermes_cli/dashboard_auth/registry.py:clear_providers */
void dash_auth_clear_providers(void)
{
    pthread_mutex_lock(&g_lock);
    g_nproviders = 0;
    pthread_mutex_unlock(&g_lock);
}
