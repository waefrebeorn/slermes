/*
 * t_port_foreign_sessions_pure.c — faithful parity harness for
 * src/hermes_cli/foreign_sessions_pure.c (hermes_cli/foreign_sessions.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_foreign_sessions_pure.py).
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "foreign_sessions_pure.h"

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
            if (strcmp(op, "is_wrapper_text") == 0) {
                int r = foreign_sessions_is_wrapper_text(js(c, "text", ""));
                printf("{\"op\": \"is_wrapper_text\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "flatten_blocks") == 0) {
                json_t *content = json_object_get(c, "content");
                char *cj = json_serialize(content);
                char *r = foreign_sessions_flatten_blocks(cj ? cj : "null", js(c, "source", "claude"));
                printf("{\"op\": \"flatten_blocks\", \"out\": %s}\n", r ? r : "\"\"");
                free(r); free(cj);
            } else if (strcmp(op, "merge_turns") == 0) {
                json_t *turns = json_object_get(c, "turns");
                char *tj = json_serialize(turns);
                char *r = foreign_sessions_merge_turns(tj ? tj : "[]");
                printf("{\"op\": \"merge_turns\", \"out\": %s}\n", r ? r : "[]");
                free(r); free(tj);
            } else if (strcmp(op, "session_label") == 0) {
                char *r = foreign_sessions_session_label(js(c, "source", ""), js(c, "title", ""), js(c, "stem", ""));
                printf("{\"op\": \"session_label\", \"out\": %s}\n", r ? r : "\"\"");
                free(r);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
