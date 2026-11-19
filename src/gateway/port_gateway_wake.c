/*
 * port_gateway_wake.c — Port of Python: gateway/wake.py (pure shape helpers).
 */

#include "port_gateway_wake.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *node_str(const json_t *node)
{
    if (!node) return dup_str("");
    char buf[64];
    switch (node->type) {
    case JSON_NULL:   return dup_str("");
    case JSON_STRING: return dup_str(node->str_val ? node->str_val : "");
    case JSON_NUMBER: snprintf(buf, sizeof(buf), "%.17g", node->num_val); return dup_str(buf);
    case JSON_BOOL:   return dup_str(node->bool_val ? "True" : "False");
    default:          return json_serialize(node);
    }
}

/* PoP: wake_adapter_supports_push @ gateway/wake.py:adapter_supports_push */
int wake_adapter_supports_push(int attr_present, int attr_value)
{
    return attr_present ? (attr_value != 0) : 1;
}

/* PoP: wake_runner_draining @ gateway/wake.py:_runner_draining */
int wake_runner_draining(int runner_present, int draining_present, int draining)
{
    return runner_present && draining_present && draining != 0;
}

/* PoP: wake_delegation_display_metadata @ gateway/wake.py:_delegation_display_metadata */
json_t *wake_delegation_display_metadata(const char *evt_json, int warnings_enabled)
{
    json_t *evt = evt_json ? json_parse(evt_json, NULL) : NULL;
    if (!evt || evt->type != JSON_OBJECT) {
        if (evt) json_free(evt);
        return NULL;
    }

    json_t *raw_results = json_obj_get(evt, "results");
    int n_results = 0;
    int completed = 0, failed = 0;
    if (raw_results && raw_results->type == JSON_ARRAY) {
        for (size_t i = 0; i < raw_results->c.count; i++) {
            json_t *r = raw_results->c.items[i];
            if (!r || r->type != JSON_OBJECT) continue;
            n_results++;
            const char *status = json_get_str(r, "status", NULL);
            if (status && (strcmp(status, "completed") == 0 || strcmp(status, "success") == 0))
                completed++;
            if (status && (strcmp(status, "failed") == 0 || strcmp(status, "error") == 0))
                failed++;
        }
    }
    int task_count = n_results > 0 ? n_results : 1;

    json_t *meta = json_object();
    char *delegation_id = node_str(json_obj_get(evt, "delegation_id"));
    json_set(meta, "delegation_id", json_string(delegation_id ? delegation_id : ""));
    free(delegation_id);
    json_set(meta, "task_count", json_number((double)task_count));
    json_set(meta, "completed_count",
             json_number((double)(completed ? completed : task_count - failed)));
    json_set(meta, "failed_count", json_number((double)failed));

    json_t *notice = json_obj_get(evt, "task_failure_notice");
    int has_notice = 0;
    if (notice) {
        switch (notice->type) {
        case JSON_NULL: has_notice = 0; break;
        case JSON_BOOL: has_notice = notice->bool_val; break;
        case JSON_NUMBER: has_notice = notice->num_val != 0.0; break;
        case JSON_STRING: has_notice = notice->str_val && notice->str_val[0]; break;
        default: has_notice = notice->c.count > 0; break;
        }
    }
    if (has_notice) {
        char notice_text[128] = "task_failure:";
        if (n_results > 0 && raw_results) {
            for (size_t i = 0; i < raw_results->c.count; i++) {
                json_t *r = raw_results->c.items[i];
                if (!r || r->type != JSON_OBJECT) continue;
                char *index = node_str(json_obj_get(r, "task_index"));
                snprintf(notice_text, sizeof(notice_text), "task_failure:%s",
                         index ? index : "");
                free(index);
                break;
            }
        }
        json_set(meta, "delivery_notice", json_string(notice_text));
        json_set(meta, "notification_category", json_string("diagnostic"));
        if (!warnings_enabled)
            json_set(meta, "presentation_suppressed", json_bool(true));
    }

    json_t *total = json_obj_get(evt, "total_duration_seconds");
    json_t *duration = (total && total->type == JSON_NUMBER) ? total
                                                             : json_obj_get(evt, "duration_seconds");
    if (duration && duration->type == JSON_NUMBER)
        json_set(meta, "duration_seconds", json_number(duration->num_val));

    json_free(evt);
    return meta;
}
