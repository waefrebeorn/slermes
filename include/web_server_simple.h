/* web_server_simple.h — additional C11 ports from hermes_cli/web_server.py */

#ifndef HERMES_WEB_SERVER_SIMPLE_H
#define HERMES_WEB_SERVER_SIMPLE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── hermes_cli/web_server.py additional ports ─────────────────────── */

/* PoP: _fs_mime_type @ hermes_cli/web_server.py:_fs_mime_type
 * Return MIME type for a file path based on suffix, with fallback. */
const char *ws_fs_mime_type(const char *path);

/* PoP: _fs_looks_binary @ hermes_cli/web_server.py:_fs_looks_binary
 * Return True if data looks like binary (contains NUL or >12% control chars). */
bool ws_fs_looks_binary(const unsigned char *data, size_t len);

/* PoP: _git_path @ hermes_cli/web_server.py:_git_path
 * Return the fs_path string for a git path. */
char *ws_git_path(const char *path);

/* PoP: _is_addr_in_use_error @ hermes_cli/web_server.py:_is_addr_in_use_error
 * True when errno indicates an address-in-use bind failure. */
bool ws_is_addr_in_use_error(int errnum);

/* PoP: _port_bind_conflict @ hermes_cli/web_server.py:_port_bind_conflict
 * Probe whether binding host:port would fail with EADDRINUSE. */
bool ws_port_bind_conflict(const char *host, int port);

/* PoP: _durable_completed_update_action_id @ hermes_cli/web_server.py:_durable_completed_update_action_id
 * Recover the latest successful update identity from update.log lines. */
char *ws_durable_completed_update_action_id(const char **lines, size_t n_lines);

/* PoP: _report_port_in_use @ hermes_cli/web_server.py:_report_port_in_use
 * Print the port-in-use sentinel and human hint. */
void ws_report_port_in_use(const char *host, int port);

/* PoP: _dashboard_public_hosts @ hermes_cli/web_server.py:_dashboard_public_hosts
 * Return the hostname from dashboard.public_url (fail closed as empty string). */
char *ws_dashboard_public_hosts(void);

/* PoP: _read_or_create_install_id @ hermes_cli/web_server.py:_read_or_create_install_id
 * Read (or mint + persist) the install id under the root Hermes home. */
char *ws_read_or_create_install_id(void);

/* PoP: _latest_update_receipt_summary @ hermes_cli/web_server.py:_latest_update_receipt_summary
 * Compact summary of the most recent update receipt, or NULL. */
char *ws_latest_update_receipt_summary(void);

/* PoP: _is_other_profile @ hermes_cli/web_server.py:_is_other_profile
 * True when profile names a profile other than this process's own. */
bool ws_is_other_profile(const char *profile);

/* PoP: _approval_mode_of @ hermes_cli/web_server.py:_approval_mode_of
 * Normalize approvals.mode from an in-memory config document. */
const char *ws_approval_mode_of(const char *mode_val);

/* PoP: _host_header_hostname @ hermes_cli/web_server.py:_host_header_hostname
 * Return a normalized hostname from a valid HTTP Host authority. */
char *ws_host_header_hostname(const char *host_header);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_WEB_SERVER_SIMPLE_H */
