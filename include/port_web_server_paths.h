/*
 * port_web_server_paths.h — Declarations for shared web-server path helpers.
 *
 * These functions implement logic ported from:
 *   cron/scheduler.py:_get_hermes_home
 *   tools/tirith_security.py:_get_hermes_home
 *   hermes_cli/config.py:_is_container
 *   hermes_cli/config.py:detect_install_method
 *
 * Keeping them in a dedicated TU avoids bloating the web-server orchestrator
 * and lets any other subsystem reuse them without circular includes.
 */
#ifndef PORT_WEB_SERVER_PATHS_H
#define PORT_WEB_SERVER_PATHS_H

#include <stdbool.h>

const char *get_hermes_home(void);
bool is_container(void);
const char *detect_install_method(const char *project_root);

#endif /* PORT_WEB_SERVER_PATHS_H */
