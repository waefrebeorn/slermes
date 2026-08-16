/*
 * ports/agent/monitoring/events.c — C11 port of agent/monitoring/events.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_monitoring_events.c ── */
/*
 * port_monitoring_events.c — C port of agent/monitoring/events.py
 *
 * Typed gateway monitoring events: content-free service-health and
 * redacted diagnostic events for the gateway daemon. No prompts,
 * messages, tool args/results, session history, or usage analytics.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hermes_json.h"

/* PoP: monev_now_ns @ agent/monitoring/events.py:_now_ns */
long long monev_now_ns(void) {
    /* Python: time.time_ns(). */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

/* PoP: monev_gateway_health_to_dict @ agent/monitoring/events.py:to_dict */
char *monev_gateway_health_to_dict(const char *arg) {
    /* Python: GatewayHealthEvent.to_dict — {"event": "gateway_health", **asdict}.
     * Arg = "fields_json" (object of the dataclass fields). */
    if (!arg || !*arg) return strdup("{\"event\": \"gateway_health\"}");
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{\"event\": \"gateway_health\"}"); }
    char *body = json_serialize(j);
    json_free(j);
    char *out = NULL;
    if (body) {
        asprintf(&out, "{\"event\": \"gateway_health\", %s}", body[0] == '{' ? body + 1 : body);
        free(body);
    }
    return out ? out : strdup("{\"event\": \"gateway_health\"}");
}

/* PoP: monev_gateway_diagnostic_to_dict @ agent/monitoring/events.py:to_dict */
char *monev_gateway_diagnostic_to_dict(const char *arg) {
    /* Python: GatewayDiagnosticEvent.to_dict. */
    if (!arg || !*arg) return strdup("{\"event\": \"gateway_diagnostic\"}");
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{\"event\": \"gateway_diagnostic\"}"); }
    char *body = json_serialize(j);
    json_free(j);
    char *out = NULL;
    if (body) {
        asprintf(&out, "{\"event\": \"gateway_diagnostic\", %s}", body[0] == '{' ? body + 1 : body);
        free(body);
    }
    return out ? out : strdup("{\"event\": \"gateway_diagnostic\"}");
}

/* PoP: monev_cron_execution_to_dict @ agent/monitoring/events.py:to_dict */
char *monev_cron_execution_to_dict(const char *arg) {
    /* Python: CronExecutionEvent.to_dict. */
    if (!arg || !*arg) return strdup("{\"event\": \"cron_execution\"}");
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{\"event\": \"cron_execution\"}"); }
    char *body = json_serialize(j);
    json_free(j);
    char *out = NULL;
    if (body) {
        asprintf(&out, "{\"event\": \"cron_execution\", %s}", body[0] == '{' ? body + 1 : body);
        free(body);
    }
    return out ? out : strdup("{\"event\": \"cron_execution\"}");
}

