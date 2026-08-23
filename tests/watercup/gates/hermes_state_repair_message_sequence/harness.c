/* gate: hermes_state_repair_message_sequence — drives the REAL
 * src/agent/hermes_state/hermes_state_repair.c::
 * hermes_state_repair_message_sequence (linked via extra_src).
 * LIVE twin: agent/agent_runtime_helpers.py::repair_message_sequence.
 *
 * Input per cup: JSON array of simple message dicts {role, content,
 * tool_call_id, tool_call_ids, finish_reason, reasoning_content, codex}.
 * Rungs: B1 non-empty / B2 consecutive-assistant run / B3 has tool rows /
 * B4 consecutive-user run. V = "<repairs>|<canonical msg dump>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_state_db.h"

int hermes_state_repair_message_sequence(repair_msg_t *msgs, int *count);

static char *dup_str(const char *s) { return s ? strdup(s) : NULL; }

static void dump_msgs(char *buf, size_t cap, const repair_msg_t *m, int n,
                      int repairs) {
    size_t off = 0;
    off += (size_t)snprintf(buf + off, cap - off, "%d|", repairs);
    for (int i = 0; i < n && off < cap - 32; i++) {
        int interim = m[i].codex_items ||
                      (m[i].finish_reason &&
                       strcmp(m[i].finish_reason, "incomplete") == 0);
        off += (size_t)snprintf(buf + off, cap - off, "%s;%s;%s;%s;%s;%s;%d#",
                                m[i].role ? m[i].role : "",
                                m[i].content ? m[i].content : "",
                                m[i].tool_call_id ? m[i].tool_call_id : "",
                                m[i].tool_call_ids ? m[i].tool_call_ids : "",
                                m[i].finish_reason ? m[i].finish_reason : "",
                                m[i].reasoning_content ? m[i].reasoning_content : "",
                                interim ? 1 : 0);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s repair <messages-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("repair_message_sequence");
        /* Parse the argv as libjson array of objects */
        extern int rs_parse(const char *, repair_msg_t *, int max);
        repair_msg_t msgs[64];
        memset(msgs, 0, sizeof msgs);
        int n = rs_parse(argv[i], msgs, 64);
        int nonempty = n > 0;
        WCUP_BRANCH(nonempty);                                    /* B1 */
        int adj_asst = 0, has_tool = 0, adj_user = 0;
        for (int k = 1; k < n; k++) {
            const char *a = msgs[k - 1].role ? msgs[k - 1].role : "";
            const char *b = msgs[k].role ? msgs[k].role : "";
            if (strcmp(a, "assistant") == 0 && strcmp(b, "assistant") == 0)
                adj_asst = 1;
            if (strcmp(a, "user") == 0 && strcmp(b, "user") == 0)
                adj_user = 1;
        }
        for (int k = 0; k < n; k++)
            if (msgs[k].role && strcmp(msgs[k].role, "tool") == 0)
                has_tool = 1;
        WCUP_BRANCH(adj_asst);                                    /* B2 */
        WCUP_BRANCH(has_tool);                                    /* B3 */
        WCUP_BRANCH(adj_user);                                    /* B4 */

        int count = n;
        int repairs = hermes_state_repair_message_sequence(msgs, &count);
        char valbuf[8192];
        dump_msgs(valbuf, sizeof valbuf, msgs, count, repairs);
        WCUP_VALSTR(valbuf);                                      /* V */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("repair_message_sequence");
        for (int k = 0; k < n; k++) {
            free(msgs[k].role); free(msgs[k].content);
            free(msgs[k].tool_call_id); free(msgs[k].tool_call_ids);
            free(msgs[k].finish_reason); free(msgs[k].reasoning_content);
        }
    }
    return 0;
}

/* Minimal argv -> repair_msg_t parser over the libjson API (kept local so
 * the traced path stays exactly the port under test). */
#include "hermes_json.h"
int rs_parse(const char *text, repair_msg_t *out, int max) {
    json_t *arr = json_parse(text, NULL);
    if (!arr || arr->type != JSON_ARRAY) return 0;
    int n = 0;
    for (size_t i = 0; i < arr->c.count && n < max; i++) {
        json_t *o = arr->c.items[i];
        if (!o || o->type != JSON_OBJECT) continue;
        json_t *v;
        v = json_obj_get(o, "role");
        out[n].role = dup_str(v && v->type == JSON_STRING ? v->str_val : NULL);
        v = json_obj_get(o, "content");
        out[n].content = dup_str(v && v->type == JSON_STRING ? v->str_val : NULL);
        v = json_obj_get(o, "tool_call_id");
        out[n].tool_call_id = dup_str(v && v->type == JSON_STRING ? v->str_val : NULL);
        v = json_obj_get(o, "tool_call_ids");
        out[n].tool_call_ids = dup_str(v && v->type == JSON_STRING ? v->str_val : NULL);
        v = json_obj_get(o, "finish_reason");
        out[n].finish_reason = dup_str(v && v->type == JSON_STRING ? v->str_val : NULL);
        v = json_obj_get(o, "reasoning_content");
        out[n].reasoning_content = dup_str(v && v->type == JSON_STRING ? v->str_val : NULL);
        v = json_obj_get(o, "codex");
        out[n].codex_items = v && v->type == JSON_BOOL && v->bool_val;
        n++;
    }
    json_free(arr);
    return n;
}
