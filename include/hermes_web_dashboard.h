#ifndef HERMES_WEB_DASHBOARD_H
#define HERMES_WEB_DASHBOARD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the web dashboard server.
 * Reads config from env vars (DASHBOARD_HOST, DASHBOARD_PORT, HERMES_WEB_DIST).
 */
void dashboard_init(void);

/**
 * Start the web dashboard server in a background thread.
 * Returns true on success.
 */
bool dashboard_start(void);

/**
 * Stop the web dashboard server.
 */
void dashboard_stop(void);

/**
 * Check if the dashboard server is running.
 */
bool dashboard_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WEB_DASHBOARD_H */
