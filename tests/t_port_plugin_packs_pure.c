/*
 * t_port_plugin_packs_pure.c — faithful parity harness for
 * src/hermes_cli/plugin_packs_pure.c (hermes_cli/plugin_packs.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_plugin_packs_pure.py).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "plugin_packs_pure.h"

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
            if (strcmp(op, "is_exact_sha") == 0) {
                int r = plugin_pack_is_exact_sha(js(c, "ref", ""));
                printf("{\"op\": \"is_exact_sha\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "entry_label") == 0) {
                json_t *item = json_object_get(c, "item");
                int idx = (int)json_get_num(c, "index", 0);
                char *r = plugin_pack_entry_label(item, idx);
                printf("{\"op\": \"entry_label\", \"out\": %s}\n", r ? r : "\"\"");
                free(r);
            } else if (strcmp(op, "validate_config_seed") == 0) {
                const char *pid = js(c, "plugin_id", "");
                json_t *seed = json_object_get(c, "seed");
                char *seedj = json_serialize(seed);
                char *r = plugin_pack_validate_config_seed(pid, seedj ? seedj : "{}");
                printf("{\"op\": \"validate_config_seed\", \"out\": %s}\n", r ? r : "{}");
                free(r); free(seedj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
