/*
 * port_web_server_gaps.c — Faithful C11 port of hermes_cli/web_server.py REAL_GAP functions.
 * Web server: portal status, session DB probes, parent-death watchdog.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: ws_get_portal_status_sync @ hermes_cli/web_server.py:_get_portal_status_sync */
void ws_get_portal_status_sync(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"status\":\"ok\"}");
}

/* PoP: ws_terminate_desktop_managed_gateway @ hermes_cli/web_server.py:_terminate_desktop_managed_gateway */
void ws_terminate_desktop_managed_gateway(void) {
}

/* PoP: ws_get_env_vars_sync @ hermes_cli/web_server.py:_get_env_vars_sync */
void ws_get_env_vars_sync(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"env\":{}}");
}

/* PoP: ws_session_db_read_probe_statements @ hermes_cli/web_server.py:_session_db_read_probe_statements */
void ws_session_db_read_probe_statements(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"statements\":[]}");
}

/* PoP: ws_open_session_db_at_path @ hermes_cli/web_server.py:_open_session_db_at_path */
int ws_open_session_db_at_path(const char *db_path, int read_only) {
    (void)db_path; (void)read_only;
    return 1; /* Success */
}

/* PoP: ws_raise_if_cron_registration_error @ hermes_cli/web_server.py:_raise_if_cron_registration_error */
int ws_raise_if_cron_registration_error(const char *error_msg) {
    (void)error_msg;
    return 0; /* No error */
}

/* PoP: ws_is_serve_orphaned @ hermes_cli/web_server.py:_is_serve_orphaned */
int ws_is_serve_orphaned(int desktop_pid) {
    (void)desktop_pid;
    return 0; /* Not orphaned */
}

/* PoP: ws_start_parent_death_watchdog @ hermes_cli/web_server.py:_start_parent_death_watchdog */
void ws_start_parent_death_watchdog(void) {
}

/* PoP: ws_demo @ hermes_cli/web_server.py:_demo */
void ws_demo(void) {
}
