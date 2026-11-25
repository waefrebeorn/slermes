/* JSON-lines harness for the w31_anthropicm_b31360_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_anthropicm_b31360_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* flag: int from a JSON value, Python bool() semantics. */
static int flag_of(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY: return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    default: return 0;
    }
}

int main(void) {
    static char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "model_name")) {
            out = w31_anthropicm_b31360_p2_model_name(json_get_str(in, "model", NULL),
                                                      flag_of(json_obj_get(in, "preserve_dots")));
        } else if (!strcmp(func, "tools")) {
            out = w31_anthropicm_b31360_p2_convert_tools(json_obj_get(in, "tools"));
        } else if (!strcmp(func, "normalize_schema")) {
            out = w31_anthropicm_b31360_p2_normalize_schema(json_obj_get(in, "schema"));
        } else if (!strcmp(func, "content_part")) {
            out = w31_anthropicm_b31360_p2_content_part(json_obj_get(in, "part"));
        } else if (!strcmp(func, "content_list")) {
            out = w31_anthropicm_b31360_p2_content_list(json_obj_get(in, "content"));
        } else if (!strcmp(func, "content_parts_blocks")) {
            out = w31_anthropicm_b31360_p2_content_parts_blocks(json_obj_get(in, "parts"));
        } else if (!strcmp(func, "image_block")) {
            out = w31_anthropicm_b31360_p2_image_block(json_get_str(in, "url", NULL));
        } else if (!strcmp(func, "sanitize_replay")) {
            out = w31_anthropicm_b31360_p2_sanitize_replay(json_obj_get(in, "block"));
        } else if (!strcmp(func, "split_blank")) {
            out = w31_anthropicm_b31360_p2_split_blank(json_obj_get(in, "blocks"));
        } else if (!strcmp(func, "is_blank")) {
            out = w31_anthropicm_b31360_p2_is_blank(json_obj_get(in, "block"));
        } else if (!strcmp(func, "safe_text")) {
            out = w31_anthropicm_b31360_p2_safe_text(json_obj_get(in, "text"));
        } else if (!strcmp(func, "sanitize_tool_id")) {
            out = w31_anthropicm_b31360_p2_sanitize_tool_id(json_obj_get(in, "id"));
        } else if (!strcmp(func, "parse_tool_args")) {
            out = w31_anthropicm_b31360_p2_parse_tool_args(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "tool_result_content")) {
            out = w31_anthropicm_b31360_p2_tool_result_content(json_obj_get(in, "message"));
        } else if (!strcmp(func, "route")) {
            out = w31_anthropicm_b31360_p2_route(json_get_str(in, "base_url", NULL),
                                                json_get_str(in, "model", NULL));
        } else if (!strcmp(func, "replays_prior")) {
            out = w31_anthropicm_b31360_p2_replays_prior(json_get_str(in, "route", NULL),
                                                        json_get_str(in, "model", NULL));
        } else if (!strcmp(func, "issuer_foreign")) {
            out = w31_anthropicm_b31360_p2_issuer_foreign(json_obj_get(in, "message"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
