/*
 * test_kanban_format.c — Faithful port of hermes_cli/kanban.py pure helpers.
 */

#include "kanban_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

static kanban_task_t *make_task(void) {
    kanban_task_t *t = (kanban_task_t*)calloc(1, sizeof(kanban_task_t));
    t->id = strdup("K-1");
    t->title = strdup("Do the thing");
    t->status = strdup("running");
    t->assignee = strdup("alice");
    t->tenant = strdup("acme");
    t->created_at = 1700000000L;
    return t;
}

int main(void) {
    /* _fmt_ts */
    char *ts = kanban_fmt_ts(0); CHECK(strcmp(ts,"")==0, "fmt_ts(0) empty"); free(ts);
    char *ts2 = kanban_fmt_ts(1700000000L); CHECK(strlen(ts2)==16, "fmt_ts formatted len 16"); free(ts2);

    /* status icons */
    CHECK(strcmp(kanban_status_icon("done"),"\xe2\x9c\x93")==0, "icon done=✓");
    CHECK(strcmp(kanban_status_icon("blocked"),"\xe2\x8a\x98")==0, "icon blocked=⊘");
    CHECK(strcmp(kanban_status_icon("weird"),"?")==0, "icon unknown=?");

    /* _fmt_task_line */
    kanban_task_t *t = make_task();
    char *line = kanban_fmt_task_line(t);
    CHECK(strstr(line, "K-1") != NULL, "task line has id");
    CHECK(strstr(line, "running") != NULL, "task line has status");
    CHECK(strstr(line, "alice") != NULL, "task line has assignee");
    CHECK(strstr(line, "[acme]") != NULL, "task line has tenant");
    CHECK(strstr(line, "Do the thing") != NULL, "task line has title");
    free(line);

    /* _task_to_dict */
    char *js = kanban_task_to_json(t);
    CHECK(strstr(js, "\"id\": \"K-1\"") != NULL, "json id");
    CHECK(strstr(js, "\"status\": \"running\"") != NULL, "json status");
    CHECK(strstr(js, "\"tenant\": \"acme\"") != NULL, "json tenant");
    CHECK(strstr(js, "\"created_at\": 1700000000") != NULL, "json created_at");
    CHECK(strstr(js, "\"skills\": [") != NULL, "json skills array present");
    free(js);
    kanban_task_free(t);

    /* _run_state_kwargs */
    char *r0 = kanban_run_state_kwargs(NULL, NULL); CHECK(strcmp(r0,"{}")==0, "run_state {}"); free(r0);
    char *r1 = kanban_run_state_kwargs("x","y"); CHECK(strcmp(r1,"{\"state_type\": \"x\", \"state_name\": \"y\"}")==0, "run_state both"); free(r1);
    char *r2 = kanban_run_state_kwargs("x", NULL); CHECK(r2==NULL, "run_state mismatched -> NULL");

    /* _parse_workspace_flag */
    char *kind=NULL, *path=NULL, *err=NULL;
    CHECK(kanban_parse_workspace_flag("", &kind, &path, &err)==0 && strcmp(kind,"scratch")==0, "ws empty -> scratch");
    free(kind); free(path); free(err); kind=path=err=NULL;
    CHECK(kanban_parse_workspace_flag("worktree", &kind, &path, &err)==0 && strcmp(kind,"worktree")==0 && path==NULL, "ws worktree");
    free(kind); free(path); free(err); kind=path=err=NULL;
    setenv("HOME","/home/test",1);
    CHECK(kanban_parse_workspace_flag("dir:~/proj", &kind, &path, &err)==0 && strcmp(kind,"dir")==0 && strcmp(path,"/home/test/proj")==0, "ws dir:~/proj expands ~");
    free(kind); free(path); free(err); kind=path=err=NULL;
    CHECK(kanban_parse_workspace_flag("dir:", &kind, &path, &err)==-1 && err!=NULL, "ws dir: no path -> error");
    free(kind); free(path); free(err); kind=path=err=NULL;
    CHECK(kanban_parse_workspace_flag("bogus", &kind, &path, &err)==-1, "ws bogus -> error");
    free(kind); free(path); free(err);

    /* _parse_branch_flag */
    char *b = kanban_parse_branch_flag(NULL, &err); CHECK(b==NULL, "branch NULL -> NULL"); free(err); err=NULL;
    b = kanban_parse_branch_flag("feature-x", &err); CHECK(b && strcmp(b,"feature-x")==0, "branch ok"); free(b); free(err); err=NULL;
    b = kanban_parse_branch_flag("  ", &err); CHECK(b==NULL && err!=NULL, "branch empty -> error"); free(err); err=NULL;
    b = kanban_parse_branch_flag("-bad", &err); CHECK(b==NULL, "branch leading dash -> error"); free(err); err=NULL;
    b = kanban_parse_branch_flag("has space", &err); CHECK(b==NULL, "branch whitespace -> error"); free(err);

    /* _parse_duration */
    CHECK(kanban_parse_duration(NULL)==-2, "dur NULL -> -2");
    CHECK(kanban_parse_duration("")==-2, "dur empty -> -2");
    CHECK(kanban_parse_duration("30")==30, "dur bare int");
    CHECK(kanban_parse_duration("5m")==300, "dur 5m");
    CHECK(kanban_parse_duration("2h")==7200, "dur 2h");
    CHECK(kanban_parse_duration("1d")==86400, "dur 1d");
    CHECK(kanban_parse_duration("1.5h")==5400, "dur 1.5h");
    CHECK(kanban_parse_duration("abc")==-1, "dur malformed -> -1");
    CHECK(kanban_parse_duration("10x")==-1, "dur bad unit -> -1");

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
