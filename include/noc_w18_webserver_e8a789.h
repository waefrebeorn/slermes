#ifndef NOC_W18_WEBSERVER_E8A789_H
#define NOC_W18_WEBSERVER_E8A789_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/web_server_boundary.py host/auth boundary primitives.
 * PoP: w18_webserver_e8a789_public_url_hosts @ hermes_cli/web_server_boundary.py:public_url_hosts
 * PoP: w18_webserver_e8a789_host_header_hostname @ hermes_cli/web_server_boundary.py:_host_header_hostname
 * PoP: w18_webserver_e8a789_is_accepted_host @ hermes_cli/web_server_boundary.py:_is_accepted_host
 * PoP: w18_webserver_e8a789_should_require_auth @ hermes_cli/web_server_boundary.py:should_require_auth
 * PoP: w18_webserver_e8a789_public_hosts_engage_gate @ hermes_cli/web_server_boundary.py:public_hosts_engage_gate
 * PoP: w18_webserver_e8a789_loopback_origin @ hermes_cli/web_server_boundary.py:_loopback_origin
 * PoP: w18_webserver_e8a789_public_url_origins @ hermes_cli/web_server_boundary.py:public_url_origins
 */

/* -> sorted JSON array of lowercase hostname strings (empty when unset/malformed). */
json_t *w18_webserver_e8a789_public_url_hosts(const char *public_url);

/* -> normalized lowercase hostname from an HTTP Host authority, or "" when invalid. */
const char *w18_webserver_e8a789_host_header_hostname(const char *host_header);

/* -> 1 when the Host header targets the bound interface, else 0. */
int w18_webserver_e8a789_is_accepted_host(const char *host_header, const char *bound_host,
                                          const json_t *trusted_public_hosts);

/* -> 1 iff any non-loopback bind requires the auth gate (allow_public is ignored). */
int w18_webserver_e8a789_should_require_auth(const char *host, int allow_public);

/* -> 1 iff a non-loopback public host engages the gate. */
int w18_webserver_e8a789_public_hosts_engage_gate(const json_t *trusted_public_hosts);

/* -> ["scheme","host",port] for an http(s) loopback origin (default port filled), else JSON null. */
json_t *w18_webserver_e8a789_loopback_origin(const char *origin);

/* -> sorted JSON array of loopback origins a public_url declares (empty when unset/malformed). */
json_t *w18_webserver_e8a789_public_url_origins(const char *public_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_WEBSERVER_E8A789_H */
