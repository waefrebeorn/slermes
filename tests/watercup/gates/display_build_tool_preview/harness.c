/* gate: display_build_tool_preview — traces the REAL C port
 * src/hermes_cli/display_tool_preview.c::cli_agent_display__build_tool_preview
 * (linked via extra_src; result folded as a V-event). The harness emits the
 * SAME branch ladder the port walks, computed from the parsed args; the port
 * itself supplies the output bytes.
 *
 * LIVE twin: agent/display.py::build_tool_preview.
 *
 * Input argv token format: "tool_name|max_len|args_json"
 * ('|' may not appear inside args_json).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1 args empty / B2 parse ok / B3 args object
 *   tool chain (one rung each): delegate_task, process, todo,
 *   terminal|execute_code, read_file, session_search, memory,
 *   send_message, generic fallback
 *   + per-tool sub-branches, then V(result).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"
#include "hermes_cli/display.h"

static char g_res[4096];

static void wc_build_tool_preview(const char *token) {
    WCUP_BEGIN("build_tool_preview");
    /* split token: tool|max_len|args_json */
    char tool[128] = ""; int max_len = 80; char ajs[3072] = "";
    {
        const char *p1 = strchr(token, '|');
        const char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (p1 && p2) {
            size_t n = (size_t)(p1 - token);
            memcpy(tool, token, n < sizeof tool ? n : sizeof tool - 1);
            max_len = atoi(p1 + 1);
            strncpy(ajs, p2 + 1, sizeof ajs - 1);
        }
    }

    json_t *args = NULL;

    if (!ajs[0]) {                                        /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char *err = NULL;
        args = json_parse(ajs, &err);
        if (!args) {                                      /* B2 */
            if (err) free(err);
            WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(1);
        if (args->type != JSON_OBJECT) {                  /* B3 */
            WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(1);
    }

    /* ── tool dispatch chain (mirrors the port's else-if order) ── */
    if (strcmp(tool, "delegate_task") == 0) {
        WCUP_BRANCH(1);
        json_t *tasks = json_obj_get(args, "tasks");
        if (tasks && tasks->type == JSON_ARRAY) {         /* B tasks */
            WCUP_BRANCH(1);
            size_t n = json_len(tasks);
            int dict_count = 0;
            for (size_t i = 0; i < n; i++) {
                WCUP_LOOP_TICK();
                json_t *t = json_get(tasks, i);
                if (!t || t->type != JSON_OBJECT) continue;
                dict_count++;
            }
            (void)dict_count;
        } else {
            WCUP_BRANCH(0);
            json_t *goal = json_obj_get(args, "goal");
            if (!goal || goal->type == JSON_NULL) {       /* B goal missing */
                WCUP_BRANCH(1);
                goto out;
            }
            WCUP_BRANCH(0);
            char *gp = cli_agent_display__oneline(goal->type == JSON_STRING ? goal->str_val : "");
            bool nonempty = gp && gp[0];                  /* B gp nonempty */
            free(gp);
            WCUP_BRANCH(nonempty);
        }
    } else if (strcmp(tool, "process_manage") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(1);
        json_t *action = json_obj_get(args, "action");
        json_t *sid = json_obj_get(args, "session_id");
        json_t *data = json_obj_get(args, "data");
        json_t *timeout_val = json_obj_get(args, "timeout");
        bool is_wait = action && action->type == JSON_STRING &&
                       strcmp(action->str_val, "wait") == 0;
        if (sid && sid->type == JSON_STRING && sid->str_val[0]) {        /* B sid */
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }
        if (data && data->type == JSON_STRING && data->str_val[0]) {     /* B data */
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
        }
        if (timeout_val && is_wait) {                                     /* B timeout+wait */
            WCUP_BRANCH(1);
            if (timeout_val->type == JSON_NUMBER) {                       /* B number */
                WCUP_BRANCH(1);
            } else {
                WCUP_BRANCH(0);
            }
        } else {
            WCUP_BRANCH(0);
        }
    } else if (strcmp(tool, "todo_list") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
        json_t *todos = json_obj_get(args, "todos");
        json_t *merge = json_obj_get(args, "merge");
        int merge_on = merge && (merge->type == JSON_BOOL ? merge->bool_val : (merge->type == JSON_NUMBER && merge->num_val != 0.0));
        if (todos == NULL || todos->type == JSON_NULL) {   /* B todos none */
            WCUP_BRANCH(1);
        } else if (merge_on) {                             /* B merge */
            WCUP_BRANCH(0); WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0); WCUP_BRANCH(0);
        }
    } else if (strcmp(tool, "terminal") == 0 || strcmp(tool, "execute_code") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
        const char *key = strcmp(tool, "execute_code") == 0 ? "code" : "command";
        json_t *cmd = json_obj_get(args, key);
        if (!cmd || cmd->type != JSON_STRING) {            /* B cmd str */
            WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(1);
        char *sum = cli_agent_display__summarize_shell_command(cmd->str_val);
        bool nonempty = sum && sum[0];                     /* B sum nonempty */
        free(sum);
        WCUP_BRANCH(nonempty);
    } else if (strcmp(tool, "read_file") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
        json_t *p = json_obj_get(args, "path");
        if (!p || p->type != JSON_STRING) p = json_obj_get(args, "file");
        if (!p || p->type != JSON_STRING) p = json_obj_get(args, "filepath");
        if (!p || p->type != JSON_STRING) {                /* B path-ish */
            WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(1);
        char *line_lbl = cli_agent_display__read_file_line_label(args);
        bool has = line_lbl && line_lbl[0];                /* B line lbl */
        free(line_lbl);
        WCUP_BRANCH(has);
    } else if (strcmp(tool, "session_search") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
        WCUP_BRANCH(0); WCUP_BRANCH(1);
    } else if (strcmp(tool, "memory") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
        json_t *action = json_obj_get(args, "action");
        const char *act = action && action->type == JSON_STRING ? action->str_val : "";
        if (strcmp(act, "add") == 0) {                     /* B add */
            WCUP_BRANCH(1);
        } else if (strcmp(act, "replace") == 0 || strcmp(act, "remove") == 0) { /* B rep/rm */
            WCUP_BRANCH(0); WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0); WCUP_BRANCH(0);
        }
    } else if (strcmp(tool, "send_message") == 0) {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
        json_t *msg = json_obj_get(args, "message");
        char *mm = cli_agent_display__oneline(msg && msg->type == JSON_STRING ? msg->str_val : "");
        bool long_msg = strlen(mm) > 20;                   /* B long msg */
        free(mm);
        WCUP_BRANCH(long_msg);
    } else {
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
        WCUP_BRANCH(0);
        /* generic fallback: primary_args table + fallback keys */
        static const struct { const char *tool, *key; } TBL[] = {
            {"terminal","command"},{"web_search","query"},{"web_extract","urls"},
            {"read_file","path"},{"write_file","path"},{"patch","path"},
            {"search_files","pattern"},{"browser_navigate","url"},
            {"browser_click","ref"},{"browser_type","text"},
            {"image_generate","prompt"},{"text_to_speech","text"},
            {"vision_analyze","question"},{"skill_view","name"},
            {"skills_list","category"},{"cronjob_manage","action"},
            {"execute_code","code"},{"delegate_task","goal"},
            {"clarify","question"},{"skill_manage","name"},{NULL,NULL}
        };
        const char *key = NULL;
        for (int i = 0; TBL[i].tool; i++)
            if (strcmp(TBL[i].tool, tool) == 0) { key = TBL[i].key; break; }
        if (!key) {                                        /* B primary key */
            WCUP_BRANCH(0);
            static const char *fb[] = {"query","text","command","path","name","prompt","code","goal",NULL};
            for (int i = 0; fb[i]; i++) {
                WCUP_LOOP_TICK();
                if (json_obj_get(args, fb[i])) { key = fb[i]; break; }
            }
        } else {
            WCUP_BRANCH(1);
        }
        if (!key || !json_obj_get(args, key)) {            /* B resolvable */
            WCUP_BRANCH(0);
            goto out;
        }
        WCUP_BRANCH(1);
        json_t *value = json_obj_get(args, key);
        if (value->type == JSON_ARRAY) {                   /* B array */
            WCUP_BRANCH(1); WCUP_BRANCH(0);
        } else if (value->type == JSON_STRING) {           /* B string */
            WCUP_BRANCH(0); WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0); WCUP_BRANCH(0);
        }
    }

out:;
    char *result = cli_agent_display__build_tool_preview(tool, ajs, max_len);
    snprintf(g_res, sizeof g_res, "%s", result ? result : "");
    free(result);
    if (args) json_free(args);
    {
        char cup[32];
        WCUP_VAL(g_res, (int)strlen(g_res));
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("build_tool_preview");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s build_tool_preview <token>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_build_tool_preview(argv[i]);
    return 0;
}
