/*
 * port_gateway_relay_ws_transport.h — Slermes C11 port of the gateway
 * relay WebSocket transport frame handler (gateway/relay_ws_transport.py).
 *
 * Public surface consumed within the relay transport module. Faithful
 * extraction from the god header so callers no longer include hermes.h
 * transitively.
 */

#ifndef PORT_GATEWAY_RELAY_WS_TRANSPORT_H
#define PORT_GATEWAY_RELAY_WS_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Opaque relay WebSocket transport (defined in the .c). */
typedef struct ws_transport ws_transport_t;

/* Capability descriptor for one fronted platform (gateway/relay/descriptor.py). */
typedef struct {
    char label[256];
    char platform[128];
    int max_message_length;
    char emoji[64];
    char platform_hint[1024];
    bool pii_safe;
    char markdown_dialect[64];
    bool supports_draft_streaming;
    bool supports_edit;
    bool supports_threads;
    char supported_ops[1024];  /* JSON array string, or empty for legacy */
} capability_descriptor_t;

/* Parse a handshake JSON descriptor (descriptor.py:from_json). */
capability_descriptor_t capability_descriptor_from_json(const char *data);

/* The negotiated descriptor for one fronted platform, or NULL. */
capability_descriptor_t *ws_transport_descriptor_for_platform(ws_transport_t *t, const char *platform);

/* Handle one received WebSocket frame line on transport t. */
void ws_transport_handle_frame(ws_transport_t *t, const char *line);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GATEWAY_RELAY_WS_TRANSPORT_H */
