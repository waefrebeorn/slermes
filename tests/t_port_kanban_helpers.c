/*
 * t_port_kanban_helpers.c — faithful verification harness for the PURE kanban
 * helpers ported from hermes_cli/kanban.py (implemented in
 * src/hermes_cli/kanban_format.c). One op per line; exercises the REAL C
 * functions with REAL inputs (env vars for profile/author/run-id). The Python
 * oracle (tests/sta_oracle_kanban_helpers.py) recomputes the expected result
 * from the same fixture; the runner diffs them as JSON.
 *
 * To keep the diff trivially stable, task_dict / run_state emit FLAT top-level
 * fields (not a nested JSON string), and the oracle mirrors exactly that.
 *
 * Fixture line grammar (one op per line):
 *   fmt_ts <ts>
 *   fmt_task_line <id>|<status>|<assignee>|<tenant>|<title>
 *   task_dict <id>|<title>|<body>|<assignee>|<status>|<priority>|<tenant>|<wspath>|<branch>|<project>|<created_by>|<created_at>|<started_at>|<completed_at>|<result>|<skills>|<max_retries>|<session_id>|<wtid>|<stepkey>
 *   run_state <state_type>|<state_name>      (empty field before | = absent)
 *   ws_flag <value>
 *   branch_flag <value>
 *   profile_author
 *   duration <value>
 *   worker_run <task_id>
 *
 * Fields are '|'-separated so titles/bodies may contain spaces.
 */

#include "kanban_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* emit a top-level JSON string value: "text" (raw quotes, valid top-level). */
static void emit_json_string(const char *s)
{
    putchar('"');
    for (const char *p = s ? s : ""; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c < 0x20) fprintf(stdout, "\\u%04x", c);
        else putchar((int)c);
    }
    putchar('"');
}

/* split `rest` on '|' into up to n fields; empties preserved. */
static int split_pipe(char *rest, char **out, int n)
{
    for (int i = 0; i < n; i++) out[i] = (char *)"";
    if (!rest) return 0;
    int cnt = 0;
    char *p = rest;
    while (*p && cnt < n) {
        out[cnt++] = p;
        char *colon = strchr(p, '|');
        if (!colon) break;
        *colon = '\0';
        p = colon + 1;
    }
    return cnt;
}

/* parse a skills JSON array string ("[\"a\",\"b\"]") into a char** + count.
 * Returns malloc'd array (caller frees elements + array) or NULL. */
static char **parse_skills(const char *s, int *nout)
{
    *nout = 0;
    if (!s || !*s) return NULL;
    const char *p = s;
    while (*p && *p != '[') p++;
    if (*p != '[') return NULL;
    p++;
    char **arr = NULL; int n = 0, cap = 0;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == ',') p++;
        if (*p == ']') break;
        if (*p != '"') break;
        p++;
        const char *start = p;
        while (*p && *p != '"') p++;
        if (!*p) break;
        size_t len = (size_t)(p - start);
        if (n >= cap) { cap = cap ? cap * 2 : 4; arr = realloc(arr, cap * sizeof(char *)); }
        arr[n] = malloc(len + 1);
        memcpy(arr[n], start, len); arr[n][len] = '\0';
        n++;
        p++;
    }
    *nout = n;
    return arr;
}

static kanban_task_t *build_task(char **a)
{
    kanban_task_t *t = calloc(1, sizeof(*t));
    t->id = strdup(a[0]); t->title = strdup(a[1]); t->body = strdup(a[2]);
    t->assignee = strdup(a[3]); t->status = strdup(a[4]);
    t->priority = strdup(a[5]); t->tenant = strdup(a[6]);
    t->workspace_kind = strdup(""); t->workspace_path = strdup(a[7]);
    t->branch_name = strdup(a[8]); t->project_id = strdup(a[9]);
    t->created_by = strdup(a[10]);
    t->created_at = a[11][0] ? atol(a[11]) : 0;
    t->started_at = a[12][0] ? atol(a[12]) : 0;
    t->completed_at = a[13][0] ? atol(a[13]) : 0;
    t->result = strdup(a[14]);
    t->max_retries = a[16][0] ? atoi(a[16]) : 0;
    t->session_id = strdup(a[17]);
    t->workflow_template_id = strdup(a[18]);
    t->current_step_key = strdup(a[18]);
    t->model_override = strdup("");
    t->provider_override = strdup("");
    t->skills = parse_skills(a[15], &t->skills_n);
    return t;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;
        char *op = line;
        char *rest = strchr(op, ' ');
        if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; } else rest = (char *)"";

        char *a[32];

        if (strcmp(op, "fmt_ts") == 0) {
            long ts = rest[0] ? atol(rest) : 0;
            char *out = kanban_fmt_ts(ts);
            printf("{\"op\":\"fmt_ts\",\"ts\":%ld,\"out\":", ts);
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out);

        } else if (strcmp(op, "fmt_task_line") == 0) {
            split_pipe(rest, a, 5);
            kanban_task_t *t = calloc(1, sizeof(*t));
            t->id = strdup(a[0]); t->status = strdup(a[1]);
            t->assignee = strdup(a[2]); t->tenant = strdup(a[3]);
            t->title = strdup(a[4]);
            char *out = kanban_fmt_task_line(t);
            printf("{\"op\":\"fmt_task_line\",\"out\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out); kanban_task_free(t);

        } else if (strcmp(op, "task_dict") == 0) {
            split_pipe(rest, a, 19);
            kanban_task_t *t = build_task(a);
            printf("{\"op\":\"task_dict\"");
            printf(",\"id\":"); emit_json_string(t->id);
            printf(",\"title\":"); emit_json_string(t->title);
            printf(",\"body\":"); emit_json_string(t->body);
            printf(",\"assignee\":"); emit_json_string(t->assignee);
            printf(",\"status\":"); emit_json_string(t->status);
            printf(",\"priority\":"); emit_json_string(t->priority);
            printf(",\"tenant\":"); emit_json_string(t->tenant);
            printf(",\"workspace_path\":"); emit_json_string(t->workspace_path);
            printf(",\"branch_name\":"); emit_json_string(t->branch_name);
            printf(",\"project_id\":"); emit_json_string(t->project_id);
            printf(",\"created_by\":"); emit_json_string(t->created_by);
            printf(",\"created_at\":%ld", t->created_at);
            printf(",\"started_at\":%ld", t->started_at);
            printf(",\"completed_at\":%ld", t->completed_at);
            printf(",\"result\":"); emit_json_string(t->result);
            printf(",\"max_retries\":%d", t->max_retries);
            printf(",\"session_id\":"); emit_json_string(t->session_id);
            printf(",\"workflow_template_id\":"); emit_json_string(t->workflow_template_id);
            printf(",\"current_step_key\":"); emit_json_string(t->current_step_key);
            printf(",\"skills\":[");
            for (int i = 0; i < t->skills_n; i++) {
                if (i) printf(",");
                emit_json_string(t->skills[i]);
            }
            printf("]");
            printf("}\n");
            kanban_task_free(t);

        } else if (strcmp(op, "run_state") == 0) {
            split_pipe(rest, a, 2);
            const char *st = a[0][0] ? a[0] : NULL;
            const char *sn = a[1][0] ? a[1] : NULL;
            int has_t = st != NULL, has_n = sn != NULL;
            printf("{\"op\":\"run_state\"");
            if (has_t != has_n) {
                printf(",\"state\":null");
            } else if (!has_t) {
                printf(",\"state\":{}");
            } else {
                printf(",\"state_name\":"); emit_json_string(sn);
                printf(",\"state_type\":"); emit_json_string(st);
            }
            printf("}\n");

        } else if (strcmp(op, "ws_flag") == 0) {
            char *kind = NULL, *path = NULL, *err = NULL;
            int r = kanban_parse_workspace_flag(rest[0] ? rest : "", &kind, &path, &err);
            printf("{\"op\":\"ws_flag\",\"ok\":%s,\"kind\":", r == 0 ? "true" : "false");
            emit_json_string(kind ? kind : "");
            printf(",\"path\":"); emit_json_string(path ? path : "");
            printf(",\"err\":"); emit_json_string(err ? err : "");
            printf("}\n");
            free(kind); free(path); free(err);

        } else if (strcmp(op, "branch_flag") == 0) {
            char *err = NULL;
            char *b = kanban_parse_branch_flag(rest[0] ? rest : NULL, &err);
            printf("{\"op\":\"branch_flag\",\"ok\":%s,\"branch\":", b ? "true" : "false");
            emit_json_string(b ? b : "");
            printf(",\"err\":"); emit_json_string(err ? err : "");
            printf("}\n");
            free(b); free(err);

        } else if (strcmp(op, "profile_author") == 0) {
            char *out = kanban_profile_author();
            printf("{\"op\":\"profile_author\",\"author\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out);

        } else if (strcmp(op, "duration") == 0) {
            long r = kanban_parse_duration(rest[0] ? rest : "");
            printf("{\"op\":\"duration\",\"seconds\":%ld}\n", r);

        } else if (strcmp(op, "worker_run") == 0) {
            long r = kanban_worker_run_id_for(rest[0] ? rest : "");
            printf("{\"op\":\"worker_run\",\"task_id\":");
            emit_json_string(rest);
            printf(",\"run_id\":%ld}\n", r);
        }
    }
    fclose(f);
    return 0;
}
