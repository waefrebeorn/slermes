/*
 * t_port_memory_tool_pure.c — faithful parity harness for
 * src/tools/memory_tool_pure.c (tools/memory_tool.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_memory_tool_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "memory_tool_pure.h"

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
            if (strcmp(op, "drift_error") == 0) {
                char *r = memory_tool_drift_error(js(c, "path_name", ""), js(c, "bak_path", ""));
                printf("{\"op\": \"drift_error\", \"out\": %s}\n", r ? r : "{}");
                free(r);
            } else if (strcmp(op, "read_failed_error") == 0) {
                char *r = memory_tool_read_failed_error(js(c, "path_name", ""));
                printf("{\"op\": \"read_failed_error\", \"out\": %s}\n", r ? r : "{}");
                free(r);
            } else if (strcmp(op, "consolidation") == 0) {
                json_t *resp = json_object_get(c, "response");
                char *rj = json_serialize(resp);
                memory_tool_pure_store_t *s = NULL;
                int calls = (int)json_get_num(c, "calls", 1);
                char *last = NULL;
                for (int k = 0; k < calls; k++) {
                    free(last);
                    last = memory_tool_pure_store_consolidation_failure(s, rj ? rj : "{}");
                }
                printf("{\"op\": \"consolidation\", \"out\": %s}\n", last ? last : "{}");
                free(last); free(rj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
