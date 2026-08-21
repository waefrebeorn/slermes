/*
 * t_port_plugin_index_pure.c — faithful parity harness for
 * src/hermes_cli/plugin_index_pure.c (hermes_cli/plugin_index.py pure logic).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_plugin_index_pure.py).
 * For resolve_name/search_index, the fixture carries raw entries; the harness
 * first normalizes them via pindex_parse_entries (mirroring the oracle's
 * _parse_entries call) and feeds the normalized JSON to resolve/search.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "plugin_index_pure.h"

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
            if (strcmp(op, "install_identifier") == 0) {
                char *r = pindex_install_identifier(js(c, "repo", ""), js(c, "subdir", ""));
                printf("{\"op\": \"install_identifier\", \"out\": %s}\n", r ? r : "\"\"");
                free(r);
            } else if (strcmp(op, "parse_entries") == 0) {
                json_t *rawj = json_object_get(c, "raw");
                char *rj = json_serialize(rawj);
                char *r = pindex_parse_entries(rj ? rj : "null");
                printf("{\"op\": \"parse_entries\", \"out\": %s}\n", r ? r : "[]");
                free(r); free(rj);
            } else if (strcmp(op, "resolve_name") == 0) {
                json_t *entries = json_object_get(c, "entries");
                char *ej = json_serialize(entries);
                char *norm = pindex_parse_entries(ej ? ej : "[]");
                char *r = pindex_resolve_name(norm ? norm : "[]", js(c, "name", ""));
                printf("{\"op\": \"resolve_name\", \"out\": %s}\n", r ? r : "{\"entry\":null,\"candidates\":[]}");
                free(r); free(norm); free(ej);
            } else if (strcmp(op, "search_index") == 0) {
                json_t *entries = json_object_get(c, "entries");
                char *ej = json_serialize(entries);
                char *norm = pindex_parse_entries(ej ? ej : "[]");
                char *r = pindex_search_index(norm ? norm : "[]", js(c, "term", ""), js(c, "capability", ""));
                printf("{\"op\": \"search_index\", \"out\": %s}\n", r ? r : "[]");
                free(r); free(norm); free(ej);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
