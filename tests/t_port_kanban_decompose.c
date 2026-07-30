/*
 * t_port_kanban_decompose.c — oracle harness for the pure triage/roster
 * helpers in hermes_cli/kanban_decompose.py (the LLM-free concern).
 *
 * Reads a fixture JSON of ops from argv[1], runs the ported C helpers, prints
 * one JSON object per op. The Python oracle (sta_oracle_kanban_decompose.py)
 * runs the same fixture against the LIVE Python module. Runner diffs them.
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Minimal JSON-string emitter (mirrors the engine harness style). */
static void jprint_str(const char *s)
{
    if (!s) s = "";
    printf("\"");
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
    }
    printf("\"");
}

static const char *js_str(json_t *a, const char *k)
{
    json_t *o = json_obj_get(a, k);
    return (o && o->type == JSON_STRING) ? o->str_val : NULL;
}

/* Mirror the Python oracle: ensure alice/bob profiles exist on disk so the
 * resolve/roster helpers behave identically on both sides. The C engine reads
 * profiles from <home>/profiles while the Python profiles module reads from
 * <home>/.hermes/profiles, so we write to BOTH roots. Drop HERMES_HOME first
 * so both sides fall back to SLERMES_HOME (the temp dir the runner provisions)
 * and we don't touch the developer's real home. */
static void setup_profiles(void)
{
    unsetenv("HERMES_HOME");
    char *home = kanban_home();
    if (!home) return;
    const char *names[] = {"alice", "bob"};
    char roots[2][4096];
    snprintf(roots[0], sizeof(roots[0]), "%s/profiles", home);
    snprintf(roots[1], sizeof(roots[1]), "%s/.hermes/profiles", home);
    for (int r = 0; r < 2; r++) {
        for (int i = 0; i < 2; i++) {
            char d[9000];
            snprintf(d, sizeof(d), "%s/%s", roots[r], names[i]);
            mkdir(roots[r], 0755);
            mkdir(d, 0755);
            char cf[9000];
            snprintf(cf, sizeof(cf), "%s/%s/config.yaml", roots[r], names[i]);
            FILE *f = fopen(cf, "r");
            if (!f) {
                f = fopen(cf, "w");
                if (f) { fprintf(f, "name: %s\n", names[i]); fclose(f); }
            } else fclose(f);
        }
    }
    free(home);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s fixture.json\n", argv[0]); return 2; }
    setup_profiles();
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1); fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);

    const char *err = NULL;
    json_t *root = json_parse(buf, &err);
    free(buf);
    if (!root) { fprintf(stderr, "json parse fail: %s\n", err ? err : "?"); return 2; }

    json_t *ops = json_obj_get(root, "ops");
    int n = (ops && ops->type == JSON_ARRAY) ? ops->c.count : 0;

    printf("[");
    for (int i = 0; i < n; i++) {
        json_t *a = ops->c.items[i];
        const char *name = js_str(a, "op");
        if (!name) { continue; }
        printf(i ? "," : "");
        if (strcmp(name, "extract") == 0) {
            const char *raw = js_str(a, "raw");
            char *b = kdb_extract_json_blob(raw ? raw : "");
            printf("{\"op\":\"extract\",\"value\":");
            jprint_str(b ? b : "null");
            printf("}");
            free(b);
        }
        else if (strcmp(name, "resolve_orch") == 0) {
            const char *cfg = js_str(a, "cfg");
            char *r = kdb_resolve_orchestrator_profile(cfg ? cfg : "{}");
            printf("{\"op\":\"resolve_orch\",\"value\":");
            jprint_str(r); printf("}");
            free(r);
        }
        else if (strcmp(name, "resolve_def") == 0) {
            const char *cfg = js_str(a, "cfg");
            char *r = kdb_resolve_default_assignee(cfg ? cfg : "{}");
            printf("{\"op\":\"resolve_def\",\"value\":");
            jprint_str(r); printf("}");
            free(r);
        }
        else if (strcmp(name, "normalize") == 0) {
            const char *assignee = js_str(a, "assignee");
            const char *def = js_str(a, "default_assignee");
            /* valid_names is a JSON array of strings */
            json_t *vn = json_obj_get(a, "valid_names");
            char **names = NULL; int nv = 0;
            if (vn && vn->type == JSON_ARRAY) {
                names = malloc(sizeof(char*) * (vn->c.count + 1));
                for (int k = 0; k < vn->c.count; k++) {
                    json_t *sv = vn->c.items[k];
                    if (sv && sv->type == JSON_STRING) names[nv++] = (char*)sv->str_val;
                }
                names[nv] = NULL;
            }
            char *r = kdb_normalize_assignee_choice(assignee, def ? def : "default", names);
            printf("{\"op\":\"normalize\",\"value\":");
            jprint_str(r); printf("}");
            free(r);
            free(names);
        }
        else if (strcmp(name, "build_roster") == 0) {
            char *r = kdb_build_roster();
            printf("{\"op\":\"build_roster\",\"value\":");
            jprint_str(r); printf("}");
            free(r);
        }
        else if (strcmp(name, "format_roster") == 0) {
            /* roster is a JSON array in the fixture; serialize it to text. */
            json_t *rnode = json_obj_get(a, "roster");
            char *rj = NULL;
            if (rnode) rj = json_dumps(rnode, 0);
            char *r = kdb_format_roster(rj ? rj : "[]");
            printf("{\"op\":\"format_roster\",\"value\":");
            jprint_str(r); printf("}");
            free(r);
            free(rj);
        }
        else {
            printf("{\"op\":\"%s\",\"ok\":false}", name);
        }
    }
    printf("]\n");
    json_free(root);
    return 0;
}
