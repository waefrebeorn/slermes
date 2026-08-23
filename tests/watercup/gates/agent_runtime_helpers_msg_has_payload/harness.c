/* gate: agent_runtime_helpers_msg_has_payload — traced C copy of
 * src/agent/agent_message_payload.c::msg_has_payload (+ content_list_has_payload).
 * LIVE twin: agent/agent_runtime_helpers.py::_msg_has_payload.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: content is a string        -> B2 nonblank -> result
 *   B3: content is an array        -> per-block loop:
 *        B4 block is object; B5 type=="text"; B6 text nonblank; B7 truthy block
 *   B8: content not in (None, "")  -> result true
 *   B9 tool_calls truthy / B10 reasoning_content nonblank /
 *   B11 reasoning|reasoning_details / B12 codex_*_items
 *   V : final boolean as "true"/"false"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include <json.h>

/* linked real module needs hermes_log but the traced path never reaches it;
 * provide the symbol locally so the link succeeds without liblogger. */
void hermes_log(int lvl, const char *src, const char *fmt, ...)
{ (void)lvl; (void)src; (void)fmt; }

static bool json_is_truthy(const json_t *v)
{
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:  return v->c.count > 0;
        default:          return v->c.count > 0;
    }
}

static bool json_str_nonblank(const json_t *v)
{
    if (!v || v->type != JSON_STRING || !v->str_val) return false;
    for (const char *p = v->str_val; *p; p++)
        if (!isspace((unsigned char)*p)) return true;
    return false;
}

static bool json_is_string(const json_t *v)
{
    return v && v->type == JSON_STRING;
}

static bool trace_content_list(const json_t *container, wcup_t *wc)
{
    for (size_t i = 0; i < container->c.count; i++) {
        wcup_loop(wc);
        json_t *block = container->c.items[i];
        if (!block) continue;
        if (block->type == JSON_OBJECT) {
            wcup_branch(wc, 1);                   /* B4 block is object */
            json_t *ttype = json_obj_get(block, "type");
            int is_text = json_is_string(ttype) && ttype->str_val &&
                          strcmp(ttype->str_val, "text") == 0;
            wcup_branch(wc, is_text);             /* B5 type=="text" */
            if (is_text) {
                int nonblank = json_str_nonblank(json_obj_get(block, "text"));
                wcup_branch(wc, nonblank);        /* B6 text nonblank */
                if (nonblank) return true;
                continue;
            }
            return true;                          /* B7 other typed block */
        }
        wcup_branch(wc, 0);                       /* B4 non-object */
        if (json_is_truthy(block)) { wcup_branch(wc, 1); return true; }
        wcup_branch(wc, 0);
    }
    return false;
}

static bool trace_msg_has_payload(const json_t *msg)
{
    WCUP_BEGIN("msg_has_payload");
    bool result = false;

    if (!msg || msg->type != JSON_OBJECT) {
        WCUP_BRANCH(1);
        result = false;
        goto out;
    }
    WCUP_BRANCH(0);

    json_t *content = json_obj_get(msg, "content");
    /* B1: content is string */
    int c_is_str = content && content->type == JSON_STRING;
    WCUP_BRANCH(c_is_str);
    if (c_is_str) {
        int nb = json_str_nonblank(content);
        WCUP_BRANCH(nb);                          /* B2 */
        result = !!nb;
        goto out;
    }
    /* B3: content is array */
    int c_is_arr = content && content->type == JSON_ARRAY;
    WCUP_BRANCH(c_is_arr);
    if (c_is_arr) {
        result = trace_content_list(content, &_wc);
        goto out;
    }
    /* B8: content not in (None, "") — dict/number/bool pass */
    {
        int not_none_empty = content && content->type != JSON_NULL &&
                             content->type != JSON_STRING;
        WCUP_BRANCH(not_none_empty);
        if (not_none_empty) { result = true; goto out; }
    }
    WCUP_BRANCH(json_is_truthy(json_obj_get(msg, "tool_calls")));       /* B9 */
    if (json_is_truthy(json_obj_get(msg, "tool_calls"))) { result = true; goto out; }

    {
        json_t *rc = json_obj_get(msg, "reasoning_content");
        int rc_nb = json_str_nonblank(rc);
        WCUP_BRANCH(rc_nb);                       /* B10 */
        if (rc_nb) { result = true; goto out; }
    }
    {
        int r = json_is_truthy(json_obj_get(msg, "reasoning"));
        WCUP_BRANCH(r);
        int rd = !r && json_is_truthy(json_obj_get(msg, "reasoning_details"));
        WCUP_BRANCH(rd);                          /* B11a/B11b */
        if (r || rd) { result = true; goto out; }
    }
    {
        int cm = json_is_truthy(json_obj_get(msg, "codex_message_items"));
        WCUP_BRANCH(cm);
        int cri = !cm && json_is_truthy(json_obj_get(msg, "codex_reasoning_items"));
        WCUP_BRANCH(cri);                         /* B12a/B12b */
        if (cm || cri) { result = true; goto out; }
    }
    result = false;

out:
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msg_has_payload");
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msg_has_payload <json-msg>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        json_t *m = json_parse(argv[i], NULL);
        if (!m) { fprintf(stderr, "bad json %s\n", argv[i]); return 2; }
        trace_msg_has_payload(m);
        json_free(m);
    }
    return 0;
}
