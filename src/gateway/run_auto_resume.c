/*
 * run_auto_resume.c — C home for upstream Hermes gateway/run_auto_resume.py.
 *
 * PoP: w9_runautores_9a4e1c_session_route @ gateway/run_auto_resume.py:_session_route
 *
 * _session_route binds a claimed child's completion back to its parent
 * conversation.  The one imported collaborator (gateway.run._parse_session_key)
 * is supplied by the caller as an already-computed dict (the `parsed` seam);
 * the rest of the function is ported statement-for-statement.
 */
#include "noc_w9_runautores_9a4e1c.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for JSON values. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

/* str(v) for the scalar values _session_route coerces. */
static const char *py_str(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) return "";
    if (!py_truthy(v)) return "";   /* str(v or ""): falsy scalars collapse to "" */
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL)   return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else
            snprintf(buf, n, "%g", v->num_val);
        return buf;
    }
    return "";
}

struct json_t *w9_runautores_9a4e1c_session_route(const struct json_t *unit,
                                                  const struct json_t *parsed) {
    json_t *out = json_object();
    json_t *task = json_obj_get(unit, "task");
    if (!task || task->type != JSON_OBJECT) task = NULL;

    char b_key[64], b_oid[64], b_pchat[64], b_pplat[64], b_pctype[64];
    char b_pthread[64], b_uid[64], b_uname[64], b_scope[64], b_sess[64], b_ui[64];

    /* key = str(unit.get("origin_session") or "") */
    const char *key = py_str(json_obj_get(unit, "origin_session"), b_key, sizeof b_key);
    /* parsed = _parse_session_key(key) or {}  (injected seam) */
    if (!parsed || parsed->type != JSON_OBJECT) parsed = NULL;

    json_t *oidv = json_obj_get(unit, "origin_session_id");
    int has_oid = py_truthy(oidv);
    const char *oid = py_str(oidv, b_oid, sizeof b_oid);

    const char *pchat   = py_str(json_obj_get(parsed, "chat_id"),   b_pchat,   sizeof b_pchat);
    const char *pplat   = py_str(json_obj_get(parsed, "platform"),  b_pplat,   sizeof b_pplat);
    const char *pctype  = py_str(json_obj_get(parsed, "chat_type"), b_pctype,  sizeof b_pctype);
    const char *pthread = py_str(json_obj_get(parsed, "thread_id"), b_pthread, sizeof b_pthread);

    /* chat_id = str(origin_session_id or "") or parsed.get("chat_id","") */
    const char *chat_id = has_oid ? oid : pchat;
    const char *platform = has_oid ? "api_server" : pplat;

    json_set(out, "platform",  json_string(platform));
    json_set(out, "chat_id",   json_string(chat_id));
    json_set(out, "chat_type", json_string(pctype));
    json_set(out, "thread_id", json_string(pthread));
    json_set(out, "user_id",   json_string(task ? py_str(json_obj_get(task, "user_id"),   b_uid,   sizeof b_uid)   : ""));
    json_set(out, "user_name", json_string(task ? py_str(json_obj_get(task, "user_name"), b_uname, sizeof b_uname) : ""));
    json_set(out, "scope_id",  json_string(task ? py_str(json_obj_get(task, "scope_id"),  b_scope, sizeof b_scope)  : ""));
    json_set(out, "session_key",    json_string(key));
    json_set(out, "session_id",     json_string(py_str(json_obj_get(unit, "parent_session_id"),   b_sess, sizeof b_sess)));
    json_set(out, "ui_session_id",  json_string(py_str(json_obj_get(unit, "origin_ui_session_id"), b_ui,   sizeof b_ui)));
    if (has_oid)
        json_set(out, "session_history_delivery", json_string("1"));

    return out;
}
