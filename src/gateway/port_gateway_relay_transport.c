/**
 * port_gateway_relay_transport.c — Port of Python gateway/relay/transport.py
 *
 * Real C implementations for relay transport functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"

typedef void (*inbound_handler_fn)(const char *message_json, size_t len);
typedef void (*passthrough_handler_fn)(const char *forward_json, size_t len, int buffer_id);

static inbound_handler_fn relay_inbound_handler = NULL;
static passthrough_handler_fn relay_passthrough_handler = NULL;
static volatile int relay_idle_acked = 0;

void relay_transport_set_inbound_handler(inbound_handler_fn handler)
{
    relay_inbound_handler = handler;
    hermes_log(LOG_DEBUG, "port", "relay_transport_set_inbound_handler: handler=%p", handler);
}

inbound_handler_fn relay_transport_get_inbound_handler(void)
{
    hermes_log(LOG_DEBUG, "port", "relay_transport_get_inbound_handler: handler=%p", relay_inbound_handler);
    return relay_inbound_handler;
}

/* PoP: relay_transport_set_passthrough_handler @ gateway/relay/transport.py:set_passthrough_handler */
/*
 * Port of Python gateway/relay/transport.py:set_passthrough_handler().
 * Registers the callback invoked with each forwarded passthrough request
 * (the passthrough plane forwards the real request to the gateway over the
 * same outbound socket). Optional on a transport; NULL clears it.
 */
void relay_transport_set_passthrough_handler(passthrough_handler_fn handler)
{
    relay_passthrough_handler = handler;
    hermes_log(LOG_DEBUG, "port", "relay_transport_set_passthrough_handler: handler=%p", handler);
}

passthrough_handler_fn relay_transport_get_passthrough_handler(void)
{
    return relay_passthrough_handler;
}

/* PoP: relay_transport_go_idle @ gateway/relay/transport.py:go_idle */
/*
 * Port of Python gateway/relay/transport.py:go_idle().
 * Sends a `going_idle` frame and awaits the connector's `going_idle_ack`
 * (connector-authoritative confirmation that live delivery stopped and inbound
 * now buffers durably). Returns true on ack, false on timeout / not-connected.
 * The caller proceeds to close regardless; without wiring there is simply no
 * buffering. Time-bounded by timeout_s (default 10s).
 */
bool relay_transport_go_idle(double timeout_s)
{
    if (!relay_inbound_handler) {
        /* Not connected / no transport wired — caller closes regardless. */
        return false;
    }
    relay_idle_acked = 0;
    /* Emit the going_idle frame via the inbound-handler channel (outbound
     * socket, in the live relay this is the same socket the connector listens
     * on). The connector replies with going_idle_ack which the inbound handler
     * is expected to flag via relay_transport_signal_idle_ack(). */
    if (relay_inbound_handler) {
        relay_inbound_handler("{\"op\":\"going_idle\"}", strlen("{\"op\":\"going_idle\"}"));
    }
    /* Await ack (polled by the inbound handler in the real loop). */
    double waited = 0.0;
    const double step = 0.05;
    while (waited < (timeout_s > 0 ? timeout_s : 10.0)) {
        if (relay_idle_acked) return true;
        struct timespec ts = {0, (long)(step * 1e9)};
        nanosleep(&ts, NULL);
        waited += step;
    }
    return relay_idle_acked ? true : false;
}

/* Called by the inbound handler when a going_idle_ack frame arrives. */
void relay_transport_signal_idle_ack(void)
{
    relay_idle_acked = 1;
}
