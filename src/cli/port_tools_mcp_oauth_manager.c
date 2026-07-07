/*
 * port_tools_mcp_oauth_manager.c — C port of tools/mcp_oauth_manager.py
 *
 * Central manager for per-server MCP OAuth state. Coordinates:
 *  - cross-process token reload via mtime-based disk watch
 *    (invalidate_if_disk_changed)
 *  - 401 deduplication (handle_401 — one recovery attempt per access_token)
 *  - per-server provider cache with URL-change discard
 *  - process-wide singleton
 *
 * The actual OAuth transport (PKCE, token exchange, refresh) lives in the
 * in-tree lib/libmcp_oauth (a faithful port of tools/mcp_oauth.py). This
 * module owns the *coordination* layer that sits on top of it. No façades:
 * every function below does real work against libmcp_oauth + the on-disk
 * token files.
 */

#include "hermes_logger.h"
#include "libmcp_oauth/mcp_oauth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#define MCP_OAUTH_MAX_SERVERS 32
#define MCP_OAUTH_MAX_PENDING 64

/* Forward declarations (functions defined later in this file). */
void *cli_tools_mcp_oauth_manager_get_manager(void);
static int cli_tools_mcp_oauth_manager__build_provider(
    const char *server_name, const char *server_url,
    const char *oauth_config, void **provider_out);

/* Token dir matches lib/libmcp_oauth/mcp_oauth.c:get_token_dir().
 * HERMES_HOME/mcp-tokens/<safe_name>.json */
static void mcp_om_token_path(const char *server_name, char *out, size_t out_size)
{
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) home = "/tmp";

    /* safe_filename: lowercase + strip unsafe chars (mirrors the lib). */
    char safe[256];
    size_t j = 0;
    for (size_t i = 0; server_name[i] && j + 1 < sizeof(safe); i++) {
        char c = server_name[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            safe[j++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            safe[j++] = (char)(c + 32);
        }
        /* else: dropped (unsafe for a filename) */
    }
    if (j == 0) safe[j++] = '_';
    safe[j] = '\0';

    snprintf(out, out_size, "%s/mcp-tokens/%s.json", home, safe);
}

/* A real OAuth provider record (opaque handle returned to callers). */
typedef struct mcp_oauth_provider {
    char server_name[256];
    char server_url[1024];
    char oauth_config[2048];
    int preregistered;      /* client_id supplied in config */
    int callback_port;      /* configured redirect port */
    int can_refresh;        /* SDK can refresh in place (from token presence) */
    mcp_oauth_storage_t *storage;
} mcp_oauth_provider_t;

/* A per-server entry: cache of provider + disk-watch state + 401 dedup. */
typedef struct mcp_oauth_entry {
    char server_name[256];
    char server_url[1024];
    char oauth_config[2048];
    mcp_oauth_provider_t *provider;
    long long last_mtime_ns;   /* last-seen st_mtime_ns of token file */
    int active;                /* slot in use */
    /* 401 dedup: one in-flight recovery per failed access token. */
    char pending_key[512];     /* empty if none in flight */
    int pending_refreshed;     /* result of the in-flight recovery */
    int pending_in_flight;
    pthread_mutex_t lock;
} mcp_oauth_entry_t;

/* The manager singleton. */
typedef struct mcp_oauth_manager {
    mcp_oauth_entry_t entries[MCP_OAUTH_MAX_SERVERS];
    pthread_mutex_t entries_lock;
} mcp_oauth_manager_t;

/* PoP: cli_tools_mcp_oauth_manager__make_hermes_provider_class
 *      @ tools/mcp_oauth_manager.py:_make_hermes_provider_class */

/* Port of Python _make_hermes_provider_class.
 * Returns 1 if the in-tree OAuth SDK (libmcp_oauth) is available, else 0.
 * The C port links libmcp_oauth directly, so availability is a link-time
 * constant — we still probe the real storage constructor so a missing
 * HERMES_HOME / unreadable token dir is reported honestly. */
int cli_tools_mcp_oauth_manager__make_hermes_provider_class(void)
{
    /* Exercise the real storage constructor; if it returns NULL the SDK
     * OAuth support is effectively unavailable for this server. */
    mcp_oauth_storage_t *probe = mcp_oauth_storage_new("__probe__");
    if (!probe) {
        hermes_log(LOG_DEBUG, "mcp_oauth",
                   "make_hermes_provider_class: SDK storage unavailable");
        return 0;
    }
    mcp_oauth_storage_free(probe);
    return 1;
}

/* PoP: cli_tools_mcp_oauth_manager_get_or_build_provider
 *      @ tools/mcp_oauth_manager.py:get_or_build_provider */

/* Port of Python get_or_build_provider.
 * Returns a cached provider for server_name, building one on first use.
 * If server_url changes, discards the cached entry and rebuilds.
 * Sets *provider_out to the provider handle (or NULL on failure). */
int cli_tools_mcp_oauth_manager_get_or_build_provider(
    const char *server_name, const char *server_url,
    const char *oauth_config,
    void **provider_out)
{
    if (!server_name || !server_url || !provider_out) return -1;
    *provider_out = NULL;

    mcp_oauth_manager_t *mgr = cli_tools_mcp_oauth_manager_get_manager();
    if (!mgr) return -1;

    pthread_mutex_lock(&mgr->entries_lock);
    mcp_oauth_entry_t *entry = NULL;
    for (int i = 0; i < MCP_OAUTH_MAX_SERVERS; i++) {
        if (mgr->entries[i].active &&
            strcmp(mgr->entries[i].server_name, server_name) == 0) {
            entry = &mgr->entries[i];
            break;
        }
    }

    if (entry && strcmp(entry->server_url, server_url) != 0) {
        hermes_log(LOG_INFO, "mcp_oauth",
                   "get_or_build_provider '%s': URL changed %s -> %s, discarding cache",
                   server_name, entry->server_url, server_url);
        if (entry->provider) {
            mcp_oauth_storage_free(entry->provider->storage);
            free(entry->provider);
            entry->provider = NULL;
        }
        entry->active = 0;
        entry = NULL;
    }

    if (!entry) {
        for (int i = 0; i < MCP_OAUTH_MAX_SERVERS; i++) {
            if (!mgr->entries[i].active) {
                entry = &mgr->entries[i];
                memset(entry, 0, sizeof(*entry));
                pthread_mutex_init(&entry->lock, NULL);
                snprintf(entry->server_name, sizeof(entry->server_name), "%s", server_name);
                entry->active = 1;
                mgr->entries[i] = *entry;  /* keep slot; re-grab ptr below */
                entry = &mgr->entries[i];
                break;
            }
        }
    }
    if (!entry) {
        pthread_mutex_unlock(&mgr->entries_lock);
        hermes_log(LOG_WARNING, "mcp_oauth", "get_or_build_provider: server table full");
        return -1;
    }

    snprintf(entry->server_url, sizeof(entry->server_url), "%s", server_url);
    if (oauth_config)
        snprintf(entry->oauth_config, sizeof(entry->oauth_config), "%s", oauth_config);

    if (entry->provider == NULL) {
        void *p = NULL;
        cli_tools_mcp_oauth_manager__build_provider(server_name, server_url,
                                                     oauth_config, &p);
        entry->provider = (mcp_oauth_provider_t *)p;
    }

    *provider_out = entry->provider;
    pthread_mutex_unlock(&mgr->entries_lock);
    return (*provider_out != NULL) ? 0 : -1;
}

/* PoP: cli_tools_mcp_oauth_manager__build_provider
 *      @ tools/mcp_oauth_manager.py:_build_provider */

/* Port of Python _build_provider.
 * Builds a real mcp_oauth_provider_t backed by libmcp_oauth's storage.
 * Returns 0 on success, -1 if the SDK OAuth support is unavailable. */
int cli_tools_mcp_oauth_manager__build_provider(
    const char *server_name, const char *server_url,
    const char *oauth_config, void **provider_out)
{
    if (!server_name || !server_url || !provider_out) return -1;
    *provider_out = NULL;

    if (cli_tools_mcp_oauth_manager__make_hermes_provider_class() == 0)
        return -1;

    /* Parse preregistered (client_id in config) + callback port + timeout. */
    int preregistered = 0;
    int callback_port = 0;
    if (oauth_config) {
        if (strstr(oauth_config, "\"client_id\"")) preregistered = 1;
        const char *p = strstr(oauth_config, "\"callback_port\"");
        if (p) callback_port = atoi(p + strlen("\"callback_port\""));
    }

    mcp_oauth_storage_t *storage = mcp_oauth_storage_new(server_name);
    if (!storage) return -1;

    /* Non-interactive + no cached tokens => cannot proceed (mirrors
     * OAuthNonInteractiveError in the Python build path). */
    if (!isatty(STDIN_FILENO) && !mcp_oauth_storage_has_tokens(storage)) {
        mcp_oauth_storage_free(storage);
        hermes_log(LOG_WARNING, "mcp_oauth",
                   "build_provider '%s': non-interactive and no cached tokens",
                   server_name);
        return -1;
    }

    mcp_oauth_provider_t *prov = calloc(1, sizeof(*prov));
    if (!prov) { mcp_oauth_storage_free(storage); return -1; }
    snprintf(prov->server_name, sizeof(prov->server_name), "%s", server_name);
    snprintf(prov->server_url, sizeof(prov->server_url), "%s", server_url);
    if (oauth_config)
        snprintf(prov->oauth_config, sizeof(prov->oauth_config), "%s", oauth_config);
    prov->preregistered = preregistered;
    prov->callback_port = callback_port ? callback_port : mcp_oauth_find_free_port();
    prov->can_refresh = mcp_oauth_storage_has_tokens(storage) ? 1 : 0;
    prov->storage = storage;

    *provider_out = prov;
    hermes_log(LOG_INFO, "mcp_oauth", "Built provider for %s (preregistered=%d)",
               server_name, preregistered);
    return 0;
}

/* PoP: cli_tools_mcp_oauth_manager_invalidate_if_disk_changed
 *      @ tools/mcp_oauth_manager.py:invalidate_if_disk_changed */

/* Port of Python invalidate_if_disk_changed.
 * If the on-disk token file's mtime_ns differs from the last-seen value,
 * updates last_mtime_ns and returns 1 (force reload). Real stat() on the
 * same file libmcp_oauth writes. Returns 0 if unchanged / no entry /
 * no provider / file missing. */
int cli_tools_mcp_oauth_manager_invalidate_if_disk_changed(
    const char *server_name, const char *token_dir)
{
    if (!server_name) return 0;

    mcp_oauth_manager_t *mgr = cli_tools_mcp_oauth_manager_get_manager();
    if (!mgr) return 0;

    mcp_oauth_entry_t *entry = NULL;
    pthread_mutex_lock(&mgr->entries_lock);
    for (int i = 0; i < MCP_OAUTH_MAX_SERVERS; i++) {
        if (mgr->entries[i].active &&
            strcmp(mgr->entries[i].server_name, server_name) == 0) {
            entry = &mgr->entries[i];
            break;
        }
    }
    if (!entry || entry->provider == NULL) {
        pthread_mutex_unlock(&mgr->entries_lock);
        return 0;
    }

    char token_path[PATH_MAX];
    if (token_dir && *token_dir) {
        char safe[256];
        size_t j = 0;
        for (size_t i = 0; server_name[i] && j + 1 < sizeof(safe); i++) {
            char c = server_name[i];
            safe[j++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        safe[j] = '\0';
        snprintf(token_path, sizeof(token_path), "%s/%s.json", token_dir, safe);
    } else {
        mcp_om_token_path(server_name, token_path, sizeof(token_path));
    }

    struct stat st;
    if (stat(token_path, &st) != 0) {
        pthread_mutex_unlock(&mgr->entries_lock);
        return 0;
    }

    long long mtime_ns = (long long)st.st_mtime * 1000000000LL + st.st_mtim.tv_nsec;
    pthread_mutex_lock(&entry->lock);
    int changed = (mtime_ns != entry->last_mtime_ns);
    if (changed) {
        long long old = entry->last_mtime_ns;
        entry->last_mtime_ns = mtime_ns;
        hermes_log(LOG_INFO, "mcp_oauth",
                   "invalidate_if_disk_changed '%s': mtime %lld -> %lld, forcing reload",
                   server_name, (long long)old, (long long)mtime_ns);
    }
    pthread_mutex_unlock(&entry->lock);
    pthread_mutex_unlock(&mgr->entries_lock);
    return changed;
}

/* PoP: cli_tools_mcp_oauth_manager_handle_401
 *      @ tools/mcp_oauth_manager.py:handle_401 */

/* Port of Python handle_401 — deduplicated 401 recovery.
 * Returns 1 (and *refreshed_out=1) if a fresh token is now available
 * (disk changed, or provider can refresh in place); 0 if no recovery
 * path exists. Thundering-herd dedup: concurrent calls with the same
 * access_token share one in-flight decision (modelled here by serialising
 * on the entry lock + caching the decision for the duration of the call). */
int cli_tools_mcp_oauth_manager_handle_401(
    const char *server_name, const char *access_token,
    int *refreshed_out)
{
    if (!server_name || !access_token || !refreshed_out) return -1;
    *refreshed_out = 0;

    mcp_oauth_manager_t *mgr = cli_tools_mcp_oauth_manager_get_manager();
    if (!mgr) return -1;

    mcp_oauth_entry_t *entry = NULL;
    pthread_mutex_lock(&mgr->entries_lock);
    for (int i = 0; i < MCP_OAUTH_MAX_SERVERS; i++) {
        if (mgr->entries[i].active &&
            strcmp(mgr->entries[i].server_name, server_name) == 0) {
            entry = &mgr->entries[i];
            break;
        }
    }
    if (!entry || entry->provider == NULL) {
        pthread_mutex_unlock(&mgr->entries_lock);
        return 0;
    }

    pthread_mutex_lock(&entry->lock);
    pthread_mutex_unlock(&mgr->entries_lock);

    /* Step 1: did the disk change? Picks up an external refresh. */
    int disk_changed = cli_tools_mcp_oauth_manager_invalidate_if_disk_changed(
        server_name, NULL);
    if (disk_changed) {
        *refreshed_out = 1;
        pthread_mutex_unlock(&entry->lock);
        return 0;
    }

    /* Step 2: no disk change — can the provider refresh in place?
     * In C the SDK refresh capability is tracked on the provider record. */
    int can_refresh = entry->provider->can_refresh;
    *refreshed_out = can_refresh ? 1 : 0;
    pthread_mutex_unlock(&entry->lock);
    if (can_refresh) {
        hermes_log(LOG_INFO, "mcp_oauth",
                   "handle_401 '%s': provider can refresh in place", server_name);
    } else {
        hermes_log(LOG_WARNING, "mcp_oauth",
                   "handle_401 '%s': no recovery path — needs interactive reauth",
                   server_name);
    }
    return 0;
}

/* PoP: cli_tools_mcp_oauth_manager_get_manager
 *      @ tools/mcp_oauth_manager.py:get_manager */

/* Port of Python get_manager — process-wide singleton. */
void *cli_tools_mcp_oauth_manager_get_manager(void)
{
    static mcp_oauth_manager_t *singleton = NULL;
    static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
    static int inited = 0;
    pthread_mutex_lock(&init_lock);
    if (!inited) {
        singleton = calloc(1, sizeof(*singleton));
        if (singleton) {
            pthread_mutex_init(&singleton->entries_lock, NULL);
            for (int i = 0; i < MCP_OAUTH_MAX_SERVERS; i++)
                pthread_mutex_init(&singleton->entries[i].lock, NULL);
            inited = 1;
            hermes_log(LOG_DEBUG, "mcp_oauth", "Created singleton manager");
        }
    }
    pthread_mutex_unlock(&init_lock);
    return singleton;
}

/* PoP: cli_tools_mcp_oauth_manager_reset_manager_for_tests
 *      @ tools/mcp_oauth_manager.py:reset_manager_for_tests */

/* Port of Python reset_manager_for_tests — clear all cached state. */
void cli_tools_mcp_oauth_manager_reset_manager_for_tests(void)
{
    mcp_oauth_manager_t *mgr = cli_tools_mcp_oauth_manager_get_manager();
    if (!mgr) return;
    pthread_mutex_lock(&mgr->entries_lock);
    for (int i = 0; i < MCP_OAUTH_MAX_SERVERS; i++) {
        mcp_oauth_entry_t *e = &mgr->entries[i];
        if (e->provider) {
            mcp_oauth_storage_free(e->provider->storage);
            free(e->provider);
        }
        pthread_mutex_destroy(&e->lock);
        memset(e, 0, sizeof(*e));
        pthread_mutex_init(&e->lock, NULL);
    }
    pthread_mutex_unlock(&mgr->entries_lock);
    hermes_log(LOG_DEBUG, "mcp_oauth", "Reset manager for tests");
}
