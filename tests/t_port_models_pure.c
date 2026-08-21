/*
 * t_port_models_pure.c — faithful parity harness for
 * src/hermes_cli/models_pure.c (hermes_cli/models.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_models_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "models_pure.h"

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
            if (strcmp(op, "xai_finalize") == 0) {
                json_t *ids = json_object_get(c, "ids");
                char *ij = json_serialize(ids);
                char *r = models_xai_finalize_catalog(ij ? ij : "[]");
                printf("{\"op\": \"xai_finalize\", \"out\": %s}\n", r ? r : "[]");
                free(r); free(ij);
            } else if (strcmp(op, "is_nous_free_tier") == 0) {
                json_t *a = json_object_get(c, "account");
                char *aj = json_serialize(a);
                int r = models_is_nous_free_tier(aj ? aj : "{}");
                printf("{\"op\": \"is_nous_free_tier\", \"out\": %s}\n", r ? "true" : "false");
                free(aj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
