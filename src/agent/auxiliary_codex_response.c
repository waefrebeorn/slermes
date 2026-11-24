/*
 * auxiliary_codex_response.c — C port of upstream Hermes agent/auxiliary_codex_response.py.
 *
 * PoP: w18_auxiliaryc_cbc55f_parse @ agent/auxiliary_codex_response.py:_parse_codex_final_response
 *
 * The shared Responses normalizer (_normalize_codex_response / _lower_or_none)
 * lives in a heavyweight sibling module and is a controlled seam here: the
 * normalizer's (content, tool_calls, finish_reason) result is supplied as the
 * `norm` argument. Everything this module itself does is ported verbatim:
 * empty output handling, the aux-only "incomplete" -> "length" remap
 * (never for completed tool calls), text_parts/tool_calls extraction and
 * usage re-keying with getattr/dict fallbacks.
 */
#include "noc_w18_auxiliaryc_cbc55f.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool json_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default:          return false;
    }
}

json_t *w18_auxiliaryc_cbc55f_parse(const json_t *final, const char *issuer_kind,
                                    const char *issuer_model, const json_t *norm) {
    (void)issuer_kind;   /* consumed by the (seamed) shared normalizer */
    (void)issuer_model;  /* consumed by the (seamed) shared normalizer */
    const json_t *fobj = (final && final->type == JSON_OBJECT) ? final : NULL;
    const char *status = fobj ? json_get_str(fobj, "status", NULL) : NULL;
    const json_t *nobj = (norm && norm->type == JSON_OBJECT) ? norm : NULL;
    const char *fr = nobj ? json_get_str(nobj, "finish_reason", "stop") : "stop";
    json_t *content = nobj ? json_obj_get(nobj, "content") : NULL;
    json_t *tool_calls = nobj ? json_obj_get(nobj, "tool_calls") : NULL;

    json_t *text_parts = json_array();
    if (json_truthy(content)) json_append(text_parts, json_copy(content));

    /* finish_reason != "tool_calls" and (finish_reason == "incomplete" or
     * lower(status) == "incomplete") -> "length" for aux consumers. */
    if (strcmp(fr, "tool_calls") != 0 &&
        (strcmp(fr, "incomplete") == 0 ||
         (status && strcasecmp(status, "incomplete") == 0)))
        fr = "length";

    /* usage = None unless resp_usage is truthy; _u(k) is effectively
     * resp_usage.get(k, 0) because dict objects have no such attributes. */
    json_t *usage = NULL;
    json_t *u = fobj ? json_obj_get(fobj, "usage") : NULL;
    if (json_truthy(u)) {
        usage = json_object();
        static const char *kin[3] = {"input_tokens", "output_tokens", "total_tokens"};
        static const char *kout[3] = {"prompt_tokens", "completion_tokens", "total_tokens"};
        for (int i = 0; i < 3; i++) {
            json_t *val = (u->type == JSON_OBJECT) ? json_obj_get(u, kin[i]) : NULL;
            json_set(usage, kout[i], val ? json_copy(val) : json_number(0));
        }
    }

    json_t *out = json_array();
    json_append(out, text_parts);
    json_append(out, tool_calls ? json_copy(tool_calls) : json_null());
    json_append(out, usage ? usage : json_null());
    json_append(out, json_string(fr));
    return out;
}
