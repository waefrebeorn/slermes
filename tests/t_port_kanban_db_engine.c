/*
 * t_port_kanban_db_engine.c — oracle harness for hermes_cli/kanban_db.py
 *
 * Contract-B oracle: reads a fixture JSON from argv[1] describing a sequence
 * of engine operations against a temp board DB, executes them via the
 * ported engine, and prints a JSON summary. The Python oracle
 * (sta_oracle_kanban_db_engine.py) runs the SAME fixture against the LIVE
 * Python module. The runner diffs the two outputs byte-for-byte.
 *
 * This proves the C engine is a faithful port, not just "compiles".
 *
 * Minimal includes. Uses libjson (via hermes_json.h) for both parsing the
 * fixture and emitting the result.
 */

#include "kanban_db.h"
#include "hermes_json.h"
#include "hermes_cli/sqlite_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* tiny arena of malloc'd strings to free at end (not strictly needed) */
static void jprint_str(const char *s) {
    if (!s) { printf("null"); return; }
    printf("\"");
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
    }
    printf("\"");
}

int main(int argc, char **argv)
{
    /* Oracle isolation: each side (C harness + Python oracle) must start from
     * a FRESH, PRIVATE board so their outputs are comparable. The shared
     * runner TMPH would otherwise let the Python side observe the C side's
     * writes. Use a per-process subdir under the oracle home. */
    const char *base = getenv("SLERMES_HOME");
    if (!base || !*base) base = getenv("HOME");
    char priv[1024];
    if (base && *base) {
        snprintf(priv, sizeof(priv), "%s/eng_%ld", base, (long)getpid());
        mkdir(priv, 0755);
        setenv("HERMES_KANBAN_HOME", priv, 1);
    }
    if (argc < 2) { printf("{}\n"); return 1; }
    char *txt = NULL;
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("{}\n"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    txt = malloc((size_t)sz + 1); fread(txt, 1, (size_t)sz, f); txt[sz] = '\0'; fclose(f);

    json_t *root = json_parse(txt, NULL);
    free(txt);
    if (!root) { printf("{}\n"); return 1; }

    const char *board = json_get_str(root, "board", NULL);
    long frozen_now = (long)json_get_num(root, "now", -1);
    if (frozen_now >= 0) {
        char nb[32];
        snprintf(nb, sizeof(nb), "%ld", frozen_now);
        setenv("HERMES_KANBAN_NOW", nb, 1);   /* freeze engine wall-clock */
    }
    json_t *ops = json_obj_get(root, "ops");
    int nops = ops ? json_len(ops) : 0;

    sqlite3 *conn = kdb_connect(board && board[0] ? board : NULL);
    if (!conn) { printf("{\"error\":\"connect_failed\"}\n"); json_free(root); return 1; }

    /* name registry for <NAME> substitution */
    char *named[64]; const char *names[64]; int nnamed = 0;
    /* stable id normalization: map each real task id -> "T<n>" so the oracle
     * output is deterministic despite random id generation. */
    char *real_ids[128]; char norm_ids[128][8]; int nids = 0;
    const char *norm_id(const char *rid) {
        if (!rid) return NULL;
        for (int k = 0; k < nids; k++) if (strcmp(real_ids[k], rid) == 0) return norm_ids[k];
        if (nids < 128) { real_ids[nids] = strdup(rid); snprintf(norm_ids[nids], 8, "T%d", nids); return norm_ids[nids++]; }
        return rid;
    }
    char *subst(const char *in) {
        if (!in) return NULL;
        size_t L = strlen(in);
        if (L >= 3 && in[0] == '<' && in[L-1] == '>') {
            char key[128];
            if (L - 2 < sizeof(key)) {
                memcpy(key, in + 1, L - 2);
                key[L - 2] = '\0';
                for (int k = 0; k < nnamed; k++)
                    if (strcmp(names[k], key) == 0) return named[k];
            }
        }
        return (char *)in;
    }

    printf("{\"results\":[");
    for (int i = 0; i < nops; i++) {
        json_t *op = json_get(ops, i);
        const char *name = json_get_str(op, "op", "");
        json_t *a = json_obj_get(op, "args");
        const char *nm = a ? json_get_str(a, "name", NULL) : NULL;

        if (strcmp(name, "create") == 0) {
            kdb_create_spec_t spec;
            memset(&spec, 0, sizeof(spec));
            spec.title = json_get_str(a, "title", NULL);
            spec.body = json_get_str(a, "body", NULL);
            spec.assignee = json_get_str(a, "assignee", NULL);
            spec.created_by = json_get_str(a, "created_by", NULL);
            spec.workspace_kind = json_get_str(a, "workspace_kind", NULL);
            spec.workspace_path = json_get_str(a, "workspace_path", NULL);
            spec.branch_name = json_get_str(a, "branch_name", NULL);
            spec.tenant = json_get_str(a, "tenant", NULL);
            spec.idempotency_key = json_get_str(a, "idempotency_key", NULL);
            spec.skills_json = json_get_str(a, "skills_json", NULL);
            spec.session_id = json_get_str(a, "session_id", NULL);
            spec.project_id = json_get_str(a, "project_id", NULL);
            spec.priority = (int)json_get_num(a, "priority", 0);
            spec.triage = json_get_bool(a, "triage", false);
            spec.goal_mode = json_get_bool(a, "goal_mode", false);
            char **parents = NULL;
            json_t *pa = json_obj_get(a, "parents");
            int np = pa ? (int)json_len(pa) : 0;
            if (np) { parents = calloc((size_t)np + 1, sizeof(char*));
                      for (int j=0;j<np;j++) {
                          json_t *el = json_get(pa, j);
                          const char *s = (el && el->type == JSON_STRING) ? el->str_val : NULL;
                          parents[j] = s ? subst(s) : NULL;
                      } }
            char *id = kdb_create_task(conn, &spec, parents);
            if (nm && id && nnamed < 64) { names[nnamed]=nm; named[nnamed]=id; nnamed++; }
            printf("%s{\"op\":\"create\",\"id\":", i ? "," : "");
            jprint_str(norm_id(id)); printf("}");
            if (!nm) free(id); /* keep named ids for substitution */
            if (parents) free(parents);
        }
        else if (strcmp(name, "list") == 0) {
            const char *st = json_get_str(a, "status", NULL);
            const char *as = json_get_str(a, "assignee", NULL);
            int inc = json_get_bool(a, "include_archived", false);
            int cnt = 0;
            kanban_task_t **list = kdb_list_tasks(conn, st, as, NULL, NULL, inc, 0, &cnt);
            printf("%s{\"op\":\"list\",\"count\":%d,\"ids\":[", i ? "," : "", cnt);
            for (int j=0;j<cnt;j++){ if(j)printf(","); jprint_str(norm_id(kdb_task_id(list[j]))); }
            printf("]}");
            kdb_task_list_free(list);
        }
        else if (strcmp(name, "claim") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            char *cid = kdb_claim_task(conn, tid, -1, NULL);
            printf("%s{\"op\":\"claim\",\"ok\":%s}", i?",":"", cid ? "true":"false");
            free(cid);
        }
        else if (strcmp(name, "complete") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *res = json_get_str(a, "result", NULL);
            const char *sum = json_get_str(a, "summary", NULL);
            int ok = kdb_complete_task(conn, tid, res, sum, NULL, NULL, -1);
            printf("%s{\"op\":\"complete\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "block") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *kind = json_get_str(a, "kind", NULL);
            int ok = kdb_block_task(conn, tid, NULL, kind, -1);
            printf("%s{\"op\":\"block\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "unblock") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            int ok = kdb_unblock_task(conn, tid);
            printf("%s{\"op\":\"unblock\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "recompute") == 0) {
            int promoted = kdb_recompute_ready(conn, -1);
            printf("%s{\"op\":\"recompute\",\"promoted\":%d}", i?",":"", promoted);
        }
        else if (strcmp(name, "reassign") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *p = json_get_str(a, "profile", NULL);
            int ok = kdb_reassign_task(conn, tid, p);
            printf("%s{\"op\":\"reassign\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "link") == 0) {
            const char *par = subst(json_get_str(a, "parent_id", NULL));
            const char *ch = subst(json_get_str(a, "child_id", NULL));
            int ok = kdb_link_tasks(conn, par, ch);
            printf("%s{\"op\":\"link\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "comment") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *au = json_get_str(a, "author", NULL);
            const char *b = json_get_str(a, "body", NULL);
            int ok = kdb_add_comment(conn, tid, au, b);
            printf("%s{\"op\":\"comment\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "notify_add") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *pf = json_get_str(a, "platform", NULL);
            const char *cid = json_get_str(a, "chat_id", NULL);
            int ok = kdb_add_notify_sub(conn, tid, pf, cid, NULL, NULL, NULL);
            printf("%s{\"op\":\"notify_add\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "stats") == 0) {
            char *s = kdb_board_stats(conn);
            printf("%s{\"op\":\"stats\",\"value\":", i?",":"");
            jprint_str(s); printf("}"); free(s);
        }
        else if (strcmp(name, "age") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            char *s = kdb_task_age(conn, tid);
            printf("%s{\"op\":\"age\",\"value\":", i?",":"");
            jprint_str(s); printf("}"); free(s);
        }
        else if (strcmp(name, "delete") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            int ok = kdb_delete_task(conn, tid);
            printf("%s{\"op\":\"delete\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else {
            printf("%s{\"op\":\"%s\",\"ok\":false}", i?",":"", name);
        }
    }
    printf("]}\n");
    kdb_close(conn);
    for (int k=0;k<nnamed;k++) free(named[k]);
    json_free(root);
    return 0;
}
