/*
 * web_server_events.h — chat-tab event broadcast + PTY channel plumbing of
 * the Hermes dashboard (faithful C11 port of the /api/pub + /api/events +
 * /api/pty support layer in hermes_cli/web_server.py).
 *
 * Covers: _ws_close_reason, _resolve_client_ws_host, _build_gateway_ws_url,
 * _build_sidecar_url, _broadcast_event (subscriber registry),
 * _active_session_file_for_channel, _forget_active_session_file,
 * _render_active_theme_bootstrap_css's _esc, plus the channel registry
 * (subscribe/unsubscribe/publisher lifecycle with auto-evict).
 */

#ifndef WEB_SERVER_EVENTS_H
#define WEB_SERVER_EVENTS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── WS close-reason clamp (RFC 6455 123-byte cap) ──────────────────────── */

/* Python _ws_close_reason: clamp to 123 UTF-8 bytes; if longer, keep the
 * first 120 bytes (dropping any torn multibyte tail like Python's
 * errors="ignore") and append "...". Returns malloc'd string. */
char *ws_events_close_reason(const char *text);

/* ── client host / URL builders ─────────────────────────────────────────── */

/* Python _resolve_client_ws_host: HERMES_DASHBOARD_WS_HOST env wins; else
 * bound_host (wildcards 0.0.0.0/:: become 127.0.0.1); NULL when no host.
 * Returns malloc'd string or NULL. */
char *ws_events_resolve_client_host(const char *bound_host);

/* Python _build_gateway_ws_url: ws://<netloc>/api/ws?token=… (or
 * ?internal=… in gated mode). IPv6 hosts get bracketed. Returns malloc'd
 * URL or NULL when host/port unbound. */
char *ws_events_build_gateway_ws_url(const char *bound_host, int bound_port,
                                     bool auth_required,
                                     const char *session_token,
                                     const char *internal_credential);

/* Python _build_sidecar_url: ws://<netloc>/api/pub?token=…&channel=… (or
 * ?internal=…&channel=…). Channel is URL-encoded. Returns malloc'd URL or
 * NULL when host/port unbound. */
char *ws_events_build_sidecar_url(const char *bound_host, int bound_port,
                                  bool auth_required,
                                  const char *session_token,
                                  const char *internal_credential,
                                  const char *channel);

/* ── channel registry (Python event_channels dict + evict rules) ────────── */

typedef struct ws_event_registry ws_event_registry_t;

/* Callback a subscriber receives on publish. Return false if the send
 * failed (subscriber is auto-dropped, mirroring the finally-clause evict). */
typedef bool (*ws_event_send_fn)(void *sub_ctx, const char *payload);

ws_event_registry_t *ws_event_registry_new(void);
void ws_event_registry_free(ws_event_registry_t *reg);

/* /api/events subscribe: register a subscriber on a channel. Returns a
 * subscriber id (>=0) or -1 on invalid channel. */
int ws_event_subscribe(ws_event_registry_t *reg, const char *channel,
                       ws_event_send_fn send, void *sub_ctx);

/* /api/events finally-clause: remove the subscriber; channel auto-evicts
 * when empty AND its publisher already disconnected. */
void ws_event_unsubscribe(ws_event_registry_t *reg, const char *channel,
                          int sub_id);

/* /api/pub connect/disconnect: publisher lifecycle. Disconnect evicts the
 * channel when no subscribers remain. */
void ws_event_publisher_connect(ws_event_registry_t *reg, const char *channel);
void ws_event_publisher_disconnect(ws_event_registry_t *reg,
                                   const char *channel);

/* Python _broadcast_event: fan one payload out to every subscriber on the
 * channel. Failed sends drop the subscriber. Returns delivered count. */
int ws_event_broadcast(ws_event_registry_t *reg, const char *channel,
                       const char *payload);

/* Introspection (dashboard health): number of live channels / subscribers. */
size_t ws_event_channel_count(const ws_event_registry_t *reg);
size_t ws_event_subscriber_count(const ws_event_registry_t *reg,
                                 const char *channel);

/* ── per-channel active-session breadcrumb files ────────────────────────── */

typedef struct ws_session_files ws_session_files_t;

ws_session_files_t *ws_session_files_new(void);
void ws_session_files_free(ws_session_files_t *sf);

/* Python _active_session_file_for_channel: return (creating on first use)
 * the per-channel tempfile path where the TUI writes its active session id.
 * Returns malloc'd path or NULL on error. */
char *ws_session_file_for_channel(ws_session_files_t *sf, const char *channel);

/* Python _forget_active_session_file: unlink (missing ok) + drop mapping. */
void ws_session_file_forget(ws_session_files_t *sf, const char *channel);

/* ── theme bootstrap CSS escape ─────────────────────────────────────────── */

/* Python _esc inside _render_active_theme_bootstrap_css: replace "</" with
 * "<\\/" so user themes can't close the <style> block. Malloc'd. */
char *ws_events_theme_css_esc(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_EVENTS_H */
