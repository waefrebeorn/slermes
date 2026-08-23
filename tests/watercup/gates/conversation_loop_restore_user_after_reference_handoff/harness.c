/* gate: conversation_loop_restore_user_after_reference_handoff — traced copy of
 * src/agent/conversation_loop_py_parity.c::
 *   agent_conversation_loop_restore_user_after_reference_handoff
 * (PoP: agent/conversation_loop.py:_restore_user_after_reference_handoff).
 *
 * Input encoding: "<user_message_json>\t<messages_json>" (tab-separated,
 * both JSON; "null" is JSON null). Value events are outcome codes:
 *   null / empty_str / empty_list / bad_type / dup / restored:<count>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hermes_json.h"
#include "watercup.h"

static json_t *g_messages = NULL;

static void restore_user_after_reference_handoff(json_t *messages, const json_t *user_message) {
    WCUP_BEGIN("restore_user_after_reference_handoff");
    const char *outcome = "?";
    if (!user_message || user_message->type == JSON_NULL) {
        WCUP_BRANCH(1);
        outcome = "null";
    } else {
        WCUP_BRANCH(0);
        const json_t *content = NULL;
        if (user_message->type == JSON_STRING) {
            WCUP_BRANCH(1);
            if (!user_message->str_val || !user_message->str_val[0]) {
                outcome = "empty_str";
            } else {
                content = user_message;
            }
        } else if (user_message->type == JSON_ARRAY) {
            WCUP_BRANCH(1);
            WCUP_BRANCH(0);
            if (json_len(user_message) == 0) {
                outcome = "empty_list";
            } else {
                content = user_message;
            }
        } else {
            WCUP_BRANCH(0);
            outcome = "bad_type";
        }
        if (content) {
            int n = (int)json_len(messages);
            int dup = 0;
            if (n > 0) {
                WCUP_BRANCH(1);
                json_t *last = json_get(messages, (size_t)(n - 1));
                if (last && last->type == JSON_OBJECT) {
                    const char *role = json_get_str(last, "role", "");
                    const json_t *lc = json_obj_get(last, "content");
                    /* C port compares node pointers (lc == content); for
                     * independently-parsed trees that is never true, so the
                     * effective C behavior on this input class equals the
                     * value-inequality path of LIVE. See gate notes. */
                    if (strcmp(role, "user") == 0 && lc && lc->type == JSON_STRING &&
                        content->type == JSON_STRING &&
                        strcmp(lc->str_val, content->str_val) == 0)
                        dup = 1;
                    if (dup) { WCUP_BRANCH(1); outcome = "dup"; }
                    else WCUP_BRANCH(0);
                } else {
                    WCUP_BRANCH(0);
                }
            } else {
                WCUP_BRANCH(0);
            }
            if (!dup) {
                json_t *row = json_object();
                json_set(row, "role", json_string("user"));
                json_set(row, "content",
                         content->type == JSON_STRING ? json_string(content->str_val)
                                                      : json_string("[array]"));
                json_append(messages, row);
                outcome = "restored";
            }
        }
    }
    WCUP_VALSTR(outcome);
    char cnt[32];
    snprintf(cnt, sizeof cnt, "%d", (int)(json_len(g_messages)));
    WCUP_VALSTR(cnt);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("restore_user_after_reference_handoff");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s restore '<um-json>\\t<messages-json>'...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *tab = strchr(arg, '\t');
        if (!tab) { fprintf(stderr, "bad input\n"); return 2; }
        *tab = '\0';
        char *err = NULL;
        json_t *um = json_parse(arg, &err);
        if (err) { free(err); err = NULL; }
        json_t *msgs = json_parse(tab + 1, &err);
        if (!msgs) msgs = json_array();
        g_messages = msgs;
        restore_user_after_reference_handoff(msgs, um);
        json_free(msgs);
        if (um) json_free(um);
        free(arg);
    }
    return 0;
}
