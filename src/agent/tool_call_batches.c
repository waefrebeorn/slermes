/*
 * tool_call_batches.c - C home for upstream Hermes agent/tool_call_batches.py.
 *
 * PoP: w9_toolcallba_449062_replace_native_tool_blocks @ agent/tool_call_batches.py:replace_native_tool_blocks
 * PoP: w9_toolcallba_449062_replace_native_tool_blocks @ agent/tool_call_batches.py:_anthropic_children
 * PoP: w9_toolcallba_449062_replace_native_tool_blocks @ agent/tool_call_batches.py:_bedrock_children
 *
 * coalesce_tool_call_id(child) is stubbed by the oracle to child.id; children are
 * plain records with function.name / function.arguments (a JSON string).
 */
#include "noc_w9_toolcallba_449062.h"
#include <string.h>

static const char *w9tb_cid(const json_t *child) {
    return json_get_str(child, "id", "");
}

static const char *w9tb_cname(const json_t *child) {
    json_t *fn = json_obj_get(child, "function");
    return json_get_str(fn, "name", "");
}

static json_t *w9tb_cinput(const json_t *child) {
    json_t *fn = json_obj_get(child, "function");
    const char *args = json_get_str(fn, "arguments", "{}");
    json_t *parsed = json_parse(args, NULL);
    return parsed ? parsed : json_null();
}

/* {**block, "id": cid, "name": name, "input": <parsed args>} */
static json_t *w9tb_anthropic_child(const json_t *block, const json_t *child) {
    json_t *out = json_copy(block);
    json_set(out, "id", json_string(w9tb_cid(child)));
    json_set(out, "name", json_string(w9tb_cname(child)));
    json_set(out, "input", w9tb_cinput(child));
    return out;
}

/* {**block, "toolUse": {**tool, "toolUseId": cid, "name": name, "input": ...}} */
static json_t *w9tb_bedrock_child(const json_t *block, const json_t *tool, const json_t *child) {
    json_t *inner = json_copy(tool);
    json_set(inner, "toolUseId", json_string(w9tb_cid(child)));
    json_set(inner, "name", json_string(w9tb_cname(child)));
    json_set(inner, "input", w9tb_cinput(child));
    json_t *out = json_copy(block);
    json_set(out, "toolUse", inner);
    return out;
}

static json_t *w9tb_expand_block(const json_t *block, json_t *replacements, int bedrock) {
    const char *lookup = NULL;
    json_t *tool = NULL;
    if (!bedrock) {
        const char *type = json_get_str(block, "type", "");
        if (!strcmp(type, "tool_use")) lookup = json_get_str(block, "id", "");
    } else {
        tool = json_obj_get(block, "toolUse");
        if (tool && tool->type == JSON_OBJECT) lookup = json_get_str(tool, "toolUseId", "");
    }
    json_t *result = json_array();
    if (!lookup) {
        json_append(result, json_copy(block));
        return result;
    }
    json_t *children = json_obj_get(replacements, lookup);
    if (!children || children->type != JSON_ARRAY) {
        json_append(result, json_copy(block));
        return result;
    }
    for (size_t i = 0; i < json_len(children); ++i) {
        json_t *child = json_get(children, i);
        if (bedrock) json_append(result, w9tb_bedrock_child(block, tool, child));
        else json_append(result, w9tb_anthropic_child(block, child));
    }
    return result;
}

json_t *w9_toolcallba_449062_replace_native_tool_blocks(json_t *provider_data, json_t *replacements) {
    json_t *out = provider_data ? json_copy(provider_data) : json_object();
    if (!replacements) replacements = json_object();
    static const char *const keys[2] = {"anthropic_content_blocks", "bedrock_content_blocks"};
    for (int k = 0; k < 2; ++k) {
        json_t *blocks = json_obj_get(out, keys[k]);
        if (!blocks || blocks->type != JSON_ARRAY || json_len(blocks) == 0) continue;
        json_t *flat = json_array();
        for (size_t i = 0; i < json_len(blocks); ++i) {
            json_t *expanded = w9tb_expand_block(json_get(blocks, i), replacements, k == 1);
            for (size_t j = 0; j < json_len(expanded); ++j) {
                json_append(flat, json_copy(json_get(expanded, j)));
            }
            json_free(expanded);
        }
        json_set(out, keys[k], flat);
    }
    return out;
}
