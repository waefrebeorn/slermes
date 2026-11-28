#include "crypto.h"
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

/* PoP: agent_monitoring_gateway_health_u_safe_instance_id
 * @ agent/monitoring/gateway_health.py:_safe_instance_id */
/* "sha256:" + sha256(str(raw or "unknown").encode("utf-8"))[:24]. */
char *agent_monitoring_gateway_health_u_safe_instance_id(const char *arg) {
    const char *v = (arg && arg[0]) ? arg : "unknown";
    unsigned char d[32]; crypto_sha256((const unsigned char *)v, strlen(v), d);
    char hex[33]; { static const char *H="0123456789abcdef"; for (int i=0;i<32;i++){ hex[2*i]=H[(d[i]>>4)&15]; hex[2*i+1]=H[d[i]&15]; } hex[64]=0; }
    char *out = malloc(40); snprintf(out, 40, "sha256:%.24s", hex);
    return out;
}
