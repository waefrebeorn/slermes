/*
 * cloud_agents.c — faithful C11 port of upstream Hermes hermes_cli/cloud_agents.py.
 *
 * PoP: w9_cloudagents_e05d38_org @ hermes_cli/cloud_agents.py:_org
 * PoP: w9_cloudagents_e05d38_agent @ hermes_cli/cloud_agents.py:_agent
 * PoP: w9_cloudagents_e05d38_discover_response @ hermes_cli/cloud_agents.py:discover
 * PoP: w9_cloudagents_e05d38_portal_signin_status @ hermes_cli/cloud_agents.py:PORTAL_SIGNIN_STATUS
 *
 * The network/auth half of discover() is a fixture seam: the oracle returns a
 * fake HTTP response (status code + body) and the real discover() shapes it.
 * Error paths are rendered as a status/code/message object instead of raising.
 */
#include "noc_w9_cloudagents_e05d38.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness of an arbitrary JSON value. */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default:          return 0;
    }
}

static const char *str_field(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int w9_cloudagents_e05d38_portal_signin_status(void) { return 409; }

json_t *w9_cloudagents_e05d38_org(const json_t *raw) {
    if (!json_is_object(raw)) return json_null();
    const char *id = str_field(raw, "id");
    if (!id) return json_null();
    const char *slug = str_field(raw, "slug");
    const char *name = str_field(raw, "name");
    json_t *out = json_object();
    json_set(out, "id", json_string(id));
    json_set(out, "slug", slug ? json_string(slug) : json_null());
    json_set(out, "name", json_string(name ? name : id));
    json_set(out, "isPersonal", json_bool(py_truthy(json_obj_get(raw, "isPersonal")) ? true : false));
    json_t *role = json_obj_get(raw, "role");
    if (py_truthy(role)) json_set(out, "role", json_copy(role));
    else                 json_set(out, "role", json_string("MEMBER"));
    return out;
}

json_t *w9_cloudagents_e05d38_agent(const json_t *raw) {
    json_t *out = json_object();
    if (!json_is_object(raw)) {
        json_set(out, "id", json_null());
        json_set(out, "name", json_null());
        json_set(out, "status", json_string("unknown"));
        json_set(out, "dashboardUrl", json_null());
        json_set(out, "dashboardGatewayState", json_string("unknown"));
        return out;
    }
    json_t *id_v = json_obj_get(raw, "id");
    const char *id = (id_v && id_v->type == JSON_STRING) ? id_v->str_val : NULL;
    json_set(out, "id", id ? json_copy(id_v) : json_null());
    const char *name = str_field(raw, "name");
    json_set(out, "name", name ? json_string(name) : (id ? json_string(id) : json_null()));
    const char *status = str_field(raw, "status");
    json_set(out, "status", json_string(status ? status : "unknown"));
    const char *dash = str_field(raw, "dashboardUrl");
    json_set(out, "dashboardUrl", dash ? json_string(dash) : json_null());
    const char *state = str_field(raw, "dashboardGatewayState");
    json_set(out, "dashboardGatewayState", json_string(state ? state : "unknown"));
    return out;
}

static json_t *cloud_error(int status, const char *code, const char *message) {
    json_t *e = json_object();
    json_set(e, "status", json_number((double)status));
    json_set(e, "code", json_string(code));
    json_set(e, "message", json_string(message));
    return e;
}

json_t *w9_cloudagents_e05d38_discover_response(int status_code,
                                               const struct json_t *body,
                                               const char *org) {
    (void)org; /* only affects the outgoing HTTP params, not the shaped result */
    json_t *b = json_is_object(body) ? (json_t *)body : NULL;
    if (status_code == 409 && b) {
        json_t *err = json_obj_get(b, "error");
        if (err && err->type == JSON_STRING && !strcmp(err->str_val, "org_selection_required")) {
            json_t *out = json_object();
            json_set(out, "needsOrgSelection", json_bool(true));
            json_t *orgs = json_array();
            json_t *raw_orgs = json_obj_get(b, "orgs");
            if (raw_orgs && raw_orgs->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len(raw_orgs); i++) {
                    json_t *o = w9_cloudagents_e05d38_org(json_get(raw_orgs, i));
                    if (o && o->type != JSON_NULL) json_append(orgs, o);
                    else json_free(o);
                }
            }
            json_set(out, "orgs", orgs);
            return out;
        }
    }
    if (status_code == 401)
        return cloud_error(409, "portal_signin_required",
                           "Your Nous Portal sign-in has expired. Sign in again.");
    if (status_code >= 400 || !b) {
        char msg[128];
        snprintf(msg, sizeof msg, "Nous Portal answered %d.", status_code);
        return cloud_error(502, "portal_error", msg);
    }
    json_t *out = json_object();
    json_t *agents = json_array();
    json_t *raw_agents = json_obj_get(b, "agents");
    if (raw_agents && raw_agents->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw_agents); i++) {
            json_t *a = json_get(raw_agents, i);
            if (json_is_object(a) && str_field(a, "id"))
                json_append(agents, w9_cloudagents_e05d38_agent(a));
        }
    }
    json_set(out, "agents", agents);
    json_set(out, "org", w9_cloudagents_e05d38_org(json_obj_get(b, "org")));
    return out;
}
