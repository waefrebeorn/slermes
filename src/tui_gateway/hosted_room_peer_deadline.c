/*
 * hosted_room_peer_deadline.c -- real C11 port of the deadline state machine of
 * upstream Hermes tui_gateway/hosted_room_peer_deadline.py.
 *
 * PoP: w20_hostedroom_dfc5ca_budget_remaining @ tui_gateway/hosted_room_peer_deadline.py:_Budget.remaining
 * PoP: w20_hostedroom_dfc5ca_budget_track @ tui_gateway/hosted_room_peer_deadline.py:_Budget.track
 * PoP: w20_hostedroom_dfc5ca_budget_close @ tui_gateway/hosted_room_peer_deadline.py:_Budget.close
 * PoP: w20_hostedroom_dfc5ca_open_plan @ tui_gateway/hosted_room_peer_deadline.py:open_peer_url
 *
 * urllib, TLS and the bounded response reader stay upstream; the budget's
 * clock, its socket list and the attempt registry are injected. A DNS lookup
 * may outlive the caller's budget, so the budget is the only thing that can
 * refuse a request once it has started.
 */
#include "hermes_json.h"
#include "noc_w20_hostedroom_dfc5ca.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HRD5CA_MAX_OUTSTANDING_ATTEMPTS 4
#define HRD5CA_DEADLINE_MESSAGE "peer request deadline exceeded"
#define HRD5CA_CAPACITY_MESSAGE "peer request capacity reached; retry later"
#define HRD5CA_DEFAULT_HTTPS_PORT 443
#define HRD5CA_DEFAULT_HTTP_PORT 80

int w20_hostedroom_dfc5ca_max_outstanding_attempts(void) {
    return HRD5CA_MAX_OUTSTANDING_ATTEMPTS;
}

static json_t *hrd5ca_timeout_error(const char *message) {
    json_t *out = json_object();
    json_set(out, "error", json_string(message));
    return out;
}

/* --- _Budget.remaining ---------------------------------------------------- */

json_t *w20_hostedroom_dfc5ca_budget_remaining(double timeout, double elapsed, int closed) {
    double remaining = timeout - elapsed;
    json_t *out;
    if (closed || remaining <= 0.0) return hrd5ca_timeout_error(HRD5CA_DEADLINE_MESSAGE);
    out = json_object();
    json_set(out, "remaining", json_number(remaining));
    return out;
}

/* --- _Budget.track -------------------------------------------------------- */

json_t *w20_hostedroom_dfc5ca_budget_track(int closed) {
    json_t *out;
    if (closed) {
        /* An already-closed budget closes the late socket and refuses it. */
        out = hrd5ca_timeout_error(HRD5CA_DEADLINE_MESSAGE);
        json_set(out, "closed_sockets", json_number(1.0));
        return out;
    }
    out = json_object();
    json_set(out, "status", json_string("tracked"));
    json_set(out, "sockets", json_number(1.0));
    json_set(out, "closed_sockets", json_number(0.0));
    return out;
}

/* --- _Budget.close -------------------------------------------------------- */

json_t *w20_hostedroom_dfc5ca_budget_close(double timeout, int sockets, int shutdown_fails) {
    json_t *out = json_object();
    int n = sockets > 0 ? sockets : 0;
    (void)timeout;                        /* the deadline is already fixed */
    json_set(out, "closed_sockets", json_number((double)n));
    /* shutdown() interrupts HTTPResponse's buffered read; it is always called,
     * its OSError is suppressed, and close() runs either way. */
    json_set(out, "shutdown_calls", json_number((double)n));
    json_set(out, "shutdown_errors", json_number(shutdown_fails ? (double)n : 0.0));
    json_set(out, "sockets", json_number(0.0));       /* self.sockets.clear() */
    json_set(out, "closed", json_bool(1));
    json_set(out, "remaining_error", json_string(HRD5CA_DEADLINE_MESSAGE));
    return out;
}

/* --- open_peer_url's prologue -------------------------------------------- */

/* `url.port or (443 if url.scheme == "https" else 80)` */
static double hrd5ca_effective_port(const json_t *port, const char *scheme) {
    if (port && port->type == JSON_NUMBER && port->num_val != 0.0) return port->num_val;
    return (scheme && strcmp(scheme, "https") == 0) ? HRD5CA_DEFAULT_HTTPS_PORT
                                                    : HRD5CA_DEFAULT_HTTP_PORT;
}

/* The target tuple as the registry stores it. */
static int hrd5ca_target_present(const json_t *targets, const char *host, double port) {
    size_t i;
    if (!targets || targets->type != JSON_ARRAY) return 0;
    for (i = 0; i < targets->c.count; i++) {
        const json_t *row = targets->c.items[i];
        const char *row_host;
        const json_t *row_port;
        if (!row || row->type != JSON_ARRAY || row->c.count < 2) continue;
        row_host = (row->c.items[0] && row->c.items[0]->type == JSON_STRING)
                       ? row->c.items[0]->str_val : NULL;
        row_port = row->c.items[1];
        if (row_host && host && strcmp(row_host, host) == 0 && row_port &&
            row_port->type == JSON_NUMBER && row_port->num_val == port) {
            return 1;
        }
    }
    return 0;
}

json_t *w20_hostedroom_dfc5ca_open_plan(const json_t *targets, const char *host,
                                        const json_t *port, const char *scheme,
                                        double timeout, double elapsed) {
    double effective = hrd5ca_effective_port(port, scheme);
    json_t *out;
    size_t outstanding = (targets && targets->type == JSON_ARRAY) ? targets->c.count : 0;

    if (hrd5ca_target_present(targets, host, effective) ||
        outstanding >= (size_t)HRD5CA_MAX_OUTSTANDING_ATTEMPTS) {
        out = json_object();
        json_set(out, "status", json_string("error"));
        json_set(out, "message", json_string(HRD5CA_CAPACITY_MESSAGE));
        return out;
    }
    if (timeout - elapsed <= 0.0) {
        out = json_object();
        json_set(out, "status", json_string("error"));
        json_set(out, "message", json_string(HRD5CA_DEADLINE_MESSAGE));
        return out;
    }
    out = json_object();
    {
        json_t *pair = json_array();
        json_append(pair, json_string(host ? host : ""));
        json_append(pair, json_number(effective));
        json_set(out, "target", pair);
    }
    json_set(out, "status", json_string("ok"));
    return out;
}
