/*
 * dispatch.c -- real C11 port of connector dispatch routing
 *   tools/connectors/dispatch.py.
 *
 * PoP: w20_dispatch_c9fbe4_partition @ tools/connectors/gateway/merge.py:partition_calls
 * PoP: w20_dispatch_c9fbe4_connector_call @ tools/connectors/dispatch.py:dispatch_connector_call
 * PoP: w20_dispatch_c9fbe4_connector_batch @ tools/connectors/dispatch.py:dispatch_connector_batch
 * PoP: w20_dispatch_c9fbe4_max_calls @ tools/connectors/gateway/config.py:MAX_CALLS_PER_DISPATCH
 *
 * The gateway bridge, the local tool executor and the interrupt flag are not
 * pure; they are injected by the caller as a scripted answer list, exactly like
 * the reference gate injects its service.  Partitioning, per-entry call ids,
 * result splicing and the batch refusals are the ported behaviour.
 */
#include "hermes_json.h"
#include "noc_w20_dispatch_c9fbe4.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISP_MAX_CALLS 10
#define DISP_PREFIX "connectors"
#define DISP_PREFIX_LEN 10

static char *disp_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static const char *disp_str(const json_t *obj, const char *key) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int disp_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `type(x) is int`, needed for assemble_results' slot index. */
static int disp_is_py_int(const json_t *v, double *out) {
    if (!v || v->type != JSON_NUMBER) return 0;
    if (!isfinite(v->num_val) || floor(v->num_val) != v->num_val) return 0;
    *out = v->num_val;
    return 1;
}

int w20_dispatch_c9fbe4_max_calls(void) {
    return DISP_MAX_CALLS;
}

/* ---- parse_connector_name ------------------------------------------------- */

/* name.split("__", 2) with prefix == "connectors" and both slugs non-empty. */
static int disp_split_name(const char *name, char *connector, size_t cap_c,
                           char *tool, size_t cap_t) {
    const char *a, *b;
    size_t n_conn, n_tool;
    if (!name) return 0;
    if (strncmp(name, DISP_PREFIX, DISP_PREFIX_LEN) != 0) return 0;
    a = strstr(name, "__");
    if (!a || (size_t)(a - name) != DISP_PREFIX_LEN) return 0;
    b = strstr(a + 2, "__");
    if (!b) return 0;
    n_conn = (size_t)(b - (a + 2));
    n_tool = strlen(b + 2);
    if (!n_conn || !n_tool) return 0;
    if (n_conn >= cap_c || n_tool >= cap_t) return 0;
    memcpy(connector, a + 2, n_conn);
    connector[n_conn] = '\0';
    memcpy(tool, b + 2, n_tool);
    tool[n_tool] = '\0';
    return 1;
}

static json_t *disp_error_entry(int position, const char *name, const char *code,
                                const char *message) {
    json_t *entry = json_object();
    json_t *error = json_object();
    json_set(error, "code", json_string(code));
    json_set(error, "message", json_string(message));
    json_set(entry, "index", json_number((double)position));
    json_set(entry, "name", json_string(name ? name : ""));
    json_set(entry, "error", error);
    return entry;
}

/* partition_calls: local / remote / malformed-connector-name errors. */
json_t *w20_dispatch_c9fbe4_partition(const json_t *req) {
    json_t *calls = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "calls") : NULL;
    json_t *local = json_array(), *remote = json_array(), *errors = json_array();
    json_t *out = json_object();
    size_t i, n = (calls && calls->type == JSON_ARRAY) ? calls->c.count : 0;

    for (i = 0; i < n; i++) {
        const json_t *call = calls->c.items[i];
        const char *name = (call && call->type == JSON_OBJECT) ? disp_str(call, "name") : NULL;
        char connector[256], tool[512];
        if (disp_split_name(name, connector, sizeof(connector), tool, sizeof(tool))) {
            json_t *plan = json_object();
            json_t *args = json_obj_get(call, "arguments");
            json_set(plan, "position", json_number((double)i));
            json_set(plan, "name", json_string(name));
            json_set(plan, "connector", json_string(connector));
            json_set(plan, "tool", json_string(tool));
            json_set(plan, "arguments",
                     (args && args->type == JSON_OBJECT) ? json_copy(args) : json_object());
            json_append(remote, plan);
            continue;
        }
        if (name && !strncmp(name, "connectors__", 12)) {
            json_append(errors, disp_error_entry(
                (int)i, name, "TOOL_NOT_FOUND",
                "Malformed connector tool name; expected connectors__<connector>__{tool}."));
            continue;
        }
        {
            json_t *pair = json_array();
            json_append(pair, json_number((double)i));
            json_append(pair, (call && call->type == JSON_OBJECT) ? json_copy(call) : json_object());
            json_append(local, pair);
        }
    }
    json_set(out, "local", local);
    json_set(out, "remote", remote);
    json_set(out, "errors", errors);
    return out;
}

/* ---- dispatch_connector_call --------------------------------------------- */

json_t *w20_dispatch_c9fbe4_connector_call(const json_t *req) {
    json_t *calls = json_array();
    json_t *partition;
    json_t *remote;
    json_t *entries = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "entries") : NULL;
    json_t *entry, *result, *data_use, *out;
    static const char *const KEEP[] = {"response", "error", "account", NULL};
    int i;

    {
        json_t *call = json_object();
        json_t *args = json_obj_get(req, "arguments");
        json_set(call, "name", json_string(disp_str(req, "name") ? disp_str(req, "name") : ""));
        json_set(call, "arguments", args ? json_copy(args) : json_object());
        json_append(calls, call);
    }
    {
        json_t *wrapper = json_object();
        json_set(wrapper, "calls", calls);
        partition = w20_dispatch_c9fbe4_partition(wrapper);
        json_free(wrapper);
    }
    json_free(partition);

    /* `entry = entries[0]` -- an empty bridge answer is an IndexError. */
    if (!entries || entries->type != JSON_ARRAY || entries->c.count == 0) {
        out = json_object();
        json_set(out, "status", json_string("raised"));
        json_set(out, "type", json_string("IndexError"));
        json_set(out, "message", json_string("list index out of range"));
        return out;
    }
    entry = entries->c.items[0];
    if (!entry || entry->type != JSON_OBJECT) entry = NULL;
    (void)remote;

    result = json_object();
    for (i = 0; KEEP[i]; i++) {
        json_t *v = entry ? json_obj_get(entry, KEEP[i]) : NULL;
        if (v) json_set(result, KEEP[i], json_copy(v));
    }
    data_use = json_array();
    {
        /* `record_data_use(entry.get("data_use") or ())` */
        json_t *du = entry ? json_obj_get(entry, "data_use") : NULL;
        if (du && du->type == JSON_ARRAY) {
            size_t k;
            for (k = 0; k < du->c.count; k++) json_append(data_use, json_copy(du->c.items[k]));
        }
    }
    out = json_object();
    json_set(out, "result", result);
    json_set(out, "data_use", data_use);
    return out;
}

/* ---- assemble_results ---------------------------------------------------- */

static json_t *disp_assemble(int total, json_t *entries) {
    json_t **slots;
    json_t *results = json_array();
    json_t *out = json_object();
    size_t i;
    int position, error_count = 0;

    if (total < 0) total = 0;
    slots = (json_t **)calloc((size_t)(total > 0 ? total : 1), sizeof(*slots));
    if (!slots) return out;
    for (i = 0; entries && i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        double index;
        if (!entry || entry->type != JSON_OBJECT) continue;
        if (!disp_is_py_int(json_obj_get(entry, "index"), &index)) continue;
        if (index < 0 || index >= (double)total) continue;
        if (slots[(int)index]) continue;              /* first claim wins */
        slots[(int)index] = json_copy(entry);
    }
    for (position = 0; position < total; position++) {
        json_t *entry = slots[position];
        if (!entry) {
            entry = disp_error_entry(position, "", "PROVIDER_ERROR",
                                     "No result was produced for this call.");
        }
        if (json_obj_get(entry, "error")) error_count++;
        json_append(results, entry);
    }
    free(slots);
    json_set(out, "results", results);
    json_set(out, "success_count", json_number((double)(total - error_count)));
    json_set(out, "error_count", json_number((double)error_count));
    json_set(out, "total_count", json_number((double)total));
    return out;
}

/* ---- dispatch_connector_batch -------------------------------------------- */

static char *disp_with_id(const char *text, const char *call_id) {
    const char *p = text, *tag = "$ID$";
    size_t tag_len = 4, id_len = strlen(call_id);
    size_t cap = strlen(text) + 64, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    while (*p) {
        const char *hit = strstr(p, tag);
        size_t chunk = hit ? (size_t)(hit - p) : strlen(p);
        if (len + chunk + id_len + 1 > cap) {
            cap = (len + chunk + id_len + 1) * 2;
            out = (char *)realloc(out, cap);
            if (!out) return NULL;
        }
        memcpy(out + len, p, chunk);
        len += chunk;
        p += chunk;
        if (!hit) break;
        memcpy(out + len, call_id, id_len);
        len += id_len;
        p += tag_len;
    }
    out[len] = '\0';
    return out;
}

/* json.loads(payload) when it is a string; ValueError keeps the raw text. */
static json_t *disp_payload_value(const json_t *spec, const char *call_id) {
    if (spec && spec->type == JSON_STRING) {
        char *text = disp_with_id(spec->str_val, call_id);
        json_t *parsed;
        if (!text) return json_string("");
        parsed = json_parse(text, NULL);
        if (!parsed) parsed = json_string(text);   /* the ValueError branch */
        free(text);
        return parsed;
    }
    return spec ? json_copy(spec) : json_null();
}

json_t *w20_dispatch_c9fbe4_connector_batch(const json_t *req) {
    json_t *calls = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "calls") : NULL;
    json_t *ids = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "ids") : NULL;
    json_t *payloads = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "payloads") : NULL;
    json_t *interrupt_at_j = (req && req->type == JSON_OBJECT) ? json_obj_get(req, "interrupted_at") : NULL;
    const char *local_error = disp_str(req, "local_error");
    int max_calls = DISP_MAX_CALLS;
    int total = (calls && calls->type == JSON_ARRAY) ? (int)calls->c.count : 0;
    const char *base_id = disp_str(ids, "tool_call_id");
    int interrupt_at = -1;
    json_t *partition, *remote, *entries, *out;
    size_t i;

    out = json_object();
    if (total > max_calls) {
        char message[256];
        snprintf(message, sizeof(message),
                 "too many calls: %d > max %d. Retry with fewer calls per batch.",
                 total, max_calls);
        json_set(out, "error", json_string(message));
        return out;
    }
    {
        json_t *wrapper = json_object();
        json_set(wrapper, "calls", calls ? json_copy(calls) : json_array());
        partition = w20_dispatch_c9fbe4_partition(wrapper);
        json_free(wrapper);
    }
    {
        json_t *local = json_obj_get(partition, "local");
        if (local && local->type == JSON_ARRAY && local->c.count > 0) {
            /* a local tool in a multi-entry batch: restated by the caller's own hook */
            json_set(out, "error", json_string(local_error ? local_error : ""));
            json_free(partition);
            return out;
        }
    }
    entries = json_array();
    {
        json_t *errors = json_obj_get(partition, "errors");
        if (errors && errors->type == JSON_ARRAY) {
            for (i = 0; i < errors->c.count; i++) json_append(entries, json_copy(errors->c.items[i]));
        }
    }
    if (interrupt_at_j && interrupt_at_j->type == JSON_NUMBER) {
        interrupt_at = (int)interrupt_at_j->num_val;
    }
    remote = json_obj_get(partition, "remote");
    for (i = 0; remote && i < remote->c.count; i++) {
        json_t *plan = remote->c.items[i];
        double position_d = 0.0;
        const char *name = disp_str(plan, "name");
        json_t *payload, *value, *entry;
        char *call_id;
        (void)disp_is_py_int(json_obj_get(plan, "position"), &position_d);

        if (interrupt_at >= 0 && (int)i >= interrupt_at) {
            size_t k;
            for (k = i; k < remote->c.count; k++) {
                json_t *rest = remote->c.items[k];
                double pos = 0.0;
                (void)disp_is_py_int(json_obj_get(rest, "position"), &pos);
                json_append(entries, disp_error_entry(
                    (int)pos, disp_str(rest, "name"), "INTERRUPTED",
                    "Stopped by the user before this call was made."));
            }
            break;
        }
        /* entry_ids: ``<id>#<position>`` so observers never fold an entry into
         * the wrapper or into a sibling entry. */
        if (base_id && base_id[0]) {
            size_t need = strlen(base_id) + 32;
            call_id = (char *)malloc(need);
            if (!call_id) { json_free(partition); json_free(entries); return out; }
            snprintf(call_id, need, "%s#%d", base_id, (int)position_d);
        } else {
            call_id = disp_dup("None");
        }
        payload = (payloads && payloads->type == JSON_ARRAY && i < payloads->c.count)
                      ? payloads->c.items[i] : NULL;
        value = disp_payload_value(payload, call_id ? call_id : "None");
        entry = json_object();
        json_set(entry, "index", json_number(position_d));
        json_set(entry, "name", json_string(name ? name : ""));
        if (value && value->type == JSON_OBJECT && json_has(value, "error")) {
            json_t *error = json_obj_get(value, "error");
            if (error && error->type == JSON_OBJECT) {
                json_set(entry, "error", json_copy(error));
            } else {
                json_t *wrapped = json_object();
                char text[64];
                if (!error) snprintf(text, sizeof(text), "None");
                else if (error->type == JSON_STRING) snprintf(text, sizeof(text), "%s", error->str_val);
                else if (error->type == JSON_BOOL) snprintf(text, sizeof(text), "%s", error->bool_val ? "True" : "False");
                else if (error->type == JSON_NUMBER) {
                    if (isfinite(error->num_val) && floor(error->num_val) == error->num_val)
                        snprintf(text, sizeof(text), "%lld", (long long)error->num_val);
                    else
                        snprintf(text, sizeof(text), "%.17g", error->num_val);
                } else snprintf(text, sizeof(text), "%s", "");
                json_set(wrapped, "code", json_string("TOOL_ERROR"));
                json_set(wrapped, "message", json_string(text));
                json_set(entry, "error", wrapped);
            }
        } else if (value && value->type == JSON_OBJECT) {
            json_t *response = json_obj_get(value, "response");
            json_set(entry, "response", response ? json_copy(response) : json_copy(value));
        } else {
            json_set(entry, "response", value ? json_copy(value) : json_null());
        }
        if (value && value->type == JSON_OBJECT && disp_truthy(json_obj_get(value, "account"))) {
            json_set(entry, "account", json_copy(json_obj_get(value, "account")));
        }
        json_append(entries, entry);
        json_free(value);
        free(call_id);
    }
    {
        json_t *assembled = disp_assemble(total, entries);
        json_free(out);
        json_free(partition);
        json_free(entries);
        return assembled;
    }
}
