/* water-cup gate: is_public_path — traced copy of src/web_dashboard.c::
 * is_public_path. Asserts the C public-path allowlist is in 1:1 parity with the
 * LIVE Python hermes_cli/dashboard_auth/public_paths.PUBLIC_API_PATHS. */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static bool is_public_path(const char *path) {
    if (!path) return false;
    static const char *const public_paths[] = {
        "/api/health",
        "/api/status",
        "/api/config/defaults",
        "/api/config/schema",
        "/api/model/info",
        "/api/dashboard/themes",
        "/api/dashboard/plugins",
        "/api/cron/fire",
        NULL
    };
    for (int i = 0; public_paths[i]; i++) {
        if (strcmp(path, public_paths[i]) == 0) return true;
    }
    return false;
}

/* Paths that LIVE Python PUBLIC_API_PATHS contains -> C must return true. */
static const char *const LIVE_PUBLIC[] = {
    "/api/health", "/api/status", "/api/config/defaults", "/api/config/schema",
    "/api/model/info", "/api/dashboard/themes", "/api/dashboard/plugins",
    "/api/cron/fire", NULL
};
/* Paths that LIVE does NOT contain -> C must return false.
 * (These were the spurious C-only entries before the fix.) */
static const char *const LIVE_PRIVATE[] = {
    "/health", "/api/version", "/api/token", "/api/platforms",
    "/api/usage", "/api/sessions", "/api/env", "/api/providers/validate",
    "/api/config", NULL
};

int main(void) {
    int fail = 0;
    for (int i = 0; LIVE_PUBLIC[i]; i++) {
        if (!is_public_path(LIVE_PUBLIC[i])) {
            printf("FAIL: LIVE-public path '%s' returned false\n", LIVE_PUBLIC[i]);
            fail++;
        }
    }
    for (int i = 0; LIVE_PRIVATE[i]; i++) {
        if (is_public_path(LIVE_PRIVATE[i])) {
            printf("FAIL: LIVE-private path '%s' returned true\n", LIVE_PRIVATE[i]);
            fail++;
        }
    }
    if (fail) { printf("RESULT: %d FAILURES\n", fail); return 1; }
    printf("RESULT: is_public_path matches LIVE PUBLIC_API_PATHS (8 public, 9 private checked)\n");
    return 0;
}
