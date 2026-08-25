#include "hermes_agent.h"
/*
 * browser_registry.c — Browser Provider Registry for Hermes C.
 *
 * MS02: Port of Python agent/browser_registry.py (192 LOC).
 * Central map of registered cloud browser providers.
 * Populated by plugins at registration time; consumed by browser.c
 * to route cloud-mode browser_* tool calls to the active backend.
 *
 * Active selection mirrors Python resolution order:
 *   1. browser.cloud_provider in config.yaml (explicit override)
 *   2. Legacy preference: browser-use → browserbase (filtered by availability)
 *   3. Firecrawl only via explicit config (not in legacy walk)
 *   4. Otherwise NULL — dispatcher falls back to local browser mode
 */

#include "browser_provider.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __linux__
#include <pthread.h>
#endif

/* ── Registry state ─────────────────────────────────────────────── */

#define MAX_PROVIDERS 16

static browser_provider_t *g_providers[MAX_PROVIDERS];
static int g_provider_count = 0;
#ifdef __linux__
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Legacy auto-detect order — matches Python _LEGACY_PREFERENCE.
 * Firecrawl intentionally absent. */
static const char *LEGACY_PREFERENCE[] = {
    "browser-use",
    "browserbase",
    NULL
};

/* ── Registration ──────────────────────────────────────────────── */

/* Port of Python: browser_registry.py register_provider() — line 52 */
int browser_registry_register(browser_provider_t *provider) {
    if (!provider || !provider->name || !provider->name[0])
        return -1;

#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif

    /* Check for existing registration (re-registration overwrites) */
    for (int i = 0; i < g_provider_count; i++) {
        if (g_providers[i] && strcmp(g_providers[i]->name, provider->name) == 0) {
            g_providers[i] = provider;
#ifdef __linux__
            pthread_mutex_unlock(&g_lock);
#endif
            return 0; /* re-registered */
        }
    }

    /* Add new provider */
    if (g_provider_count >= MAX_PROVIDERS) {
#ifdef __linux__
        pthread_mutex_unlock(&g_lock);
#endif
        return -1; /* registry full */
    }

    g_providers[g_provider_count++] = provider;

#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
    return 0;
}

/* ── Lookup ────────────────────────────────────────────────────── */

/* Port of Python: browser_registry.py get_provider() — line 89 */
const browser_provider_t *browser_registry_get(const char *name) {
    if (!name || !name[0]) return NULL;

#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif

    const browser_provider_t *result = NULL;
    for (int i = 0; i < g_provider_count; i++) {
        if (g_providers[i] && strcmp(g_providers[i]->name, name) == 0) {
            result = g_providers[i];
            break;
        }
    }

#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
    return result;
}

/* ── List all providers ────────────────────────────────────────── */

/* Port of Python: browser_registry.py list_providers() — line 82 */
int browser_registry_list(const browser_provider_t ***out_list) {
#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif

    *out_list = (const browser_provider_t **)g_providers;
    int count = g_provider_count;

#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
    return count;
}

/* ── Active provider resolution ──────────────────────────────────
 * Mirrors Python _resolve() logic. */

/* AG26: Port of Python agent/browser_registry.py:_resolve(). */
/* Port of Python: browser_registry.py _resolve() — line 113 */
const browser_provider_t *browser_registry_resolve(const char *configured) {
#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif

    /* Take snapshot under lock */
    browser_provider_t *snapshot[MAX_PROVIDERS];
    int count = g_provider_count;
    for (int i = 0; i < count; i++)
        snapshot[i] = g_providers[i];

#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif

    /* 1. Explicit "local" short-circuit */
    if (configured && strcmp(configured, "local") == 0)
        return NULL;

    /* 2. Explicit config wins — return regardless of is_available()
     *    so user gets precise error message */
    if (configured && configured[0]) {
        for (int i = 0; i < count; i++) {
            if (snapshot[i] && strcmp(snapshot[i]->name, configured) == 0)
                return snapshot[i];
        }
        /* Configured name not registered — fall through to auto-detect */
    }

    /* 3. Legacy preference walk — filtered by availability */
    for (int l = 0; LEGACY_PREFERENCE[l]; l++) {
        for (int i = 0; i < count; i++) {
            if (snapshot[i] && strcmp(snapshot[i]->name, LEGACY_PREFERENCE[l]) == 0) {
                /* Check availability */
                bool available = true;
                if (snapshot[i]->is_available) {
                    available = snapshot[i]->is_available(snapshot[i]);
                }
                if (available)
                    return snapshot[i];
            }
        }
    }

    return NULL; /* no provider — dispatcher falls back to local mode */
}

/* ── Test-only reset ───────────────────────────────────────────── */

/* Port of Python: browser_registry.py _reset_for_tests() — line 189 */
void browser_registry_reset(void) {
#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif
    g_provider_count = 0;
    memset(g_providers, 0, sizeof(g_providers));
#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
}

/* ── snapshot / generation / restore (plugin re-registration safety) ──── */

static long g_generation = 0;

/* PoP: snapshot_registration @ agent/browser_registry.py
 * Look up a provider by name under the registry lock (global scope; the
 * C port has no scoped maps). Returns the provider pointer or NULL. */
browser_provider_t *browser_registry_snapshot_registration(const char *name)
{
    if (!name) return NULL;
#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif
    browser_provider_t *found = NULL;
    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i]->name, name) == 0) { found = g_providers[i]; break; }
    }
#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
    return found;
}

/* PoP: registry_generation @ agent/browser_registry.py
 * Cache fingerprint for consumers memoizing resolution. */
long browser_registry_generation(void)
{
    long gen;
#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif
    gen = g_generation;
#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
    return gen;
}

/* PoP: restore_registration @ agent/browser_registry.py
 * Restore a plugin registration only when *current* is still installed.
 * previous==NULL removes the slot; otherwise swap back. Bumps generation.
 * Returns false (no change) when the slot moved on. */
bool browser_registry_restore_registration(const char *name,
                                           browser_provider_t *current,
                                           browser_provider_t *previous)
{
    if (!name) return false;
    bool changed = false;
#ifdef __linux__
    pthread_mutex_lock(&g_lock);
#endif
    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i]->name, name) != 0 || g_providers[i] != current)
            continue;
        if (!previous) {
            /* remove: shift tail down */
            for (int j = i; j < g_provider_count - 1; j++)
                g_providers[j] = g_providers[j + 1];
            g_provider_count--;
        } else {
            g_providers[i] = previous;
        }
        g_generation++;
        changed = true;
        break;
    }
#ifdef __linux__
    pthread_mutex_unlock(&g_lock);
#endif
    return changed;
}
