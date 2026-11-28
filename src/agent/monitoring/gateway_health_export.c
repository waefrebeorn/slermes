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

/* PoP: agent_monitoring_gateway_health_export_u_gateway_health_config
 * @ agent/monitoring/gateway_health_export.py:_gateway_health_config */
/* _monitoring_section(config, "gateway_health_export"): node = (config or {})["monitoring"]
 * or {}; then node = node[key] or {} per key. A truthy non-dict leaf is returned as-is. */
static int _mon_truthy(json_t *v) {
    if (!v || json_is_null(v)) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}
static char *_mon_section(const char *arg, const char *const *path, int n) {
    json_t *cfg = arg ? json_parse(arg, NULL) : NULL;
    json_t *node = NULL;
    if (cfg && json_is_object(cfg)) {
        json_t *mon = json_obj_get(cfg, "monitoring");
        if (_mon_truthy(mon)) node = mon; else node = NULL;
    }
    for (int i = 0; i < n; i++) {
        json_t *next = NULL;
        if (node && json_is_object(node)) next = json_obj_get(node, path[i]);
        node = _mon_truthy(next) ? next : NULL;
    }
    char *r = node ? json_serialize(node) : strdup("{}");
    if (!r) r = strdup("{}");
    if (cfg) json_free(cfg);
    return r;
}
char *agent_monitoring_gateway_health_export_u_gateway_health_config(const char *arg) {
    static const char *P[] = {"gateway_health_export"};
    return _mon_section(arg, P, 1);
}
