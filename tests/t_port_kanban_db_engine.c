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

/* Remove every environment-specific "db_path":"..." and "new_path":"..."
 * field from a board metadata / list JSON so the oracle comparison is
 * home-dir independent. Returns a newly malloc'd string (caller frees);
 * frees the input. */
static char *strip_db_path(char *m)
{
    if (!m) return NULL;
    char *out = malloc(strlen(m) + 1);
    size_t o = 0;
    const char *p = m;
    const char *needle = ",\"db_path\":\"";
    const char *needle2 = ",\"new_path\":\"";
    while (*p) {
        const char *hit = strstr(p, needle);
        if (!hit) hit = strstr(p, needle2);
        if (!hit) { strcpy(out + o, p); break; }
        size_t pre = (size_t)(hit - p);
        memcpy(out + o, p, pre);
        o += pre;
        const char *q = hit + (hit == strstr(p, needle2) ? strlen(needle2) : strlen(needle));
        while (*q && *q != '"') { if (*q == '\\') q++; q++; }
        if (*q == '"') q++;  /* skip closing quote */
        p = q;               /* continue after the removed field */
    }
    free(m);
    return out;
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
        else if (strcmp(name, "run_lifecycle") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            int rid = kdb_current_run_id(conn, tid);
            int ended = kdb_end_run(conn, tid, "completed", "run done", NULL, NULL, "completed");
            int syn = kdb_synthesize_ended_run(conn, tid, "reclaimed", "synthetic", NULL, NULL);
            char *ls = kdb_latest_summary(conn, tid);
            printf("%s{\"op\":\"run_lifecycle\",\"cur_run\":%d,\"ended\":%d,\"synth\":%d,\"latest_summary\":",
                   i?",":"", rid, ended, syn);
            jprint_str(ls); printf("}"); if (ls) free(ls);
        }
        else if (strcmp(name, "would_cycle") == 0) {
            const char *p = subst(json_get_str(a, "parent_id", NULL));
            const char *c = subst(json_get_str(a, "child_id", NULL));
            int cyc = kdb_would_cycle(conn, p, c);
            printf("%s{\"op\":\"would_cycle\",\"cycle\":%s}", i?",":"", cyc?"true":"false");
        }
        else if (strcmp(name, "parent_results") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            char **ps = NULL, **rs = NULL;
            int n = kdb_parent_results(conn, tid, &ps, &rs);
            printf("%s{\"op\":\"parent_results\",\"n\":%d,\"parents\":[", i?",":"", n);
            for (int j=0;j<n;j++){
                if(j)printf(",");
                const char *nid = norm_id(ps[j]);
                jprint_str(nid ? nid : ps[j]);
            }
            printf("]}");
            kdb_parent_results_free(ps, rs);
        }
        else if (strcmp(name, "unseen_claim") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *pf = json_get_str(a, "platform", NULL);
            const char *cid = json_get_str(a, "chat_id", NULL);
            int old=0, neu=0, n=0; kanban_event_t **ev=NULL;
            n = kdb_claim_unseen_events_for_sub(conn, tid, pf, cid, NULL, NULL, 0,
                                                &old, &neu, &ev, &n);
            printf("%s{\"op\":\"unseen_claim\",\"old\":%d,\"new\":%d,\"n\":%d}", i?",":"", old, neu, n);
            kdb_event_list_free(ev);
        }
        else if (strcmp(name, "gc") == 0) {
            int n = kdb_gc_events(conn, 0);  /* cutoff=now -> only terminal, aged */
            printf("%s{\"op\":\"gc\",\"deleted\":%d}", i?",":"", n);
        }
        else if (strcmp(name, "assignees") == 0) {
            char *s = kdb_known_assignees(conn);
            printf("%s{\"op\":\"assignees\",\"value\":", i?",":"");
            jprint_str(s); printf("}"); if (s) free(s);
        }
        else if (strcmp(name, "latest_sum") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            char *s = kdb_latest_summary(conn, tid);
            printf("%s{\"op\":\"latest_sum\",\"value\":", i?",":"");
            jprint_str(s); printf("}"); if (s) free(s);
        }
        else if (strcmp(name, "delete") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            int ok = kdb_delete_task(conn, tid);
            printf("%s{\"op\":\"delete\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "heartbeat") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            const char *note = json_get_str(a, "note", NULL);
            long ern = (long)json_get_num(a, "expected_run_id", 0);
            int ok = kdb_heartbeat_worker(conn, tid, note, ern);
            printf("%s{\"op\":\"heartbeat\",\"ok\":%s}", i?",":"", ok?"true":"false");
        }
        else if (strcmp(name, "respawn_guard") == 0) {
            const char *tid = subst(json_get_str(a, "task_id", NULL));
            char *r = kdb_check_respawn_guard(conn, tid);
            printf("%s{\"op\":\"respawn_guard\",\"value\":", i?",":"");
            jprint_str(r); printf("}"); if (r) free(r);
        }

        else if (strcmp(name, "create_board") == 0) {
            const char *slug = json_get_str(a, "slug", NULL);
            const char *nm = json_get_str(a, "name", NULL);
            const char *ds = json_get_str(a, "description", NULL);
            const char *ic = json_get_str(a, "icon", NULL);
            const char *cl = json_get_str(a, "color", NULL);
            const char *dw = json_get_str(a, "default_workdir", NULL);
            char *m = kdb_create_board(slug, nm, ds, ic, cl, dw);
            m = strip_db_path(m);
            printf("%s{\"op\":\"create_board\",\"value\":", i?",":"");
            jprint_str(m); printf("}"); if (m) free(m);
        }
        else if (strcmp(name, "write_board_metadata") == 0) {
            const char *b = json_get_str(a, "board", NULL);
            const char *nm = json_get_str(a, "name", NULL);
            const char *ds = json_get_str(a, "description", NULL);
            const char *ic = json_get_str(a, "icon", NULL);
            const char *cl = json_get_str(a, "color", NULL);
            int arch_set = (json_obj_get(a, "archived") != NULL);
            int arch = (int)json_get_num(a, "archived", 0);
            const char *dw = json_get_str(a, "default_workdir", NULL);
            char *m = kdb_write_board_metadata(b, nm, ds, ic, cl, arch_set, arch, dw);
            m = strip_db_path(m);
            printf("%s{\"op\":\"write_board_metadata\",\"value\":", i?",":"");
            jprint_str(m); printf("}"); if (m) free(m);
        }
        else if (strcmp(name, "read_board_metadata") == 0) {
            const char *b = json_get_str(a, "board", NULL);
            char *m = kdb_read_board_metadata(b);
            m = strip_db_path(m);
            printf("%s{\"op\":\"read_board_metadata\",\"value\":", i?",":"");
            jprint_str(m); printf("}"); if (m) free(m);
        }
        else if (strcmp(name, "list_boards") == 0) {
            int inc = (int)json_get_num(a, "include_archived", 1);
            char *m = kdb_list_boards(inc);
            m = strip_db_path(m);
            printf("%s{\"op\":\"list_boards\",\"value\":", i?",":"");
            jprint_str(m); printf("}"); if (m) free(m);
        }
        else if (strcmp(name, "remove_board") == 0) {
            const char *slug = json_get_str(a, "slug", NULL);
            int arch = (int)json_get_num(a, "archive", 1);
            char *m = kdb_remove_board(slug, arch);
            m = strip_db_path(m);
            printf("%s{\"op\":\"remove_board\",\"value\":", i?",":"");
            jprint_str(m); printf("}"); if (m) free(m);
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
