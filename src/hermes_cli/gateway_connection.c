/*
 * gateway_connection.c — faithful C11 port of hermes_cli/gateway_connection.py
 * (gate w17_gatewaycon_0ab9ed).
 *
 * PoP: w17_gatewaycon_0ab9ed_lost_before @ hermes_cli/gateway_connection.py:lost_before
 * PoP: w17_gatewaycon_0ab9ed_runs_route @ hermes_cli/gateway_connection.py:SessionState.runs_route
 * PoP: w17_gatewaycon_0ab9ed_apply_event @ hermes_cli/gateway_connection.py:SessionState.apply_event
 */
#include "noc_w17_gatewaycon_0ab9ed.h"

#include <stdlib.h>
#include <string.h>

static int is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }

static const char *str_of(const json_t *v) {
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

json_t *w17_gatewaycon_0ab9ed_lost_before(int is_exception, const char *connection_epoch,
                                          const char *epoch) {
    long long lost, ep;
    if (!is_exception || !connection_epoch || !epoch) return json_bool(0);
    lost = strtoll(connection_epoch, NULL, 10);
    ep = strtoll(epoch, NULL, 10);
    return json_bool(lost < ep);
}

json_t *w17_gatewaycon_0ab9ed_runs_route(const json_t *info, const char *model, const char *provider) {
    const char *current, *current_provider;
    current = str_of(json_obj_get(info, "model"));
    current_provider = str_of(json_obj_get(info, "provider"));
    if (!current || !*current) return json_bool(0);        /* bool(current) */
    if (!model || strcmp(model, current)) return json_bool(0);
    if (provider && *provider && current_provider && *current_provider && strcmp(provider, current_provider))
        return json_bool(0);
    return json_bool(1);
}

json_t *w17_gatewaycon_0ab9ed_apply_event(const char *kind, const json_t *payload) {
    json_t *out = json_object();
    json_t *info = json_object();
    json_t *pending = json_array();
    json_t *usage = json_object();
    json_t *todo_state = json_null();
    const char *title = NULL;
    long long pending_seq = 0, gaps = 0;
    size_t i;
    kind = kind ? kind : "";

    if ((!strcmp(kind, "session.info") || !strcmp(kind, "session.updated")) && is_dict(payload)) {
        for (i = 0; i < payload->c.count; ++i) {
            const char *k = payload->c.keys[i];
            if (!k || !strcmp(k, "session_id")) continue;
            json_set(info, k, json_copy(payload->c.items[i]));
        }
        {
            const json_t *p = json_obj_get(payload, "pending");
            if (is_list(p)) {
                json_free(pending);
                pending = json_copy(p);
                pending_seq++;
            }
        }
        {
            const char *t = str_of(json_obj_get(payload, "title"));
            if (t && *t) title = t;
        }
    } else if (!strcmp(kind, "todo.updated") && is_dict(payload)) {
        const json_t *todos = json_obj_get(payload, "todos");
        const json_t *rev = json_obj_get(payload, "revision");
        json_t *td = json_object();
        json_set(td, "todos", is_list(todos) ? json_copy(todos) : json_array());
        json_set(td, "revision", rev ? json_copy(rev) : json_null());
        todo_state = td;
    } else if (!strcmp(kind, "session.usage") && is_dict(payload)) {
        const json_t *nested = json_obj_get(payload, "usage");
        const json_t *api = json_obj_get(payload, "api_calls");
        int flat = (api && api->type == JSON_NUMBER && api->num_val == (double)(long long)api->num_val)
                   || !is_dict(nested);
        const json_t *src = flat ? payload : nested;
        json_free(usage);
        usage = is_dict(src) ? json_copy(src) : json_object();
    } else if (!strcmp(kind, "session.replay_gap")) {
        gaps++;
    }
    /* request.cancel only notifies listeners; a fresh state has none. */

    json_set(out, "info", info);
    json_set(out, "title", title ? json_string(title) : json_null());
    json_set(out, "pending", pending);
    json_set(out, "pending_seq", json_number((double)pending_seq));
    json_set(out, "todo_state", todo_state);
    json_set(out, "usage", usage);
    json_set(out, "gaps", json_number((double)gaps));
    return out;
}
