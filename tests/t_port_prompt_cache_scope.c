/*
 * t_port_prompt_cache_scope.c — parity harness for
 * src/agent/prompt_cache_scope.c (agent/prompt_cache_scope.py).
 *
 * One agent instance is carried across the ops of a case so memoization
 * behavior is exercised exactly like Python's attribute-based memo:
 *   {"kind":"call","sid":"s1"|null,"db":true|false,
 *    "lineage":"ok|none|empty|badroot|nonlist|raise","lineage_root":"r",
 *    "persist_disabled":false}
 *   {"kind":"safe_call", ...same fields...}
 */
#include "prompt_cache_scope.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static char fake_last_root[256];

/* deterministic fake SessionDB: returns the lineage configured per call */
static json_t *fake_lineage(void *ctx, const char *session_id) {
    (void)session_id;
    const char *mode = (const char *)ctx;
    if (strcmp(mode, "raise") == 0)
        return NULL;   /* models the except path */
    if (strcmp(mode, "none") == 0)
        return NULL;
    json_t *arr = json_array();
    if (strcmp(mode, "empty") == 0)
        return arr;    /* [] */
    if (strcmp(mode, "nonlist") == 0) {
        /* non-list truthy result, e.g. a string */
        json_free(arr);
        return json_string("oops");
    }
    /* ok | badroot */
    const char *root = (strcmp(mode, "badroot") == 0) ? "" : fake_last_root;
    json_append(arr, json_string(root));
    return arr;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }

    static prompt_cache_scope_db db;
    static prompt_cache_scope_agent agent;
    memset(&db, 0, sizeof(db));
    memset(&agent, 0, sizeof(agent));
    db.get_compression_lineage = fake_lineage;

    json_t *ops = json_obj_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_object();
            if (strcmp(kind, "call") == 0 || strcmp(kind, "safe_call") == 0) {
                int is_safe = (strcmp(kind, "safe_call") == 0);
                /* reconfigure agent surface per op */
                static char sidbuf[512];
                json_t *sid = json_obj_get(c, "sid");
                if (sid && sid->type == JSON_STRING) {
                    snprintf(sidbuf, sizeof(sidbuf), "%s", sid->str_val);
                    agent.session_id = sidbuf;
                } else {
                    agent.session_id = NULL;
                }
                snprintf(fake_last_root, sizeof(fake_last_root), "%s",
                         json_get_str(c, "lineage_root", ""));
                static char modebuf[32];
                snprintf(modebuf, sizeof(modebuf), "%s",
                         json_get_str(c, "lineage", "ok"));
                db.ctx = modebuf;
                json_t *dbf = json_obj_get(c, "db");
                int db_on = dbf
                    && ((dbf->type == JSON_NUMBER && dbf->num_val != 0.0)
                        || (dbf->type == JSON_BOOL && dbf->bool_val));
                agent._session_db = db_on ? &db : NULL;
                json_t *pdf = json_obj_get(c, "persist_disabled");
                agent._persist_disabled = pdf
                    && ((pdf->type == JSON_NUMBER && pdf->num_val != 0.0)
                        || (pdf->type == JSON_BOOL && pdf->bool_val));

                char *scope = is_safe ? resolve_prompt_cache_scope_safe(&agent)
                                      : resolve_prompt_cache_scope(&agent);
                if (scope) {
                    json_object_set(o, "out", json_string(scope));
                    free(scope);
                } else {
                    json_object_set(o, "out", json_new_null());
                }
                /* memo slot dump: null or [sid, db_present, scope] */
                if (agent.has_memo && agent.memo_sid && agent.memo_scope) {
                    json_t *m = json_array();
                    json_append(m, json_string(agent.memo_sid));
                    json_append(m, json_bool(agent.memo_db_present ? 1 : 0));
                    json_append(m, json_string(agent.memo_scope));
                    json_object_set(o, "memo", m);
                } else {
                    json_object_set(o, "memo", json_new_null());
                }
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    prompt_cache_scope_agent_memo_free(&agent);
    json_free(root);
    free(raw);
    return 0;
}
