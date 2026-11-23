/*
 * events.c — C home for upstream Hermes acp_adapter/events.py.
 *
 * PoP: w6_events_756cad_plan_status @ acp_adapter/events.py:_PLAN_STATUS
 * PoP: w6_events_756cad_plan_update @ acp_adapter/events.py:_build_plan_update_from_todo_result
 * PoP: w6_events_756cad_upgrade_queue @ acp_adapter/events.py:_upgrade_queue
 *
 * The ACP connection / event-loop push plumbing and UUID allocator are runtime
 * concerns and are not reimplemented. The tested behaviour is the pure todo ->
 * ACP-plan translation, the cancelled-status mapping and the legacy-string
 * FIFO upgrade, all driven by injected data so the gate is deterministic.
 */
#include "noc_w6_events_756cad.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

static char *dup_str(const char *s)
{
    size_t n;
    char *o;
    if (!s) return NULL;
    n = strlen(s);
    o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* Python str(value) for the JSON scalars the translation sees. */
static char *py_str(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return dup_str("");
    switch (v->type) {
        case JSON_STRING: return dup_str(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dup_str(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%g", v->num_val);
            return dup_str(buf);
        default:          return dup_str("");
    }
}

/* Python str.strip() for ASCII whitespace. */
static char *strip_ws(const char *s)
{
    const unsigned char *a, *b;
    char *o;
    size_t n;
    if (!s) return dup_str("");
    n = strlen(s);
    a = (const unsigned char *)s;
    b = a + n;
    while (a < b && isspace(*a)) a++;
    while (b > a && isspace(*(b - 1))) b--;
    o = (char *)malloc((size_t)(b - a) + 1);
    if (!o) return NULL;
    memcpy(o, a, (size_t)(b - a));
    o[b - a] = '\0';
    return o;
}

/* _PLAN_STATUS.get(raw, "pending") with cancelled -> completed. */
static const char *plan_status_map(const char *raw)
{
    if (!strcmp(raw, "pending"))    return "pending";
    if (!strcmp(raw, "in_progress")) return "in_progress";
    if (!strcmp(raw, "completed"))  return "completed";
    if (!strcmp(raw, "cancelled"))  return "completed";
    return "pending";
}

struct json_t *w6_events_756cad_plan_status(const char *raw)
{
    const char *v = (raw && *raw) ? raw : "pending";
    char *st = strip_ws(v);
    json_t *res = json_string(plan_status_map(st ? st : "pending"));
    free(st);
    return res;
}

struct json_t *w6_events_756cad_plan_update(const char *result)
{
    json_t *data, *todos, *entries, *out;
    size_t n, i;
    char *st;

    if (!result || !*result)
        return json_null();
    st = strip_ws(result);
    if (!st || st[0] == '\0') {
        free(st);
        return json_null();
    }
    free(st);

    data = json_parse(result, NULL);
    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        return json_null();
    }
    todos = json_obj_get(data, "todos");
    if (!todos || todos->type != JSON_ARRAY) {
        json_free(data);
        return json_null();
    }

    entries = json_array();
    n = json_len(todos);
    for (i = 0; i < n; i++) {
        json_t *item = json_get(todos, i);
        json_t *cv, *sv;
        char *c_raw, *c_str, *rs_raw, *rs_str;
        const char *status;
        json_t *entry;

        if (!item || item->type != JSON_OBJECT)
            continue;
        cv = json_obj_get(item, "content");
        if (!py_truthy(cv)) cv = json_obj_get(item, "id");
        c_raw = cv ? py_str(cv) : dup_str("");
        c_str = strip_ws(c_raw);
        free(c_raw);
        if (!c_str || c_str[0] == '\0') {
            free(c_str);
            continue;
        }

        sv = json_obj_get(item, "status");
        rs_raw = py_truthy(sv) ? py_str(sv) : dup_str("pending");
        rs_str = strip_ws(rs_raw);
        free(rs_raw);
        status = plan_status_map(rs_str ? rs_str : "pending");
        if (rs_str && !strcmp(rs_str, "cancelled")) {
            char *joined = (char *)malloc(strlen(c_str) + 13);
            if (joined) {
                snprintf(joined, strlen(c_str) + 13, "[cancelled] %s", c_str);
                free(c_str);
                c_str = joined;
            }
        }
        free(rs_str);

        entry = json_object();
        json_set(entry, "content", json_string(c_str));
        json_set(entry, "priority", json_string("medium"));
        json_set(entry, "status", json_string(status));
        json_append(entries, entry);
        free(c_str);
    }

    out = json_object();
    json_set(out, "session_update", json_string("plan"));
    json_set(out, "entries", entries);
    json_free(data);
    return out;
}

struct json_t *w6_events_756cad_upgrade_queue(const struct json_t *ids, const char *name)
{
    json_t *q;
    if (!ids || ids->type != JSON_OBJECT || !name)
        return json_null();
    q = json_obj_get(ids, name);
    if (!q) return json_null();
    if (q->type == JSON_STRING) {
        json_t *arr = json_array();
        json_append(arr, json_copy(q));
        return arr;
    }
    if (q->type == JSON_ARRAY) return json_copy(q);
    return json_null();
}
