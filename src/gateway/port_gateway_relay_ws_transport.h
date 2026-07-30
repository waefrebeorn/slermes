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

/* Opaque relay WebSocket transport (defined in the .c). */
typedef struct ws_transport ws_transport_t;

/* Handle one received WebSocket frame line on transport t. */
void ws_transport_handle_frame(ws_transport_t *t, const char *line);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GATEWAY_RELAY_WS_TRANSPORT_H */
