/*
 * hermes_cli_ws_tickets.c — C port of hermes_cli/dashboard_auth/ws_tickets.py
 *
 * WS-upgrade auth credentials for gated mode.
 * Single-use browser tickets and process-lifetime internal credentials.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TTL_SECONDS 30
#define MAX_TICKETS 256

typedef struct {
    char ticket[128];
    int expires_at;
    char user_id[64];
    char provider[64];
} TicketEntry;

static TicketEntry _tickets[MAX_TICKETS];
static int _ticket_count = 0;
static char _internal_credential[128] = {0};
static int _internal_set = 0;

/* PoP: ws_tickets_mint_ticket @ hermes_cli/dashboard_auth/ws_tickets.py:mint_ticket */

/* Port of Python hermes_cli/dashboard_auth/ws_tickets.py:mint_ticket */
/* Generate a one-shot ticket bound to this user identity. */
char *ws_tickets_mint_ticket(const char *user_id, const char *provider)
{
    if (!user_id || !user_id[0]) user_id = "unknown";
    if (!provider || !provider[0]) provider = "unknown";

    /* Generate a simple random ticket (32 hex chars) */
    char ticket[65];
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        ticket[i] = hex[rand() % 16];
    }
    ticket[32] = '\0';

    /* Store ticket */
    if (_ticket_count < MAX_TICKETS) {
        TicketEntry *e = &_tickets[_ticket_count++];
        strncpy(e->ticket, ticket, sizeof(e->ticket) - 1);
        e->ticket[sizeof(e->ticket) - 1] = '\0';
        e->expires_at = (int)time(NULL) + TTL_SECONDS;
        strncpy(e->user_id, user_id, sizeof(e->user_id) - 1);
        e->user_id[sizeof(e->user_id) - 1] = '\0';
        strncpy(e->provider, provider, sizeof(e->provider) - 1);
        e->provider[sizeof(e->provider) - 1] = '\0';
    }

    /* GC expired */
    int now = (int)time(NULL);
    int write = 0;
    for (int i = 0; i < _ticket_count; i++) {
        if (_tickets[i].expires_at >= now) {
            if (write != i) _tickets[write] = _tickets[i];
            write++;
        }
    }
    _ticket_count = write;

    hermes_log(LOG_DEBUG, "ws_tickets", "Minted ticket for user=%s provider=%s", user_id, provider);
    return strdup(ticket);
}

/* PoP: ws_tickets_consume_ticket @ hermes_cli/dashboard_auth/ws_tickets.py:consume_ticket */

/* Port of Python hermes_cli/dashboard_auth/ws_tickets.py:consume_ticket */
/* Validate and consume a ticket. Returns user_id on success, NULL on failure. */
char *ws_tickets_consume_ticket(const char *ticket)
{
    if (!ticket || !ticket[0]) {
        hermes_log(LOG_WARNING, "ws_tickets", "consume_ticket: empty ticket");
        return NULL;
    }

    int now = (int)time(NULL);
    for (int i = 0; i < _ticket_count; i++) {
        if (strcmp(_tickets[i].ticket, ticket) == 0) {
            if (_tickets[i].expires_at < now) {
                hermes_log(LOG_WARNING, "ws_tickets", "consume_ticket: expired ticket");
                /* Remove expired ticket */
                for (int j = i; j < _ticket_count - 1; j++) {
                    _tickets[j] = _tickets[j + 1];
                }
                _ticket_count--;
                return NULL;
            }
            /* Valid ticket — consume it (remove from store) */
            char *user_id = strdup(_tickets[i].user_id);
            for (int j = i; j < _ticket_count - 1; j++) {
                _tickets[j] = _tickets[j + 1];
            }
            _ticket_count--;
            hermes_log(LOG_DEBUG, "ws_tickets", "Consumed ticket for user=%s", user_id);
            return user_id;
        }
    }

    hermes_log(LOG_WARNING, "ws_tickets", "consume_ticket: unknown ticket");
    return NULL;
}

/* PoP: ws_tickets__gc_expired_locked @ hermes_cli/dashboard_auth/ws_tickets.py:_gc_expired_locked */

/* Port of Python hermes_cli/dashboard_auth/ws_tickets.py:_gc_expired_locked */
/* Drop expired tickets. */
void ws_tickets__gc_expired_locked(void)
{
    int now = (int)time(NULL);
    int write = 0;
    int removed = 0;
    for (int i = 0; i < _ticket_count; i++) {
        if (_tickets[i].expires_at >= now) {
            if (write != i) _tickets[write] = _tickets[i];
            write++;
        } else {
            removed++;
        }
    }
    _ticket_count = write;
    if (removed > 0) {
        hermes_log(LOG_DEBUG, "ws_tickets", "GC'd %d expired tickets", removed);
    }
}

/* PoP: ws_tickets_internal_ws_credential @ hermes_cli/dashboard_auth/ws_tickets.py:internal_ws_credential */

/* Port of Python hermes_cli/dashboard_auth/ws_tickets.py:internal_ws_credential */
/* Return the process-lifetime internal WS credential, minting it once. */
char *ws_tickets_internal_ws_credential(void)
{
    if (!_internal_set) {
        /* Generate a random 32-hex-char credential */
        const char *hex = "0123456789abcdef";
        for (int i = 0; i < 32; i++) {
            _internal_credential[i] = hex[rand() % 16];
        }
        _internal_credential[32] = '\0';
        _internal_set = 1;
        hermes_log(LOG_INFO, "ws_tickets", "Minted internal WS credential");
    }
    return strdup(_internal_credential);
}

/* PoP: ws_tickets_consume_internal_credential @ hermes_cli/dashboard_auth/ws_tickets.py:consume_internal_credential */

/* Port of Python hermes_cli/dashboard_auth/ws_tickets.py:consume_internal_credential */
/* Validate an internal credential. Returns "server-internal" on success, NULL on failure. */
char *ws_tickets_consume_internal_credential(const char *value)
{
    if (!value || !value[0]) {
        hermes_log(LOG_WARNING, "ws_tickets", "consume_internal_credential: empty value");
        return NULL;
    }
    if (!_internal_set) {
        hermes_log(LOG_WARNING, "ws_tickets", "consume_internal_credential: no credential minted");
        return NULL;
    }
    if (strcmp(value, _internal_credential) != 0) {
        hermes_log(LOG_WARNING, "ws_tickets", "consume_internal_credential: mismatch");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "ws_tickets", "Internal credential validated successfully");
    return strdup("server-internal");
}
