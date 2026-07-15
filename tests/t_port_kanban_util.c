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

    char *ids[8]; int nid = 0;
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
            if (rid && nid < 8) ids[nid++] = rid;
            printf("{\"op\":\"create\",\"placeholder\":");
            jprint_str(id);
            printf(",\"real\":");
            jprint_str(rid);
            printf("}");
            free(rid);
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
            printf("{\"op\":\"verify_created_cards\",\"verified\":%s,\"phantom\":%s}",
                   ver ? ver : "[]", pha ? pha : "[]");
            free(ver); free(pha); free(cv);
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
            jprint_str(full);
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
        else {
            printf("{\"op\":%s,\"ok\":false}", name);
        }
    }
    printf("]\n");

    /* free created ids */
    for (int k = 0; k < nid; k++) free(ids[k]);
    sqlite3_close(conn);
    json_free(root);
    return 0;
}
