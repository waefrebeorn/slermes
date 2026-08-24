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

/* ================================================================
 *  snapshot_registration
 * ================================================================ */
/* PoP: dash_auth_snapshot_registration @ hermes_cli/dashboard_auth/registry.py:snapshot_registration */
const dash_auth_provider_t *dash_auth_snapshot_registration(const char *name)
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
 *  restore_registration
 * ================================================================ */
/* PoP: dash_auth_restore_registration @ hermes_cli/dashboard_auth/registry.py:restore_registration */
bool dash_auth_restore_registration(const char *name,
                                    const dash_auth_provider_t *current,
                                    const dash_auth_provider_t *previous)
{
    if (!name || !current) return false;
    bool ok = false;
    pthread_mutex_lock(&g_lock);
    int idx = -1;
    for (int i = 0; i < g_nproviders; i++) {
        if (strcmp(g_providers[i].name, name) == 0) { idx = i; break; }
    }
    /* Python: `if target.get(name) is not current: return False` — the live
     * registration must be the very object the caller snapshotted. The C
     * registry stores field copies, so identity is approximated by name +
     * display_name + capability flags. A NULL current never matches a
     * registered slot; an absent slot matches only the remove path. */
    bool is_current;
    if (idx < 0) {
        is_current = false;
    } else {
        dash_auth_provider_t *live = &g_providers[idx];
        is_current = strcmp(live->name, current->name) == 0 &&
                     ((live->display_name && current->display_name)
                          ? strcmp(live->display_name, current->display_name) == 0
                          : live->display_name == current->display_name) &&
                     live->supports_token == current->supports_token &&
                     live->supports_session == current->supports_session;
    }
    if (is_current && previous == NULL) {
        /* pop(name, None): remove by shifting the tail down. */
        for (int i = idx; i < g_nproviders - 1; i++)
            g_providers[i] = g_providers[i + 1];
        g_nproviders--;
        ok = true;
    } else if (is_current && previous != NULL) {
        g_providers[idx].name          = previous->name;
        g_providers[idx].display_name  = previous->display_name;
        g_providers[idx].supports_token   = previous->supports_token;
        g_providers[idx].supports_session = previous->supports_session;
        ok = true;
    }
    pthread_mutex_unlock(&g_lock);
    return ok;
}

/* ================================================================
 *  register_global_provider
 * ================================================================ */
/* PoP: dash_auth_register_global_provider @ hermes_cli/dashboard_auth/registry.py:register_global_provider */
bool dash_auth_register_global_provider(const dash_auth_provider_t *provider)
{
    if (!provider || !provider->name) return false;
    pthread_mutex_lock(&g_lock);
    int idx = -1;
    for (int i = 0; i < g_nproviders; i++) {
        if (strcmp(g_providers[i].name, provider->name) == 0) { idx = i; break; }
    }
    if (idx >= 0) {
        /* Upsert: replace in place instead of raising. */
        dash_auth_provider_t *dst = &g_providers[idx];
        dst->display_name       = provider->display_name;
        dst->supports_token     = provider->supports_token;
        dst->supports_session   = provider->supports_session;
        pthread_mutex_unlock(&g_lock);
        hermes_log(LOG_INFO, "dash-auth",
                   "registered global provider %s (%s)",
                   provider->name,
                   provider->display_name ? provider->display_name : "");
        return true;
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
    hermes_log(LOG_INFO, "dash-auth", "registered global provider %s (%s)",
               provider->name,
               provider->display_name ? provider->display_name : "");
    return true;
}

/* ================================================================
 *  unregister_global_provider
 * ================================================================ */
/* PoP: dash_auth_unregister_global_provider @ hermes_cli/dashboard_auth/registry.py:unregister_global_provider */
bool dash_auth_unregister_global_provider(const char *name,
                                          const dash_auth_provider_t *provider)
{
    if (!name || !provider || !provider->name) return false;
    bool removed = false;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_nproviders; i++) {
        if (strcmp(g_providers[i].name, name) != 0) continue;
        /* Identity-conditional: only the still-current handle clears the
         * slot. Field-copy identity, same as restore_registration. */
        dash_auth_provider_t *live = &g_providers[i];
        bool same = ((live->display_name && provider->display_name)
                         ? strcmp(live->display_name, provider->display_name) == 0
                         : live->display_name == provider->display_name) &&
                    live->supports_token == provider->supports_token &&
                    live->supports_session == provider->supports_session;
        if (same) {
            for (int j = i; j < g_nproviders - 1; j++)
                g_providers[j] = g_providers[j + 1];
            g_nproviders--;
            removed = true;
        }
        break;
    }
    pthread_mutex_unlock(&g_lock);
    return removed;
}
