/*
 * gateway_client.c — C home for upstream Hermes hermes_cli/gateway_client.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: hermes_cli_gateway_client_pending @ hermes_cli/gateway_client.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const hermes_cli_gateway_client_CONST_GATEWAY_WS_TICKET_PREFIX = "hermes-gateway-ticket.";
static long long hermes_cli_gateway_client_CONST_DEFAULT_RPC_TIMEOUT = 30;
static long long hermes_cli_gateway_client_CONST_COMPRESS_RPC_TIMEOUT = 660;
static long long hermes_cli_gateway_client_CONST_EVENT_STALL_TIMEOUT = 30;
static long long hermes_cli_gateway_client_CONST_EVENT_OVERFLOW_FRAMES = 16384;
static const char *const hermes_cli_gateway_client_CONST_NOT_SENT = "Gateway disconnected before the request was sent; nothing was sent.";

void hermes_cli_gateway_client_port_pending(void) { (void)0; }
