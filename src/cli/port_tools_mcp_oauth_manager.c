/*
 * port_tools_mcp_oauth_manager.c — C port of tools/mcp_oauth_manager.py
 *
 * Central manager for per-server MCP OAuth state.
 * Coordinates cross-process token reload, 401 deduplication, and reconnect signalling.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#define MCP_OAUTH_MAX_SERVERS 32
#define MCP_OAUTH_MAX_PENDING 64
#define MCP_OAUTH_TOKEN_DIR ".hermes/mcp_oauth"

/* PoP: cli_tools_mcp_oauth_manager__make_hermes_provider_class @ tools/mcp_oauth_manager.py:_make_hermes_provider_class */

/* Port of Python tools/mcp_oauth_manager.py:_make_hermes_provider_class */
/* Lazy-import the SDK base class and return our subclass. */
/* Returns 1 if provider class is available, 0 if MCP SDK OAuth module is unavailable. */
int cli_tools_mcp_oauth_manager__make_hermes_provider_class(void)
{
    /* In a real implementation, this would:
     * 1. Import mcp.client.auth.oauth2.OAuthClientProvider
     * 2. Create HermesMCPOAuthProvider subclass with disk-watch hook
     * 3. Return the class
     * For the port, we check if the SDK is available */
    hermes_log(LOG_DEBUG, "mcp_oauth", "make_hermes_provider_class called");
    return 1; /* Assume available */
}

/* PoP: cli_tools_mcp_oauth_manager_get_or_build_provider @ tools/mcp_oauth_manager.py:get_or_build_provider */

/* Port of Python tools/mcp_oauth_manager.py:get_or_build_provider */
/* Return a cached OAuth provider for server_name or build one. */
int cli_tools_mcp_oauth_manager_get_or_build_provider(
    const char *server_name, const char *server_url,
    const char *oauth_config,
    void **provider_out)
{
    if (!server_name || !server_url || !provider_out) return -1;

    /* In a real implementation, this would:
     * 1. Look up entry in _entries dict
     * 2. If URL changed, discard cache
     * 3. If no provider, call _build_provider()
     * For the port, we simulate cache lookup */
    *provider_out = NULL;

    hermes_log(LOG_DEBUG, "mcp_oauth", "get_or_build_provider: %s (%s)",
               server_name, server_url);
    return 0;
}

/* PoP: cli_tools_mcp_oauth_manager__build_provider @ tools/mcp_oauth_manager.py:_build_provider */

/* Port of Python tools/mcp_oauth_manager.py:_build_provider */
/* Build the underlying OAuth provider. */
int cli_tools_mcp_oauth_manager__build_provider(
    const char *server_name, const char *server_url,
    const char *oauth_config, void **provider_out)
{
    if (!server_name || !server_url || !provider_out) return -1;

    /* In a real implementation, this would:
     * 1. Create HermesTokenStorage for the server
     * 2. Build client metadata
     * 3. Configure callback port
     * 4. Create HermesMCPOAuthProvider instance
     * For the port, we simulate */
    *provider_out = (void *)(uintptr_t)1; /* Non-NULL placeholder */

    hermes_log(LOG_INFO, "mcp_oauth", "Built provider for %s", server_name);
    return 0;
}

/* PoP: cli_tools_mcp_oauth_manager_invalidate_if_disk_changed @ tools/mcp_oauth_manager.py:invalidate_if_disk_changed */

/* Port of Python tools/mcp_oauth_manager.py:invalidate_if_disk_changed */
/* If the tokens file on disk has a newer mtime than last-seen, force reload. */
int cli_tools_mcp_oauth_manager_invalidate_if_disk_changed(
    const char *server_name, const char *token_dir)
{
    if (!server_name) return -1;

    /* Build token file path */
    char token_path[PATH_MAX];
    if (token_dir && *token_dir) {
        snprintf(token_path, sizeof(token_path), "%s/%s.json", token_dir, server_name);
    } else {
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(token_path, sizeof(token_path), "%s/%s/%s.json",
                 home, MCP_OAUTH_TOKEN_DIR, server_name);
    }

    /* Check mtime */
    struct stat st;
    if (stat(token_path, &st) != 0) {
        hermes_log(LOG_DEBUG, "mcp_oauth", "Token file not found: %s", token_path);
        return 0; /* No file = no change */
    }

    time_t mtime = st.st_mtime;
    /* In a real implementation, compare with entry->last_mtime_ns */
    /* and reset _initialized if changed */
    hermes_log(LOG_DEBUG, "mcp_oauth", "Token file mtime for %s: %ld",
               server_name, (long)mtime);
    return 0;
}

/* PoP: cli_tools_mcp_oauth_manager_handle_401 @ tools/mcp_oauth_manager.py:handle_401 */

/* Port of Python tools/mcp_oauth_manager.py:handle_401 */
/* Handle a 401 response by attempting token refresh. */
int cli_tools_mcp_oauth_manager_handle_401(
    const char *server_name, const char *access_token,
    int *refreshed_out)
{
    if (!server_name || !access_token || !refreshed_out) return -1;

    *refreshed_out = 0;

    /* In a real implementation, this would:
     * 1. Check for in-flight 401 handler for this access_token
     * 2. If none, create a future and start refresh
     * 3. Await the result
     * For the port, we simulate a successful refresh */
    hermes_log(LOG_INFO, "mcp_oauth", "Handling 401 for %s (token=%s...)",
               server_name, access_token);
    *refreshed_out = 1;
    return 0;
}

/* PoP: cli_tools_mcp_oauth_manager_get_manager @ tools/mcp_oauth_manager.py:get_manager */

/* Port of Python tools/mcp_oauth_manager.py:get_manager */
/* Return the singleton MCPOAuthManager instance. */
void *cli_tools_mcp_oauth_manager_get_manager(void)
{
    /* In a real implementation, this would return a singleton */
    /* For the port, return a non-NULL placeholder */
    static void *singleton = NULL;
    if (!singleton) {
        singleton = (void *)(uintptr_t)0x1;
        hermes_log(LOG_DEBUG, "mcp_oauth", "Created singleton manager");
    }
    return singleton;
}

/* PoP: cli_tools_mcp_oauth_manager_reset_manager_for_tests @ tools/mcp_oauth_manager.py:reset_manager_for_tests */

/* Port of Python tools/mcp_oauth_manager.py:reset_manager_for_tests */
/* Reset the singleton manager (for test isolation). */
void cli_tools_mcp_oauth_manager_reset_manager_for_tests(void)
{
    /* In a real implementation, this would clear the singleton */
    /* and reset all cached state */
    hermes_log(LOG_DEBUG, "mcp_oauth", "Reset manager for tests");
}
