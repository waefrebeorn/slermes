/*
 * t_port_session_lost_and_found_pure.c — faithful parity harness for
 * src/hermes_cli/session_lost_and_found_pure.c
 * (hermes_cli/session_lost_and_found.py pure classification heuristics).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_session_lost_and_found_pure.py).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "session_lost_and_found_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *js(json_t *c, const char *k, const char *d) {
    return json_get_str(c, k, d);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_all(argv[1]);
    if (!raw) { fprintf(stderr, "cannot read fixture\n"); return 1; }
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture json\n"); free(raw); return 1; }

    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_object_get_at(ops, i);
            const char *op = js(c, "op", "");
            if (strcmp(op, "is_session_id") == 0) {
                int r = laf_is_session_id(js(c, "value", ""));
                printf("{\"op\": \"is_session_id\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "looks_like_source") == 0) {
                int r = laf_looks_like_source(js(c, "value", ""));
                printf("{\"op\": \"looks_like_source\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "classify_row") == 0) {
                int nf = (int)json_get_num(c, "nfield", 0);
                json_t *cells = json_object_get(c, "cells");
                char *cj = json_serialize(cells);
                char *r = laf_classify_row(nf, cj ? cj : "[]");
                printf("{\"op\": \"classify_row\", \"out\": %s}\n", r ? r : "null");
                free(r); free(cj);
            } else if (strcmp(op, "heuristic_started_at") == 0) {
                json_t *cells = json_object_get(c, "cells");
                char *cj = json_serialize(cells);
                double r = laf_heuristic_started_at(cj ? cj : "[]");
                printf("{\"op\": \"heuristic_started_at\", \"out\": %g}\n", r);
                free(cj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
