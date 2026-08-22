/*
 * t_port_setup_mcp_tool.c — parity harness for src/tools/setup_mcp_tool.c
 * (tools/setup_mcp_tool.py). Fixture ops:
 *   {"kind":"setup","server":...,"action":...,"reason":...,
 *    "cb":"ok_json"|"ok_text"|"none"|"raise"|"nocb"}
 *   {"kind":"schema"}
 * Mirrors tests/sta_oracle_setup_mcp_tool.py.
 */
#include "hermes_json.h"
#include "setup_mcp_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

typedef struct { const char *mode; char *last_server, *last_action, *last_reason; } fake_t;

static int fake_cb(const char *server, const char *action, const char *reason,
                   char **out_raw, void *ctx) {
    fake_t *fk = (fake_t *)ctx;
    free(fk->last_server); free(fk->last_action); free(fk->last_reason);
    fk->last_server = strdup(server);
    fk->last_action = strdup(action);
    fk->last_reason = strdup(reason);
    if (strcmp(fk->mode, "raise") == 0) return -1;
    if (strcmp(fk->mode, "none") == 0) { *out_raw = NULL; return 0; }
    if (strcmp(fk->mode, "ok_text") == 0) { *out_raw = strdup("not json at all"); return 0; }
    /* ok_json: declined card answer echoing the server name */
    size_t need = strlen(server) + 64;
    *out_raw = malloc(need);
    snprintf(*out_raw, need, "{\"status\":\"declined\",\"server\":\"%s\"}", server);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *rootj = json_parse(raw, NULL);
    if (!rootj) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(rootj, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "setup") == 0) {
                static fake_t fk; memset(&fk, 0, sizeof fk);
                fk.mode = json_get_str(c, "cb", "ok_json");
                free(fk.last_server); free(fk.last_action); free(fk.last_reason);
                fk.last_server = fk.last_action = fk.last_reason = NULL;
                char *res = setup_mcp_tool(
                    json_get_str(c, "server", ""),
                    json_get_str(c, "action", ""),
                    json_get_str(c, "reason", ""),
                    strcmp(fk.mode, "nocb") == 0 ? NULL : fake_cb, &fk);
                json_t *parsed = res ? json_parse(res, NULL) : NULL;
                json_object_set(o, "out", parsed ? parsed : json_new_null());
                json_object_set(o, "cb_server",
                    json_string(fk.last_server ? fk.last_server : ""));
                json_object_set(o, "cb_action",
                    json_string(fk.last_action ? fk.last_action : ""));
                json_object_set(o, "cb_reason",
                    json_string(fk.last_reason ? fk.last_reason : ""));
                free(res);
            } else if (strcmp(kind, "schema") == 0) {
                char *sch = setup_mcp_schema();
                json_t *parsed = sch ? json_parse(sch, NULL) : NULL;
                json_object_set(o, "out", parsed ? parsed : json_new_null());
                free(sch);
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(rootj);
    free(raw);
    return 0;
}
