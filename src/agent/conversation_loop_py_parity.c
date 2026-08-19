/* Slermes C port — agent/conversation_loop.py (remaining gaps)
 *
 * Faithful ports of the reference-handoff helpers (#80622). The C
 * conversation loop threads messages as a json_t array of {role, content}
 * objects (see context_compressor_pure.c), so these operate on json_t.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"
#include "context_compressor_pure.h"

/* Forward-declared here: appends a {role, content} user row to *messages.
 * Mirrors Python append_message(messages, {"role":"user","content":...}). */
static void cl_append_user_message(json_t *messages, const json_t *content)
{
    if (!messages || !content) return;
    json_t *row = json_new_object();
    if (!row) return;
    json_set(row, "role", json_new_string("user"));
    json_set(row, "content", (json_t *)content); /* shared, not cloned */
    json_array_append(messages, row);
}

/* PoP: agent_conversation_loop_restore_user_after_reference_handoff @ agent/conversation_loop.py:_restore_user_after_reference_handoff */
/* Re-append this turn's real user ask when compaction left only a handoff.
 * Returns true when a restore append happened. user_message is a JSON_STRING
 * or JSON_ARRAY node (Python str | list); NULL mirrors Python None. */
bool agent_conversation_loop_restore_user_after_reference_handoff(json_t *messages, const json_t *user_message)
{
    if (!user_message) return false;
    const json_t *content = NULL;
    if (user_message->type == JSON_STRING) {
        if (user_message->str_val == NULL || user_message->str_val[0] == '\0')
            return false;
        content = user_message;
    } else if (user_message->type == JSON_ARRAY) {
        if (json_len(user_message) == 0) return false;
        content = user_message;
    } else {
        return false;
    }

    int n = (int)json_len(messages);
    if (n > 0) {
        json_t *last = json_get(messages, (size_t)(n - 1));
        if (last && last->type == JSON_OBJECT) {
            const char *role = json_get_str(last, "role", "");
            const json_t *lc = json_obj_get(last, "content");
            if (strcmp(role, "user") == 0 && lc == content)
                return false;
        }
    }
    cl_append_user_message(messages, content);
    return true;
}

/* PoP: agent_conversation_loop_should_skip_model_call_for_reference_handoff @ agent/conversation_loop.py:_should_skip_model_call_for_reference_handoff */
/* Guard post-compaction continues against sole-handoff active turns (#80622). */
bool agent_conversation_loop_should_skip_model_call_for_reference_handoff(json_t *messages, const json_t *user_message)
{
    if (!cc_reference_handoff_would_drive_next_model_call(messages))
        return false;
    if (agent_conversation_loop_restore_user_after_reference_handoff(messages, user_message)) {
        /* The restored ask is an actionable non-synthetic user row appended
         * after the handoff — by construction the handoff no longer drives. */
        return false;
    }
    return true;
}
