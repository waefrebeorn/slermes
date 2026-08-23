/* gate: agent_message_sanitization_sanitize_messages_non_ascii — traced copy
 * of src/agent/message_sanitization_ports.c::msz_sanitize_messages_non_ascii
 * (core ladder), using the REAL linked msz_strip_non_ascii helper.
 * LIVE twin: agent/message_sanitization.py::_sanitize_messages_non_ascii.
 *
 * Input encoding (argv-safe): a JSON array of messages, passed verbatim.
 *
 * Branch ladder (twin.py mirrors EXACTLY), per message:
 *   L0 loop tick per message
 *   B1: msg is a dict/object
 *   B2: content is str            B3: str changed
 *   B4: content is list (elif)    then per part:
 *     L1 tick, B5: part dict, B6: text str, B7: text changed
 *   B8: name is str               B9: name changed
 *   B10: tool_calls is list       then per tc:
 *     L2 tick, B11: tc dict, B12: fn dict (get-with-{} default),
 *     B13: arguments str, B14: arguments changed
 *   per remaining key: L3 tick, B15: key in skip-set,
 *     B16: value str, B17: value changed
 *   VSTR(found as "0"/"1").
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

char *msz_strip_non_ascii(const char *text);

/* traced mirror of msz_str_field_changed (branch event emitted by caller) */
static bool wc_field_changed(json_t *obj, const char *key, bool *found) {
    json_t *node = json_obj_get(obj, key);
    bool is_str = node && node->type == JSON_STRING;
    if (!is_str) return false;
    char *s = msz_strip_non_ascii(node->str_val);
    bool changed = strcmp(s, node->str_val) != 0;
    if (changed) {
        json_set(obj, key, json_string(s));
        *found = true;
    }
    free(s);
    return changed;
}

static int key_in_skipset(const char *k) {
    return strcmp(k, "content") == 0 || strcmp(k, "name") == 0 ||
           strcmp(k, "tool_calls") == 0 || strcmp(k, "role") == 0;
}

static void wc_sanitize(const char *messages_json) {
    WCUP_BEGIN("sanitize_messages_non_ascii");
    bool found = false;
    json_t *messages = json_parse(messages_json, NULL);
    if (messages && messages->type == JSON_ARRAY) {
        size_t n = json_len(messages);
        for (size_t i = 0; i < n; i++) {                    /* L0 */
            WCUP_LOOP_TICK();
            json_t *msg = json_get(messages, i);
            bool is_obj = msg && msg->type == JSON_OBJECT;
            WCUP_BRANCH(is_obj);                            /* B1 */
            if (!is_obj) continue;

            json_t *content = json_obj_get(msg, "content");
            bool c_str = content && content->type == JSON_STRING;
            WCUP_BRANCH(c_str);                             /* B2 */
            if (c_str) {
                WCUP_BRANCH(wc_field_changed(msg, "content", &found)); /* B3 */
            } else {
                bool c_list = content && content->type == JSON_ARRAY;
                WCUP_BRANCH(c_list);                        /* B4 */
                if (c_list) {
                    size_t pn = json_len(content);
                    for (size_t j = 0; j < pn; j++) {       /* L1 */
                        WCUP_LOOP_TICK();
                        json_t *part = json_get(content, j);
                        bool p_obj = part && part->type == JSON_OBJECT;
                        WCUP_BRANCH(p_obj);                 /* B5 */
                        if (!p_obj) continue;
                        json_t *text = json_obj_get(part, "text");
                        bool t_str = text && text->type == JSON_STRING;
                        WCUP_BRANCH(t_str);                 /* B6 */
                        if (t_str)
                            WCUP_BRANCH(wc_field_changed(part, "text", &found)); /* B7 */
                    }
                }
            }

            /* name field */
            {
                json_t *name = json_obj_get(msg, "name");
                bool n_str = name && name->type == JSON_STRING;
                WCUP_BRANCH(n_str);                         /* B8 */
                if (n_str)
                    WCUP_BRANCH(wc_field_changed(msg, "name", &found)); /* B9 */
            }

            /* tool_calls[].function.arguments */
            json_t *tcs = json_obj_get(msg, "tool_calls");
            bool tcs_list = tcs && tcs->type == JSON_ARRAY;
            WCUP_BRANCH(tcs_list);                          /* B10 */
            if (tcs_list) {
                size_t tn = json_len(tcs);
                for (size_t j = 0; j < tn; j++) {           /* L2 */
                    WCUP_LOOP_TICK();
                    json_t *tc = json_get(tcs, j);
                    bool tc_obj = tc && tc->type == JSON_OBJECT;
                    WCUP_BRANCH(tc_obj);                    /* B11 */
                    if (!tc_obj) continue;
                    json_t *fn = json_obj_get(tc, "function");
                    /* Python get("function", {}): missing -> empty dict. */
                    bool fn_dict = (fn == NULL) || fn->type == JSON_OBJECT;
                    WCUP_BRANCH(fn_dict);                   /* B12 */
                    if (!fn_dict) continue;
                    json_t *args = fn ? json_obj_get(fn, "arguments") : NULL;
                    bool a_str = args && args->type == JSON_STRING;
                    WCUP_BRANCH(a_str);                     /* B13 */
                    if (a_str)
                        WCUP_BRANCH(wc_field_changed(fn, "arguments", &found)); /* B14 */
                }
            }

            /* remaining top-level string fields */
            size_t kn = msg->c.count;
            for (size_t k = 0; k < kn; k++) {               /* L3 */
                WCUP_LOOP_TICK();
                const char *key = msg->c.keys[k];
                if (!key) continue;
                bool skip = key_in_skipset(key) != 0;
                WCUP_BRANCH(skip);                          /* B15 */
                if (skip) continue;
                json_t *val = json_obj_get(msg, key);
                bool v_str = val && val->type == JSON_STRING;
                WCUP_BRANCH(v_str);                         /* B16 */
                if (v_str)
                    WCUP_BRANCH(wc_field_changed(msg, key, &found)); /* B17 */
            }
        }
    }
    if (messages) json_free(messages);

    char cup[32], res[8];
    snprintf(res, sizeof res, "%d", found ? 1 : 0);
    WCUP_VALSTR(res);
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sanitize_messages_non_ascii");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s sanitize_messages_non_ascii <messages-json>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) wc_sanitize(argv[i]);
    return 0;
}
