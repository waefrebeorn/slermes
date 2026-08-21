/*
 * t_port_plugin_capabilities_pure.c — faithful parity harness for
 * src/hermes_cli/plugin_capabilities_pure.c
 * (hermes_cli/plugin_capabilities.py pure capability/consent logic).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_plugin_capabilities_pure.py).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "plugin_capabilities_pure.h"

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
            if (strcmp(op, "is_valid_capability") == 0) {
                int r = pc_is_valid_capability(js(c, "id", ""));
                printf("{\"op\": \"is_valid_capability\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "parse_declared_capabilities") == 0) {
                json_t *rawj = json_object_get(c, "raw");
                char *rj = json_serialize(rawj);
                char *r = pc_parse_declared_capabilities(rj ? rj : "null", js(c, "name", "?"));
                printf("{\"op\": \"parse_declared_capabilities\", \"out\": %s}\n", r ? r : "[]");
                free(r); free(rj);
            } else if (strcmp(op, "capability_set_hash") == 0) {
                json_t *caps = json_object_get(c, "caps");
                char *cj = json_serialize(caps);
                char *r = pc_capability_set_hash(cj ? cj : "[]");
                printf("{\"op\": \"capability_set_hash\", \"out\": %s}\n", r ? r : "\"\"");
                free(r); free(cj);
            } else if (strcmp(op, "granted_capabilities") == 0) {
                json_t *cfg = json_object_get(c, "config");
                char *cj = json_serialize(cfg);
                char *r = pc_granted_capabilities(js(c, "plugin_id", ""), cj ? cj : "{}");
                printf("{\"op\": \"granted_capabilities\", \"out\": %s}\n", r ? r : "[]");
                free(r); free(cj);
            } else if (strcmp(op, "legacy_gate_set") == 0) {
                json_t *entry = json_object_get(c, "entry");
                char *ej = json_serialize(entry);
                int r = pc_legacy_gate_set(ej ? ej : "{}", js(c, "capability", ""));
                printf("{\"op\": \"legacy_gate_set\", \"out\": %s}\n", r ? "true" : "false");
                free(ej);
            } else if (strcmp(op, "plugin_capability_granted") == 0) {
                json_t *cfg = json_object_get(c, "config");
                char *cj = json_serialize(cfg);
                int r = pc_plugin_capability_granted(js(c, "plugin_id", ""), js(c, "capability", ""), cj ? cj : "{}");
                printf("{\"op\": \"plugin_capability_granted\", \"out\": %s}\n", r ? "true" : "false");
                free(cj);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
