/*
 * dashboard_auth_public_paths.c — canonical C11 port of
 * hermes_cli/dashboard_auth/public_paths.py.
 *
 * Shared allowlist of /api/* paths that bypass dashboard auth. Both auth
 * middlewares (loopback/_SESSION_TOKEN gate and OAuth-cookie gate) consult
 * this single set — centralised so they cannot drift (the drift broke the
 * portal's wildcard liveness probe once already; see the Python docstring).
 */

#include <stdbool.h>
#include <string.h>

/* PoP: dapaths_is_public @ hermes_cli/dashboard_auth/public_paths.py:PUBLIC_API_PATHS */
bool dapaths_is_public(const char *path) {
    static const char *const PUBLIC_API_PATHS[] = {
        "/api/health",            /* boot-handshake liveness probe */
        "/api/status",            /* portal wildcard liveness probe */
        "/api/config/defaults",   /* SPA Config page feeds (read-only) */
        "/api/config/schema",
        "/api/model/info",        /* read-only model metadata */
        "/api/dashboard/themes",  /* skin + plugin manifests */
        "/api/dashboard/plugins",
        "/api/cron/fire",         /* Chronos webhook; JWT is the boundary */
        NULL
    };
    if (!path) return false;
    for (int i = 0; PUBLIC_API_PATHS[i]; i++)
        if (strcmp(path, PUBLIC_API_PATHS[i]) == 0) return true;
    return false;
}
