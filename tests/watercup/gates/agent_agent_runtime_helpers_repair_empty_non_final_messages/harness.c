/* gate: agent_agent_runtime_helpers_repair_empty_non_final_messages — traced
 * copy of src/agent/agent_message_payload.c::repair_empty_non_final_messages
 * (+ msg_has_payload ladder). The REAL port is ALSO linked via extra_src and
 * called per input as an internal consistency check.
 * LIVE twin: agent/agent_runtime_helpers.py::repair_empty_non_final_messages.
 *
 * Input encoding: one JSON array of message objects per argv item.
 * Value event: serialized repaired array + healed flag.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1: len(messages) >= 2
 *   LOOP idx over 0..last-1:
 *     B2: msg is dict
 *     B3: role in {assistant, user}
 *     B4: not _msg_has_payload(msg) -> heal (placeholder content)
 *   B5: healed > 0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

json_t *repair_empty_non_final_messages(const json_t *messages);

/* stub: logging not on the traced path */
void hermes_log(int level, const char *cat, const char *fmt, ...) {
    (void)level; (void)cat; (void)fmt;
}

/* Python truthiness for a JSON-parsed value (copied from the port). */
static bool json_is_truthy(const json_t *v)
{
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:    return false;
        case JSON_BOOL:    return v->bool_val;
        case JSON_NUMBER:  return v->num_val != 0.0;
        case JSON_STRING:  return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:   return v->c.count > 0;
        case JSON_OBJECT:  return v->c.count > 0;
    }
    return false;
}

static bool c_msg_has_payload(const json_t *msg) {
    /* mirrors msg_has_payload in agent_message_payload.c — traced copy */
    if (!msg || msg->type != JSON_OBJECT) return false;
    json_t *content = json_obj_get(msg, "content");
    if (content && content->type == JSON_STRING) {
        for (const char *s = content->str_val; *s; s++)
            if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return true;
        return false;
    }
    if (content && content->type == JSON_ARRAY) {
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *block = content->c.items[i];
            if (block && block->type == JSON_OBJECT) {
                json_t *ttype = json_obj_get(block, "type");
                if (ttype && ttype->type == JSON_STRING &&
                    strcmp(ttype->str_val, "text") == 0) {
                    json_t *text = json_obj_get(block, "text");
                    if (text && text->type == JSON_STRING)
                        for (const char *s = text->str_val; *s; s++)
                            if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
                                return true;
                    continue;
                }
                return true;
            }
            /* Python truthiness per block */
            int truthy = 0;
            if (block) {
                switch (block->type) {
                case JSON_NULL: truthy = 0; break;
                case JSON_BOOL: truthy = block->bool_val ? 1 : 0; break;
                case JSON_STRING: truthy = block->str_val[0] != '\0'; break;
                case JSON_NUMBER: truthy = block->num_val != 0.0; break;
                default: truthy = block->c.count > 0; break;
                }
            }
            if (truthy)
                return true;
        }
        return false;
    }
    if (content && content->type != JSON_NULL && content->type != JSON_STRING)
        return true;
    if (json_is_truthy(json_obj_get(msg, "tool_calls"))) return true;
    {
        json_t *rc = json_obj_get(msg, "reasoning_content");
        if (rc && rc->type == JSON_STRING)
            for (const char *s = rc->str_val; *s; s++)
                if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return true;
    }
    if (json_is_truthy(json_obj_get(msg, "reasoning")) ||
        json_is_truthy(json_obj_get(msg, "reasoning_details"))) return true;
    if (json_is_truthy(json_obj_get(msg, "codex_message_items")) ||
        json_is_truthy(json_obj_get(msg, "codex_reasoning_items"))) return true;
    return false;
}

static void build_val(const json_t *arr, int healed, char *out, size_t outsz)
{
    size_t n = (arr && arr->type == JSON_ARRAY) ? arr->c.count : 0;
    size_t w = 0;
    w += (size_t)snprintf(out + w, outsz - w, "n=%zu|h=%d|", n, healed);
    for (size_t i = 0; arr && i < n; i++) {
        json_t *m = arr->c.items[i];
        const char *role = "-";
        int wh = 0;
        if (m && m->type == JSON_OBJECT) {
            json_t *r = json_obj_get(m, "role");
            if (r && r->type == JSON_STRING) role = r->str_val;
            json_t *ct = json_obj_get(m, "content");
            if (ct && ct->type == JSON_STRING &&
                strcmp(ct->str_val, "[response interrupted]") == 0)
                wh = 1;
        }
        w += (size_t)snprintf(out + w, outsz - w, "%s%s:%d",
                              i ? "," : "", role, wh);
    }
}

static void trace_one(const char *messages_json) {
    WCUP_BEGIN("repair_empty_non_final_messages");
    int healed = 0;
    json_t *out = NULL;
    json_t *msgs = messages_json ? json_parse(messages_json, NULL) : NULL;
    size_t n = (msgs && msgs->type == JSON_ARRAY) ? msgs->c.count : 0;
    if (!msgs || msgs->type != JSON_ARRAY || n < 2) {    /* B1 */
        WCUP_BRANCH(0);
        out = json_copy(msgs);
    } else {
        WCUP_BRANCH(1);
        out = json_copy(msgs);
        size_t last = n - 1;
        for (size_t idx = 0; idx < last; idx++) {        /* LOOP */
            WCUP_LOOP_TICK();
            json_t *msg = out->c.items[idx];
            if (!msg || msg->type != JSON_OBJECT) {      /* B2 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            json_t *role = json_obj_get(msg, "role");
            int role_ok = role && role->type == JSON_STRING &&
                          (strcmp(role->str_val, "assistant") == 0 ||
                           strcmp(role->str_val, "user") == 0);
            if (!role_ok) {                              /* B3 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            if (c_msg_has_payload(msg)) {                /* B4 */
                WCUP_BRANCH(0);
                continue;
            }
            WCUP_BRANCH(1);
            json_set(msg, "content", json_string("[response interrupted]"));
            healed++;
        }
        WCUP_BRANCH(healed > 0);                         /* B5 */
    }
    char val[4096];
    build_val(out, healed, val, sizeof val);
    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("repair_empty_non_final_messages");
    if (out) json_free(out);
    if (msgs) json_free(msgs);

    /* consistency: REAL linked port must produce an identical repaired array */
    {
        json_t *again = messages_json ? json_parse(messages_json, NULL) : NULL;
        json_t *real = repair_empty_non_final_messages(again);
        int rheal = 0;
        if (real && real->type == JSON_ARRAY)
            for (size_t i = 0; i < real->c.count; i++) {
                json_t *m = real->c.items[i];
                if (m && m->type == JSON_OBJECT) {
                    json_t *ct = json_obj_get(m, "content");
                    if (ct && ct->type == JSON_STRING &&
                        strcmp(ct->str_val, "[response interrupted]") == 0)
                        rheal++;
                }
            }
        char rv[4096];
        build_val(real, rheal, rv, sizeof rv);
        if (strcmp(rv, val) != 0) {
            fprintf(stderr, "real port disagrees on '%.80s': %s vs %s\n",
                    messages_json, rv, val);
            exit(3);
        }
        if (real) json_free(real);
        if (again) json_free(again);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s repair_empty_non_final_messages <messages-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
