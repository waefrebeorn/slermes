/*
 * kanban_specify.c — canonical C11 port of hermes_cli/kanban_specify.py.
 */

#include "kanban_db.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_kanban_specify_list_triage_ids @ hermes_cli/kanban_specify.py:list_triage_ids */
int hermes_cli_kanban_specify_list_triage_ids(const char *arg) {
    /* Python: triage task ids (tenant-filtered). Arg = "tenant\tids" (tab
     * sep, empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_kanban_specify_specify_task @ hermes_cli/kanban_specify.py:specify_task
 * Specify one triage task and promote it to todo. Never raises for expected
 * failure modes — returns 0 with *reason_out set (caller frees) so the --all
 * sweep continues past individual failures. The LLM leg routes through the
 * auxiliary client; when no aux surface is linked the outcome mirrors the
 * Python import-failure path ("auxiliary client unavailable"). */
int hermes_cli_kanban_specify_specify_task(const char *task_id,
                                           const char *author,
                                           char **reason_out)
{
    if (reason_out) *reason_out = NULL;
    if (!task_id || !*task_id) {
        if (reason_out) *reason_out = strdup("unknown task id");
        return 0;
    }
    /* Python: with kb.connect_closing() as conn: task = kb.get_task(...) */
    sqlite3 *conn = kdb_connect(NULL);
    kanban_task_t *task = conn ? kdb_task_get(conn, task_id) : NULL;
    if (!task) {
        if (conn) kdb_close(conn);
        if (reason_out) *reason_out = strdup("unknown task id");
        return 0;
    }
    const char *status = kdb_task_status(task);
    if (!status || strcmp(status, "triage") != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "task is not in triage (status=%s)",
                 status ? status : "None");
        if (reason_out) *reason_out = strdup(buf);
        free(task);
        if (conn) kdb_close(conn);
        return 0;
    }
    free(task);
    if (conn) kdb_close(conn);
    /* Python: from agent.auxiliary_client import call_llm — SDK/async dispatch
     * has no C surface (hermes_api_coverage: auxiliary_client.py N/A). */
    if (reason_out) *reason_out = strdup("auxiliary client unavailable");
    (void)author;
    return 0;
}
