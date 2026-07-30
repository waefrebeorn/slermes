/*
 * port_middleware.c — Faithful C11 port of pure helpers from
 * hermes_cli/middleware.py
 *
 * Ported: observer_payload, middleware_payload, _trace_entry.
 * _safe_copy (deepcopy) and the apply/run/invoke_middleware functions
 * depend on a plugin registry (IO/state) and are left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "middleware.h"

/* PoP: middleware_observer_payload @ hermes_cli/middleware.py:observer_payload */
json_t *middleware_observer_payload(const json_t *kwargs) {
    json_t *out = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            json_set(out, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
        }
    }
    if (!json_obj_get(out, "telemetry_schema_version")) {
        json_set(out, "telemetry_schema_version", json_string(MW_OBSERVER_SCHEMA_VERSION));
    }
    return out;
}

/* PoP: middleware_payload @ hermes_cli/middleware.py:middleware_payload */
json_t *middleware_middleware_payload(const json_t *kwargs) {
    json_t *out = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++) {
            json_set(out, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
        }
    }
    if (!json_obj_get(out, "telemetry_schema_version")) {
        json_set(out, "telemetry_schema_version", json_string(MW_OBSERVER_SCHEMA_VERSION));
    }
    if (!json_obj_get(out, "middleware_schema_version")) {
        json_set(out, "middleware_schema_version", json_string(MW_MIDDLEWARE_SCHEMA_VERSION));
    }
    return out;
}

/* PoP: middleware_trace_entry @ hermes_cli/middleware.py:_trace_entry */
json_t *middleware_trace_entry(const json_t *result) {
    json_t *entry = json_object();
    if (result && result->type == JSON_OBJECT) {
        for (size_t i = 0; i < result->c.count; i++) {
            const char *key = result->c.keys[i];
            if (strcmp(key, "source") == 0 || strcmp(key, "reason") == 0 || strcmp(key, "name") == 0) {
                json_t *v = result->c.items[i];
                if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
                    json_set(entry, key, json_copy(v));
                }
            }
        }
    }
    if (entry->c.count == 0) {
        json_set(entry, "source", json_string("plugin"));
    }
    return entry;
}
