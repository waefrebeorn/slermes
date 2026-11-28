#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * gateway_health_export.c — C home for upstream Hermes agent/monitoring/gateway_health_export.py.
 * STATUS: C HOME CREATED — behavior port PENDING. This file is provenance,
 *          not verified parity. It is deliberately not linked into the build.
 * PoP: agent_monitoring_gateway_health_export_pending @ agent/monitoring/gateway_health_export.py:<module>
 */
#include <stddef.h>

void agent_monitoring_gateway_health_export_port_pending(void) { (void)0; }

/* PoP: agent_monitoring_gateway_health_export_u_gateway_health_event
 * @ agent/monitoring/gateway_health_export.py:_gateway_health_event */
/* ev.get("event") in {"gateway_health","cron_execution"}. */
int agent_monitoring_gateway_health_export_u_gateway_health_event(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    if (!o || !json_is_object(o)) { if (o) json_free(o); return 0; }
    json_t *ev = json_obj_get(o, "event");
    int hit = 0;
    if (ev && json_is_string(ev) && ev->str_val)
        hit = (strcmp(ev->str_val, "gateway_health") == 0 || strcmp(ev->str_val, "cron_execution") == 0);
    json_free(o);
    return hit;
}
