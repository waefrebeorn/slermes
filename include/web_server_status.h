/*
 * web_server_status.h — operational backbone of the Hermes dashboard
 * (faithful C11 port of the status/health/session/lifecycle surface in
 * hermes_cli/web_server.py).
 *
 * This is the HEAVY dependency layer that the dashboard status page and the
 * rest of web_server.py's routes sit on. It is implemented with REAL behavior
 * (no stubs):
 *   - gateway health probe  → real HTTP GET via libhttp
 *   - active-session count  → real read-only SQLite query against state.db
 *   - error ring            → real bounded in-memory ring
 *   - runtime state         → real opaque dashboard_runtime_t (Python's
 *                             app.state: event_channels, pty session files,
 *                             chat-argv lock, error ring)
 *   - desktop cron ticker   → real periodic scheduler_run_job firing
 *
 * Reuses existing slermes plumbing: hermes_http (http_new/http_get),
 * slermes_home() + SLERMES_FILE_STATE_DB + sqlite3, and the cron scheduler
 * runtime (scheduler_run_job) finished earlier in this port.
 */

#ifndef WEB_SERVER_STATUS_H
#define WEB_SERVER_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── runtime state (Python app.state) ─────────────────────────────────── */

typedef struct dashboard_runtime_t dashboard_runtime_t;

/* Create/lazy-init the process-wide dashboard runtime context. Returns a
 * stable pointer (never freed before process exit). Mirrors the FastAPI
 * lifespan initialising app.state. */
dashboard_runtime_t *dashboard_runtime_get(void);

/* Accessors (Python _get_event_state / _get_pty_active_session_files). */
void dashboard_runtime_lock(dashboard_runtime_t *rt);
void dashboard_runtime_unlock(dashboard_runtime_t *rt);

/* PTY active-session-file map: channel -> absolute session-file path.
 * The map is owned by the runtime; callers must hold the runtime lock while
 * reading. Returns the stored path for `channel` or NULL. */
void dashboard_runtime_set_pty_file(dashboard_runtime_t *rt,
                                    const char *channel,
                                    const char *abs_path);
const char *dashboard_runtime_get_pty_file(dashboard_runtime_t *rt,
                                           const char *channel);

/* ── gateway health probe (Python _probe_gateway_health) ───────────────── */
/* Probes GATEWAY_HEALTH_URL (env). Normalises to a base URL and tries
 * /health/detailed then /health. On success returns true and fills
 * `out_body` (malloc'd JSON, caller frees) with the response body. On any
 * failure returns false and sets *out_body = NULL. */
bool ws_probe_gateway_health(char **out_body);

/* ── active session count (Python _count_status_active_sessions) ───────── */
/* Counts sessions in state.db where ended_at IS NULL and
 * last_active >= now - 300s. Returns 0 if state.db is absent or unreadable
 * (best-effort status garnish). Opens read-only. */
int ws_count_active_sessions(void);

/* Async-style wrapper (Python _status_active_sessions): C is synchronous,
 * so this just calls ws_count_active_sessions() and never blocks. */
int ws_status_active_sessions(void);

/* ── error ring (Python record_error / recent_error_count) ────────────── */
#define WS_ERROR_RING_MAX 50
void ws_record_error(const char *component, const char *message);
int ws_recent_error_count(int window_seconds);
int ws_recent_error_count_all(void);

/* ── dashboard self-test / health (Python _dashboard_selftest_once /
 *     _dashboard_health_middleware) ──────────────────────────────────── */
/* Runs a single best-effort self-test: probes gateway health and records
 * outcomes in the error ring. Returns true if the dashboard's own surface is
 * healthy (gateway probe did not error, or no gateway configured). */
bool ws_dashboard_selftest_once(void);

/* True if the request's Host header is acceptable for `bound_host`
 * (DNS-rebinding defence). Reuses the auth helper. */
bool ws_is_accepted_host(const char *host_header, const char *bound_host);

/* ── lifecycle (Python _lifespan / _start_desktop_cron_ticker) ────────── */
/* Start the desktop cron ticker (HERMES_DESKTOP=1). Fires the real cron
 * scheduler every `interval` seconds until stopped. Returns false if not in
 * desktop mode or if the ticker is already running. */
bool ws_start_desktop_cron_ticker(int interval_seconds);
void ws_stop_desktop_cron_ticker(void);
bool ws_desktop_cron_ticker_running(void);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_STATUS_H */
