/*
 * t_port_loops_pure.c — faithful parity harness for
 * src/hermes_cli/loops_pure.c (hermes_cli/loops.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_loops_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "loops_pure.h"

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
            if (strcmp(op, "parse_interval") == 0) {
                int r = loops_parse_interval_token(js(c, "token", ""));
                if (r < 0) printf("{\"op\": \"parse_interval\", \"out\": null}\n");
                else printf("{\"op\": \"parse_interval\", \"out\": %d}\n", r);
            } else if (strcmp(op, "format_interval") == 0) {
                double secs = json_get_num(c, "seconds", 0);
                char *r = loops_format_interval(secs);
                printf("{\"op\": \"format_interval\", \"out\": \"%s\"}\n", r ? r : "");
                free(r);
            } else if (strcmp(op, "response_complete") == 0) {
                int r = loops_response_signals_complete(js(c, "text", ""));
                printf("{\"op\": \"response_complete\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "parse_loop_args") == 0) {
                char *r = loops_parse_loop_args(js(c, "text", ""));
                printf("%s\n", r ? r : "{}");
                free(r);
            } else if (strcmp(op, "config_min") == 0) {
                json_t *cfg = json_object_get(c, "config");
                char *cj = json_serialize(cfg);
                int r = loops_min_interval_seconds(cj);
                printf("{\"op\": \"config_min\", \"out\": %d}\n", r);
                free(cj);
            } else if (strcmp(op, "config_max") == 0) {
                json_t *cfg = json_object_get(c, "config");
                char *cj = json_serialize(cfg);
                int r = loops_max_ticks_default(cj);
                printf("{\"op\": \"config_max\", \"out\": %d}\n", r);
                free(cj);
            } else if (strcmp(op, "config_floor") == 0) {
                json_t *cfg = json_object_get(c, "config");
                char *cj = json_serialize(cfg);
                int r = loops_self_paced_floor_seconds(cj);
                printf("{\"op\": \"config_floor\", \"out\": %d}\n", r);
                free(cj);
            } else if (strcmp(op, "config_ceiling") == 0) {
                json_t *cfg = json_object_get(c, "config");
                char *cj = json_serialize(cfg);
                int r = loops_self_paced_ceiling_seconds(cj);
                printf("{\"op\": \"config_ceiling\", \"out\": %d}\n", r);
                free(cj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
