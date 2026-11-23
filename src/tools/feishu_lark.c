/*
 * feishu_lark.c — C home for upstream Hermes tools/feishu_lark.py.
 *
 * PoP: w6_feishulark_1501ff_raw_body @ tools/feishu_lark.py:raw_body
 * PoP: w6_feishulark_1501ff_response_data @ tools/feishu_lark.py:response_data
 *
 * The lark_oapi client, contextvar and availability probe are runtime plumbing
 * and are not reimplemented. The tested behaviour is the pure response
 * unpacking: parse response.raw.content as a JSON object (or None) and the
 * raw-body-first / typed-data-fallback resolution of response_data(). The
 * fixture injects the response shape explicitly so the gate is deterministic.
 */
#include "noc_w6_feishulark_1501ff.h"
#include "hermes_json.h"

#include <string.h>

/* Python truthiness of a decoded JSON value. */
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

/* raw_body(): json.loads(raw.content) if raw is truthy and has content,
 * resulting dict or NULL (JSON null in the harness). */
static json_t *raw_body_node(const char *content, int has_raw)
{
    json_t *body;
    if (!has_raw || !content)
        return NULL;
    body = json_parse(content, NULL);
    if (!body || body->type != JSON_OBJECT) {
        json_free(body);
        return NULL;
    }
    return body;
}

struct json_t *w6_feishulark_1501ff_raw_body(const char *content, int has_raw)
{
    json_t *body = raw_body_node(content, has_raw);
    return body ? body : json_null();
}

/* response_data(): prefer raw body's non-empty "data", else typed dict,
 * else vars(typed) (injected as data_vars), else the empty/falsy data. */
struct json_t *w6_feishulark_1501ff_response_data(const char *content, int has_raw,
                                                  const struct json_t *typed_data,
                                                  const struct json_t *data_vars)
{
    json_t *body = raw_body_node(content, has_raw);
    json_t *data = (body && body->type == JSON_OBJECT) ? json_obj_get(body, "data") : NULL;
    json_t *res = NULL;

    if (py_truthy(data)) {
        res = json_copy(data);
    } else if (typed_data && typed_data->type == JSON_OBJECT) {
        res = json_copy(typed_data);
    } else if (py_truthy(typed_data) && data_vars && data_vars->type == JSON_OBJECT) {
        res = json_copy(data_vars);
    } else if (data) {
        res = json_copy(data);
    } else {
        res = json_object();
    }
    json_free(body);
    return res;
}
