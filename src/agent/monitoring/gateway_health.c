/*
 * gateway_health.c — C home for upstream Hermes agent/monitoring/gateway_health.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: agent_monitoring_gateway_health_pending @ agent/monitoring/gateway_health.py:<module>
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void agent_monitoring_gateway_health_port_pending(void) { (void)0; }

/* PoP: agent_monitoring_gateway_health_u_platform_for_subsystem
 * @ agent/monitoring/gateway_health.py:platform_for_subsystem */
/* subsystem.split(".",1)[1] or None, but only when subsystem starts "platform.".
 * "" is the C spelling of None (the twin runtime cups None as ""). */
char *agent_monitoring_gateway_health_u_platform_for_subsystem(const char *arg) {
    const char *s = arg ? arg : "";
    if (strncmp(s, "platform.", 9) != 0) return strdup("");
    const char *rest = s + 9;
    return strdup(rest);
}
