/*
 * t_port_kanban_util.c — oracle harness for the portable kanban helpers in
 * src/cli/kanban_util.c (ported from hermes_cli/kanban_db.py).
 *
 * Reads a fixture JSON, seeds a temp board DB with a few tasks via the engine,
 * then exercises the util helpers and prints a JSON summary per op. The Python
 * oracle (sta_oracle_kanban_util.py) runs the SAME fixture against the LIVE
 * Python module; the runner diffs the two byte-for-byte.
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static void jprint_str(const char *s)
{
    if (!s) { printf("null"); return; }
    printf("\"");
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
    }
    printf("\"");
}

/* Print a path (quoted), masking the (per-run temp) home prefix with
 * <HOME> so oracle output is deterministic regardless of where the
 * runner puts HOME. */
static void print_path_masked(const char *s)
{
    if (!s) { printf("null"); return; }
    char *home = kanban_home();
    size_t hl = home ? strlen(home) : 0;
    printf("\"");
    if (home && strncmp(s, home, hl) == 0 && (s[hl] == '/' || s[hl] == 0)) {
        printf("<HOME>%s", s + hl);
    } else {
        printf("%s", s);
    }
    printf("\"");
    free(home);
}

/* Map a fixture placeholder (T0/T1/T2) to the real task id returned by
 * create_task. */
static const char *real_id(const char *placeholder, char **ids, int n)
{
    if (!placeholder) return NULL;
    if (placeholder[0] == 'T' && placeholder[1] >= '0' && placeholder[1] <= '9') {
        int idx = placeholder[1] - '0';
        if (idx >= 0 && idx < n) return ids[idx];
    }
    return placeholder; /* pass through (e.g. T_NOPE) */
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s fixture.json\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);

    char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(buf);
    if (!root) { fprintf(stderr, "json parse fail: %s\n", err ? err : "?"); return 2; }

    sqlite3 *conn = kdb_connect("default");
    if (!conn) { fprintf(stderr, "connect fail\n"); return 2; }

    json_t *ops = json_obj_get(root, "ops");
    int nops = (ops && ops->type == JSON_ARRAY) ? ops->c.count : 0;

    char *ids[8]; char *phs[8]; int nid = 0;
    printf("[");
    for (int i = 0; i < nops; i++) {
        json_t *a = ops->c.items[i];
        const char *name = json_get_str(a, "op", NULL);
        if (!name) continue;
        printf(i ? "," : "");
        if (strcmp(name, "create") == 0) {
            kdb_create_spec_t spec;
            memset(&spec, 0, sizeof(spec));
            const char *id = json_get_str(a, "id", NULL);
            spec.title = json_get_str(a, "title", "t");
            spec.assignee = json_get_str(a, "assignee", "default");
            spec.created_by = json_get_str(a, "created_by", spec.assignee);
            char *rid = kdb_create_task(conn, &spec, NULL);
            if (rid && nid < 8) { ids[nid] = rid; phs[nid] = strdup(id ? id : ""); nid++; }
            /* emit the fixture placeholder (deterministic) instead of the random real id */
            printf("{\"op\":\"create\",\"placeholder\":");
            jprint_str(id);
            printf(",\"real\":");
            jprint_str(id);
            printf("}");
        }
        else if (strcmp(name, "link") == 0) {
            const char *p = real_id(json_get_str(a, "parent", ""), ids, nid);
            const char *c = real_id(json_get_str(a, "child", ""), ids, nid);
            int rc = kdb_link_tasks(conn, p, c);
            printf("{\"op\":\"link\",\"ok\":%s}", rc ? "true" : "false");
        }
        else if (strcmp(name, "find_missing_parents") == 0) {
            json_t *pa = json_obj_get(a, "parents");
            char **pv = NULL; int pn = 0;
            if (pa && pa->type == JSON_ARRAY) {
                pv = malloc(sizeof(char*) * (pa->c.count + 1));
                for (int k = 0; k < pa->c.count; k++) {
                    const char *ph = pa->c.items[k]->str_val;
                    pv[pn++] = (char*)real_id(ph, ids, nid);
                }
                pv[pn] = NULL;
            }
            char **miss = kdb_find_missing_parents(conn, pv, pn);
            printf("{\"op\":\"find_missing_parents\",\"missing\":[");
            if (miss) for (int k = 0; miss[k]; k++) { if (k) printf(","); jprint_str(miss[k]); }
            printf("]}");
            if (miss) { for (int k = 0; miss[k]; k++) free(miss[k]); free(miss); }
            free(pv);
        }
        else if (strcmp(name, "scan_prose") == 0) {
            const char *text = json_get_str(a, "text", "");
            /* normalize KANBAN_HOME placeholder to the real home */
            char *home = kanban_home();
            char full[8192];
            snprintf(full, sizeof(full), "%s", text);
            char *out = kdb_scan_prose_for_phantom_ids(conn, full);
            printf("{\"op\":\"scan_prose\",\"phantom\":");
            printf("%s", out ? out : "[]");
            printf("}");
            free(out); free(home);
        }
        else if (strcmp(name, "verify_created_cards") == 0) {
            const char *comp = real_id(json_get_str(a, "completing", ""), ids, nid);
            json_t *ca = json_obj_get(a, "claimed");
            char **cv = NULL; int cn = 0;
            if (ca && ca->type == JSON_ARRAY) {
                cv = malloc(sizeof(char*) * (ca->c.count + 1));
                for (int k = 0; k < ca->c.count; k++)
                    cv[cn++] = (char*)real_id(ca->c.items[k]->str_val, ids, nid);
                cv[cn] = NULL;
            }
            char *ver = NULL, *pha = NULL;
            kdb_verify_created_cards(conn, comp, cv, cn, &ver, &pha);
            /* map resolved real ids back to fixture placeholders for deterministic output */
            char *vm = malloc(strlen(ver) + 64), *pm = malloc(strlen(pha) + 64);
            vm[0] = 0; pm[0] = 0;
            for (int k = 0; ver[k]; ) {
                /* copy until a quoted id */
                if (ver[k] == '"') {
                    char tok[64]; int t = 0; k++;
                    while (ver[k] && ver[k] != '"' && t + 1 < (int)sizeof(tok)) tok[t++] = ver[k++];
                    if (ver[k] == '"') k++;
                    tok[t] = 0;
                    const char *ph = tok;
                    for (int j = 0; j < nid; j++) if (strcmp(tok, ids[j]) == 0) { ph = phs[j]; break; }
                    strcat(vm, "\""); strcat(vm, ph); strcat(vm, "\"");
                } else { int n = (int)strcspn(ver + k, "\""); strncat(vm, ver + k, (size_t)n); k += n; }
            }
            for (int k = 0; pha[k]; ) {
                if (pha[k] == '"') {
                    char tok[64]; int t = 0; k++;
                    while (pha[k] && pha[k] != '"' && t + 1 < (int)sizeof(tok)) tok[t++] = pha[k++];
                    if (pha[k] == '"') k++;
                    tok[t] = 0;
                    const char *ph = tok;
                    for (int j = 0; j < nid; j++) if (strcmp(tok, ids[j]) == 0) { ph = phs[j]; break; }
                    strcat(pm, "\""); strcat(pm, ph); strcat(pm, "\"");
                } else { int n = (int)strcspn(pha + k, "\""); strncat(pm, pha + k, (size_t)n); k += n; }
            }
            printf("{\"op\":\"verify_created_cards\",\"verified\":%s,\"phantom\":%s}",
                   vm, pm);
            free(ver); free(pha); free(cv); free(vm); free(pm);
        }
        else if (strcmp(name, "is_managed_scratch") == 0) {
            char *home = kanban_home();
            const char *ph = json_get_str(a, "path", "");
            char full[8192];
            if (strncmp(ph, "KANBAN_HOME/", 12) == 0)
                snprintf(full, sizeof(full), "%s/%s", home, ph + 12);
            else snprintf(full, sizeof(full), "%s", ph);
            int r = kdb_is_managed_scratch_path(full);
            printf("{\"op\":\"is_managed_scratch\",\"path\":");
            print_path_masked(full);
            printf(",\"managed\":%s}", r ? "true" : "false");
            free(home);
        }
        else if (strcmp(name, "is_managed_scratch_bad") == 0) {
            char *home = kanban_home();
            const char *ph = json_get_str(a, "path", "");
            char full[8192];
            if (strncmp(ph, "KANBAN_HOME/", 12) == 0)
                snprintf(full, sizeof(full), "%s/%s", home, ph + 12);
            else snprintf(full, sizeof(full), "%s", ph);
            int r = kdb_is_managed_scratch_path(full);
            printf("{\"op\":\"is_managed_scratch_bad\",\"managed\":%s}", r ? "true" : "false");
            free(home);
        }
        else if (strcmp(name, "has_spawnable_ready") == 0) {
            int r = kdb_has_spawnable_ready(conn);
            printf("{\"op\":\"has_spawnable_ready\",\"v\":%s}", r ? "true" : "false");
        }
        else if (strcmp(name, "has_spawnable_review") == 0) {
            int r = kdb_has_spawnable_review(conn);
            printf("{\"op\":\"has_spawnable_review\",\"v\":%s}", r ? "true" : "false");
        }
        else if (strcmp(name, "is_busy_error") == 0) {
            const char *msg = json_get_str(a, "msg", "");
            printf("{\"op\":\"is_busy_error\",\"v\":%s}", kdb_is_busy_error(msg) ? "true" : "false");
        }
        else if (strcmp(name, "absolute_hermes_path") == 0) {
            const char *ph = json_get_str(a, "path", "");
            char *out = kdb_absolute_hermes_path(ph);
            printf("{\"op\":\"absolute_hermes_path\",\"path\":");
            print_path_masked(out ? out : "");
            printf("}");
            free(out);
        }
        else if (strcmp(name, "path_search_names") == 0) {
            const char *cmd = json_get_str(a, "cmd", "");
            char **nv = kdb_path_search_names(cmd);
            printf("{\"op\":\"path_search_names\",\"names\":[");
            if (nv) for (int k = 0; nv[k]; k++) { if (k) printf(","); jprint_str(nv[k]); }
            printf("]}");
            if (nv) { for (int k = 0; nv[k]; k++) free(nv[k]); free(nv); }
        }
        else if (strcmp(name, "safe_which_no_cwd") == 0) {
            const char *cmd = json_get_str(a, "cmd", "");
            char *out = kdb_safe_which_no_cwd(cmd);
            printf("{\"op\":\"safe_which_no_cwd\",\"path\":");
            jprint_str(out ? out : "");
            printf("}");
            free(out);
        }
        else if (strcmp(name, "file_length_invariant") == 0) {
            int r = kdb_check_file_length_invariant(conn);
            printf("{\"op\":\"file_length_invariant\",\"corrupt\":%s}", r ? "true" : "false");
        }
        else if (strcmp(name, "scratch_tip_before") == 0) {
            /* unlink any pre-existing sentinel so the lifecycle is deterministic
             * regardless of which side ran first in the shared temp home */
            char *sp = kdb_scratch_tip_sentinel_path();
            if (sp) { unlink(sp); free(sp); }
            int r = kdb_scratch_tip_shown();
            printf("{\"op\":\"scratch_tip_before\",\"shown\":%s}", r ? "true" : "false");
        }
        else if (strcmp(name, "scratch_tip_mark") == 0) {
            kdb_mark_scratch_tip_shown();
            printf("{\"op\":\"scratch_tip_mark\",\"ok\":true}");
        }
        else if (strcmp(name, "scratch_tip_after") == 0) {
            int r = kdb_scratch_tip_shown();
            printf("{\"op\":\"scratch_tip_after\",\"shown\":%s}", r ? "true" : "false");
        }
        else if (strcmp(name, "maybe_emit_scratch_tip") == 0) {
            const char *tid = real_id(json_get_str(a, "task", ""), ids, nid);
            const char *kind = json_get_str(a, "kind", "scratch");
            kdb_maybe_emit_scratch_tip(conn, tid, kind);
            /* verify the tip event was appended */
            int found = 0;
            char q[256];
            snprintf(q, sizeof(q),
                "SELECT 1 FROM task_events WHERE task_id=? AND kind='tip_scratch_workspace' LIMIT 1");
            sqlite3_stmt *st = NULL;
            if (sqlite3_prepare_v2(conn, q, -1, &st, NULL) == SQLITE_OK) {
                sqlite3_bind_text(st, 1, tid ? tid : "", -1, SQLITE_TRANSIENT);
                if (sqlite3_step(st) == SQLITE_ROW) found = 1;
                sqlite3_finalize(st);
            }
            printf("{\"op\":\"maybe_emit_scratch_tip\",\"event_appended\":%s}", found ? "true" : "false");
        }
        else {
            printf("{\"op\":%s,\"ok\":false}", name);
        }
    }
    printf("]\n");

    /* free created ids + placeholders */
    for (int k = 0; k < nid; k++) { free(ids[k]); free(phs[k]); }
    sqlite3_close(conn);
    json_free(root);
    return 0;
}
