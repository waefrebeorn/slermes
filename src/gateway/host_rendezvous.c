/*
 * host_rendezvous.c — C home for upstream Hermes gateway/host_rendezvous.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: gateway_host_rendezvous_pending @ gateway/host_rendezvous.py:<module>
 */
#include <stddef.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const gateway_host_rendezvous_CONST_HOST_PROTOCOL_VERSION = 1;
static const char *const gateway_host_rendezvous_CONST_HOST_IDENTITY_PATH = "/api/host/identity";
static const char *const gateway_host_rendezvous_CONST_ROLE_GATEWAY = "gateway";
static const char *const gateway_host_rendezvous_CONST_ROLE_SERVE = "serve";
static const char *const gateway_host_rendezvous_CONST_ROLE_DESKTOP_SERVE = "desktop-serve";

void gateway_host_rendezvous_port_pending(void) { (void)0; }
