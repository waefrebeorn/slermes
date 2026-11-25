/*
 * noc_w34_webserverl_f96a1a.h - C surface for upstream
 * hermes_cli/web_server_lifecycle.py (gate w34_webserverl_f96a1a).
 *
 * The pure decision core of the dashboard/gateway serve-process lifecycle:
 * parent-start-marker predicates and the orphan decision they feed, the
 * parent-death-watchdog arming plan, the port-conflict preflight derivation
 * with its machine sentinel and human hint, the bounded trusted-proxy
 * allow-list, and the dashboard URL the browser opener would be handed.
 * Every process/socket/file/log seam (PIDs, start-marker probes, sockets,
 * ready files, signals, uvicorn) stays in the caller; this file owns only what
 * the values decide.
 */
#ifndef NOC_W34_WEBSERVERL_F96A1A_H
#define NOC_W34_WEBSERVERL_F96A1A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module-level literals: the transient-failure exit code, the orphan unwind
 * ceiling and the loopback proxy defaults. */
#define W34_WEBSERVERL_F96A1A_PORT_IN_USE_EXIT_CODE 75
#define W34_WEBSERVERL_F96A1A_ORPHAN_EXIT_CEILING_S 10.0

/* -> {"port_in_use_exit_code", "orphan_exit_ceiling_s", "sentinel",
 *     "default_forwarded_allow_ips": [...]} */
json_t *w34_webserverl_f96a1a_constants(void);

/* {"marker": <str>} -> bool.  A usable cross-runtime parent marker:
 * "<linux|win|winms>:<digits>" or a full "ps:<lstart>" rendering. */
json_t *w34_webserverl_f96a1a_valid_parent_start_marker(const json_t *req);

/* {"actual", "expected"} -> bool.  Same Desktop incarnation, including the
 * win:/winms: generation bridge (FILETIME ticks vs Unix ms). */
json_t *w34_webserverl_f96a1a_parent_start_markers_match(const json_t *req);

/* {"actual", "expected"} -> bool.  A mismatch proves PID reuse, except for the
 * TZ/locale-rendered "ps:" wall-clock marker, which is inconclusive. */
json_t *w34_webserverl_f96a1a_parent_start_marker_mismatch_is_conclusive(const json_t *req);

/* {"desktop_pid", "expected": {"kind":"marker","value":..}|absent,
 *  "probe": {"kind":"marker","value":..}|{"kind":"lookup"}|{"kind":"error"},
 *  "pid": {"kind":"bool","value":..}|{"kind":"error"}} -> bool.
 * The orphan decision; probe/pid_existence answers arrive in the request. */
json_t *w34_webserverl_f96a1a_serve_orphaned_decision(const json_t *req);

/* {"parent_pid", "start_marker", "nonce", "poll_env"} (nullable strings) ->
 * {"armed": bool, "desktop_pid": int|null, "poll": number|null}.  The env
 * parse that decides whether the parent-death watchdog may arm, and at what
 * poll interval. */
json_t *w34_webserverl_f96a1a_watchdog_plan(const json_t *req);

/* {"errno": number|null, "winerror": number|null} -> bool.  The platform's
 * address-in-use bind failure (98/48/10048 plus a WSAEADDRINUSE winerror). */
json_t *w34_webserverl_f96a1a_addr_in_use(const json_t *req);

/* {"host", "port", "errno"|null, "winerror"|null} ->
 * {"conflict": bool, "family": "AF_INET6"|"AF_INET"|null}.  Port 0 never
 * conflicts and is never probed. */
json_t *w34_webserverl_f96a1a_port_preflight(const json_t *req);

/* {"port"} -> "BACKEND_PORT_IN_USE port=<port>". */
json_t *w34_webserverl_f96a1a_port_in_use_sentinel(const json_t *req);

/* {"host", "port"} -> the human hint line printed after the sentinel. */
json_t *w34_webserverl_f96a1a_port_in_use_hint(const json_t *req);

/* {"config": {..}|null} -> ["127.0.0.1", "::1", ...valid config entries].
 * dashboard.trusted_proxies, filtered: loopback always, then every bounded IP
 * or CIDR network; "*" and /0 fail closed. */
json_t *w34_webserverl_f96a1a_forwarded_allow_ips(const json_t *req);

/* {"host", "port", "profile", "open_browser", "platform", "display",
 *  "wayland"} -> the URL the opener thread would open, or null when the open
 * is skipped (not requested, or headless Linux). */
json_t *w34_webserverl_f96a1a_browser_open_url(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
