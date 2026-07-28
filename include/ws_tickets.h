/*
 * ws_tickets.h — WS-upgrade auth credentials for gated mode
 * (C port of hermes_cli/dashboard_auth/ws_tickets.py, impl in
 * src/cli/hermes_cli_ws_tickets.c).
 *
 * Single-use browser tickets (mint/consume, 30s TTL) and the
 * process-lifetime internal credential for server-spawned WS clients.
 * All returned strings are malloc'd; caller frees.
 */
#ifndef WS_TICKETS_H
#define WS_TICKETS_H

/* mint_ticket: one-shot ticket bound to a user identity. */
char *ws_tickets_mint_ticket(const char *user_id, const char *provider);

/* consume_ticket: returns user_id on success, NULL on missing/expired/used. */
char *ws_tickets_consume_ticket(const char *ticket);

/* internal_ws_credential: process-lifetime credential, minted once. */
char *ws_tickets_internal_ws_credential(void);

/* consume_internal_credential: "server-internal" on success, NULL on failure. */
char *ws_tickets_consume_internal_credential(const char *value);

/* _gc_expired_locked: drop expired tickets. */
void ws_tickets__gc_expired_locked(void);

#endif /* WS_TICKETS_H */
