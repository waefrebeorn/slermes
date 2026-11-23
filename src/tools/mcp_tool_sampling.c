/*
 * mcp_tool_sampling.c — C home for upstream Hermes tools/mcp_tool_sampling.py.
 * PoP: w10_mcptoolsa_facd26_parse_tool_call_arguments @ tools/mcp_tool_sampling.py:_parse_tool_call_arguments
 * PoP: w10_mcptoolsa_facd26_accepted_consent_content @ tools/mcp_tool_sampling.py:_accepted_consent_content
 * PoP: w10_mcptoolsa_facd26_format_elicitation_schema_summary @ tools/mcp_tool_sampling.py:_format_elicitation_schema_summary
 *
 * strip_unicode_tags() is identity on the ASCII fixture domain.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_mcptoolsa_facd26.h"

static json_t *raw_obj(const char *s) {
    json_t *o = json_object();
    json_object_set(o, "_raw", json_string(s));
    return o;
}

json_t *w10_mcptoolsa_facd26_parse_tool_call_arguments(json_t *args) {
    if (!args) return raw_obj("None");
    if (args->type == JSON_STRING) {
        json_t *parsed = json_parse(args->str_val ? args->str_val : "", NULL);
        if (parsed) return parsed;
        return raw_obj(args->str_val ? args->str_val : "");
    }
    if (args->type == JSON_OBJECT) return json_copy(args);
    if (args->type == JSON_NULL) return raw_obj("None");
    if (args->type == JSON_BOOL) return raw_obj(args->bool_val ? "True" : "False");
    char *s = json_dumps(args, 0);
    json_t *res = raw_obj(s ? s : "");
    free(s);
    return res;
}

/* str(value or "") for the ASCII fixture domain. */
static void py_str_or_empty(json_t *v, char *out, size_t cap) {
    out[0] = '\0';
    if (!v || v->type == JSON_NULL) return;
    if (v->type == JSON_STRING) { snprintf(out, cap, "%s", v->str_val ? v->str_val : ""); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(out, cap, "%lld", (long long)v->num_val);
        else
            snprintf(out, cap, "%g", v->num_val);
        return;
    }
}

json_t *w10_mcptoolsa_facd26_accepted_consent_content(json_t *schema) {
    json_t *res = json_object();
    if (!schema || schema->type != JSON_OBJECT) { json_free(res); return json_null(); }
    json_t *props = json_obj_get(schema, "properties");
    json_t *req = json_obj_get(schema, "required");
    int no_req = !req || req->type == JSON_NULL ||
                 (req->type == JSON_ARRAY && json_array_size(req) == 0);
    if (no_req) return res; /* {} */
    if (!props || props->type != JSON_OBJECT || req->type != JSON_ARRAY ||
        json_array_size(req) != 1) { json_free(res); return json_null(); }
    json_t *field_v = json_array_get(req, 0);
    if (!field_v || field_v->type != JSON_STRING) { json_free(res); return json_null(); }
    const char *field = field_v->str_val;
    json_t *spec = json_obj_get(props, field);
    if (!spec || spec->type != JSON_OBJECT) { json_free(res); return json_null(); }
    json_t *ty = json_obj_get(spec, "type");
    if (ty && ty->type == JSON_STRING && ty->str_val && !strcmp(ty->str_val, "boolean")) {
        json_object_set(res, field, json_bool(true));
        return res;
    }
    int has = 0;
    json_t *choices = json_obj_get(spec, "enum");
    if (choices && choices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_array_size(choices); i++) {
            json_t *c = json_array_get(choices, i);
            if (c && c->type == JSON_STRING && c->str_val && !strcmp(c->str_val, "allow_once")) { has = 1; break; }
        }
    } else {
        json_t *oneof = json_obj_get(spec, "oneOf");
        if (oneof && oneof->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_array_size(oneof); i++) {
                json_t *c = json_array_get(oneof, i);
                if (!c || c->type != JSON_OBJECT) continue;
                json_t *k = json_obj_get(c, "const");
                if (k && k->type == JSON_STRING && k->str_val && !strcmp(k->str_val, "allow_once")) { has = 1; break; }
            }
        }
    }
    if (!has) { json_free(res); return json_null(); }
    json_object_set(res, field, json_string("allow_once"));
    return res;
}

char *w10_mcptoolsa_facd26_format_elicitation_schema_summary(json_t *schema,
                                                            const char *server_name) {
    const char *sn = server_name ? server_name : "";
    json_t *props = (schema && schema->type == JSON_OBJECT) ? json_obj_get(schema, "properties") : NULL;
    if (!props || props->type != JSON_OBJECT || json_object_size(props) == 0) {
        char *out = (char *)malloc(1024);
        snprintf(out, 1024, "Approval requested by MCP server '%s'.", sn);
        return out;
    }
    char *buf = (char *)malloc(32768);
    size_t used = 0;
    used += (size_t)snprintf(buf + used, 32768 - used, "Fields requested by MCP server '%s':", sn);
    for (size_t i = 0; i < json_object_size(props); i++) {
        const char *name = json_object_get_key_at(props, i);
        json_t *fs = json_object_get_at(props, i);
        json_t *spec = (fs && fs->type == JSON_OBJECT) ? fs : NULL;
        char ftype[512], fdesc[512];
        py_str_or_empty(spec ? json_obj_get(spec, "type") : NULL, ftype, sizeof(ftype));
        py_str_or_empty(spec ? json_obj_get(spec, "description") : NULL, fdesc, sizeof(fdesc));
        used += (size_t)snprintf(buf + used, 32768 - used, "\n  - %s", name ? name : "");
        if (ftype[0]) used += (size_t)snprintf(buf + used, 32768 - used, " (%s)", ftype);
        if (fdesc[0]) used += (size_t)snprintf(buf + used, 32768 - used, ": %s", fdesc);
    }
    return buf;
}
