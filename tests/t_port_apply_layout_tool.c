/*
 * t_port_apply_layout_tool.c — parity harness for src/tools/apply_layout_tool.c
 * (tools/apply_layout_tool.py). Fixture ops:
 *   {"kind":"apply","preset":...,"emit":"ok"|"false"|"raise"|"none"}
 *   {"kind":"schema"}
 * Emits one JSON line per op mirroring tests/sta_oracle_apply_layout_tool.py.
 */
#include "hermes_json.h"
#include "apply_layout_tool.h"
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

/* scripted bridge behavior driven by the op's "emit" field */
typedef struct { const char *mode; char *last_event; char *last_payload; } fake_t;

static int fake_emit(const char *event, const char *payload_json, void *ctx) {
    fake_t *fk = (fake_t *)ctx;
    free(fk->last_event); free(fk->last_payload);
    fk->last_event = strdup(event);
    fk->last_payload = strdup(payload_json);
    if (strcmp(fk->mode, "raise") == 0) return -1;
    return strcmp(fk->mode, "false") == 0 ? 0 : 1;
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
            if (strcmp(kind, "apply") == 0) {
                static fake_t fk; memset(&fk, 0, sizeof fk);
                fk.mode = json_get_str(c, "emit", "ok");
                char *res = apply_layout_tool(json_get_str(c, "preset", ""),
                                              fake_emit, &fk);
                json_t *parsed = res ? json_parse(res, NULL) : NULL;
                json_object_set(o, "out", parsed ? parsed : json_new_null());
                /* expose the observed bridge call for the oracle to check */
                json_object_set(o, "event",
                    json_string(fk.last_event ? fk.last_event : ""));
                json_t *pl = fk.last_payload ? json_parse(fk.last_payload, NULL) : NULL;
                json_object_set(o, "payload_obj", pl ? pl : json_new_null());
                free(res);
            } else if (strcmp(kind, "schema") == 0) {
                char *sch = apply_layout_schema();
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
