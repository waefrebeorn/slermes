/*
 * todo_hydrate.c — Port of Python run_agent.AIAgent._hydrate_todo_store
 * MIT License — WuBu Slermes Project
 *
 * Scan session messages for the most recent todo tool response and replay it to
 * reconstruct in-memory todo state on session resume. (GAP 1 from the original
 * hermes_gap_fixes.c monolith — split into a self-contained module.)
 */

#include "todo_hydrate.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int todo_hydrate_from_context(void *vstate) {
    agent_state_t *state = (agent_state_t *)vstate;
    if (!state || !state->messages || state->message_count == 0)
        return 0;

    int restored = 0;

    /* Walk backwards to find the last todo tool response */
    for (size_t i = state->message_count; i > 0; i--) {
        message_t *msg = state->messages[i - 1];
        if (!msg || msg->role != MSG_TOOL)
            continue;
        if (!msg->content || !*msg->content)
            continue;

        /* Check if content contains a "todos" key */
        if (!strstr(msg->content, "todos"))
            continue;

        /* Try parsing as JSON */
        json_t *root = json_parse(msg->content, NULL);
        if (!root || root->type != JSON_OBJECT) {
            json_free(root);
            continue;
        }

        json_t *todos = json_object_get(root, "todos");
        if (todos && todos->type == JSON_ARRAY && json_array_count(todos) > 0) {
            /* Serialize the todos array and write to disk store */
            char *json_str = json_serialize(todos);
            if (json_str) {
                char todo_path[1024];
                const char *home = getenv("HERMES_HOME");
                if (!home) home = getenv("HOME");
                if (home) {
                    snprintf(todo_path, sizeof(todo_path), "%s/.hermes/todos.json", home);
                    FILE *fp = fopen(todo_path, "w");
                    if (fp) {
                        fprintf(fp, "%s", json_str);
                        fclose(fp);
                        restored = (int)json_array_count(todos);
                    }
                }
                free(json_str);
            }
        }

        json_free(root);
        if (restored > 0) break;
    }

    if (restored > 0) {
        hermes_log(LOG_INFO, "gap_fixes", "Restored %d todo item(s) from history", restored);
    }

    return restored;
}
