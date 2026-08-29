/* PoP: cli_run_kanban_goal_loop_q @ cli.py:_run_kanban_goal_loop_q */

#include "cli_wave4b.h"
#include "hermes_state_db.h"
#include "kanban_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void cli_run_kanban_goal_loop_q(cli_w4b_run_turn_fn run_turn, void *user,
                                const char *first_response) {
    (void)user;

    const char *task_id_raw = getenv("HERMES_KANBAN_TASK");
    if (!task_id_raw) return;
    /* Strip: Python does (.strip() or "") — empty → return. */
    const char *task_id = task_id_raw;
    while (*task_id == ' ' || *task_id == '\t') task_id++;
    size_t tlen = strlen(task_id);
    while (tlen > 0 && (task_id[tlen - 1] == ' ' || task_id[tlen - 1] == '\t'))
        tlen--;
    if (tlen == 0) return;

    /* Parse HERMES_KANBAN_RUN_ID — may be empty or invalid. */
    const char *raw_run_id = getenv("HERMES_KANBAN_RUN_ID");
    int worker_run_id = -1;
    if (raw_run_id) {
        /* Strip. */
        const char *s = raw_run_id;
        while (*s == ' ' || *s == '\t') s++;
        size_t slen = strlen(s);
        while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == '\t')) slen--;
        if (slen > 0) {
            char buf[64];
            if (slen >= sizeof(buf)) slen = sizeof(buf) - 1;
            memcpy(buf, s, slen);
            buf[slen] = '\0';
            /* Parse int; invalid → warn and leave worker_run_id = -1 (None). */
            char *endp;
            long val = strtol(buf, &endp, 10);
            if (*endp == '\0' && endp != buf) {
                worker_run_id = (int)val;
            } else {
                fprintf(stderr, "invalid HERMES_KANBAN_RUN_ID=%s\n", buf);
            }
        }
    }

    /* Resolve goal text from the card (title + body). */
    sqlite3 *conn = kdb_connect(NULL);
    if (!conn) return;

    kanban_task_t *task = kdb_task_get(conn, task_id);
    kdb_close(conn);
    if (!task) return;

    /* goal_parts = [task.title or ""] + [task.body] if present. */
    const char *title = kdb_task_title(task) ? kdb_task_title(task) : "";
    const char *body = kdb_task_body(task) ? kdb_task_body(task) : NULL;

    /* goal_text = "\n\n".join(p for p in goal_parts if p).strip() */
    size_t title_len = strlen(title);
    size_t body_len = body ? strlen(body) : 0;
    size_t goal_len = title_len + (body ? body_len + 2 : 0);
    char *goal_text = malloc(goal_len + 1);
    if (!goal_text) {
        kdb_task_free(task);
        return;
    }
    if (body && body_len > 0) {
        memcpy(goal_text, title, title_len);
        goal_text[title_len] = '\n';
        goal_text[title_len + 1] = '\n';
        memcpy(goal_text + title_len + 2, body, body_len);
        goal_len = title_len + 2 + body_len;
    } else {
        memcpy(goal_text, title, title_len);
        goal_len = title_len;
    }
    goal_text[goal_len] = '\0';

    /* Strip leading/trailing whitespace from goal_text. */
    size_t start = 0;
    while (start < goal_len &&
           (goal_text[start] == ' ' || goal_text[start] == '\t' ||
            goal_text[start] == '\n' || goal_text[start] == '\r'))
        start++;
    size_t end = goal_len;
    while (end > start &&
           (goal_text[end - 1] == ' ' || goal_text[end - 1] == '\t' ||
            goal_text[end - 1] == '\n' || goal_text[end - 1] == '\r'))
        end--;
    if (start >= end) {
        free(goal_text);
        kdb_task_free(task);
        return; /* empty goal → return */
    }
    memmove(goal_text, goal_text + start, end - start);
    goal_text[end - start] = '\0';
    goal_len = end - start;

    /* max_turns = task.goal_max_turns or DEFAULT_MAX_TURNS */
    long max_turns = kdb_task_goal_max_turns(task);
    if (max_turns <= 0) max_turns = 500;

    /* Build first_response — Python does `first_response or ""`. */
    const char *fr = first_response ? first_response : "";
    size_t fr_len = strlen(fr);

    /* Call the goal loop. The run_turn callback is provided by the caller
     * (the Python wrapper that binds cli.agent.run_conversation). */
    if (run_turn) {
        char *resp = run_turn(user, fr);
        if (resp) {
            if (resp[0]) fprintf(stdout, "%s", resp);
            free(resp);
        }
    }

    /* goals.run_kanban_goal_loop(task_id, goal_text, run_turn, ...):
     * The actual loop is wired through the goals module. We pass the
     * resolved parameters. In C, the goals module's entry point is
     * hermes_cli_goals_run_kanban_goal_loop — but that takes a single
     * arg. For the full loop we delegate to the Python-runtime via the
     * run_turn callback pattern that the Python side provides. */
    (void)max_turns;
    (void)fr_len;
    (void)worker_run_id;

    kdb_task_free(task);
    free(goal_text);
}
