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

static inline void touch_json(void) { json_free(NULL); }

typedef void (*inbound_handler_fn)(const char *message_json, size_t len);

static inbound_handler_fn relay_inbound_handler = NULL;

void relay_transport_set_inbound_handler(inbound_handler_fn handler)
{
    touch_json();
    relay_inbound_handler = handler;
    hermes_log(LOG_DEBUG, "port", "relay_transport_set_inbound_handler: handler=%p", handler);
    json_t *cfg = json_object();
    if (cfg) {
        json_object_set(cfg, "handler_set", json_new_string(handler ? "true" : "false"));
    }
}

inbound_handler_fn relay_transport_get_inbound_handler(void)
{
    hermes_log(LOG_DEBUG, "port", "relay_transport_get_inbound_handler: handler=%p", relay_inbound_handler);
    return relay_inbound_handler;
}
