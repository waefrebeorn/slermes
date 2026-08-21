/*
 * t_port_console_engine_pure.c — faithful parity harness for
 * src/hermes_cli/console_engine_pure.c (hermes_cli/console_engine.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_console_engine_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "console_engine_pure.h"

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
            if (strcmp(op, "is_footer_rule") == 0) {
                int r = console_engine_is_status_footer_rule(js(c, "line", ""));
                printf("{\"op\": \"is_footer_rule\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "strip_footer") == 0) {
                char *r = console_engine_strip_status_footer(js(c, "text", ""));
                printf("{\"op\": \"strip_footer\", \"out\": \"%s\"}\n", r ? r : "");
                free(r);
            } else if (strcmp(op, "table_summary") == 0) {
                int limit = (int)json_get_num(c, "limit", 76);
                char *r = console_engine_table_summary(js(c, "summary", ""), limit);
                printf("{\"op\": \"table_summary\", \"out\": \"%s\"}\n", r ? r : "");
                free(r);
            } else if (strcmp(op, "split_line") == 0) {
                char err[128];
                char *r = console_engine_split_line(js(c, "line", ""), err, sizeof(err));
                if (!r) {
                    printf("{\"op\": \"split_line\", \"out\": \"__ERR__: %s\"}\n", err);
                } else {
                    printf("{\"op\": \"split_line\", \"out\": %s}\n", r);
                    free(r);
                }
            } else if (strcmp(op, "contains_shell") == 0) {
                int r = console_engine_contains_shell_syntax(js(c, "line", ""), js(c, "tokens", ""));
                printf("{\"op\": \"contains_shell\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "format_sessions") == 0) {
                json_t *sess = json_object_get(c, "sessions");
                char *sj = json_serialize(sess);
                char *r = console_engine_format_sessions(sj ? sj : "[]");
                printf("{\"op\": \"format_sessions\", \"out\": \"%s\"}\n", r ? r : "");
                free(sj); free(r);
            } else if (strcmp(op, "format_job") == 0) {
                json_t *job = json_object_get(c, "job");
                char *jj = json_serialize(job);
                char *r = console_engine_format_job(jj ? jj : "{}", js(c, "action", ""));
                printf("{\"op\": \"format_job\", \"out\": \"%s\"}\n", r ? r : "");
                free(jj); free(r);
            } else if (strcmp(op, "clean_summary") == 0) {
                char *r = console_engine_clean_summary(js(c, "text", ""));
                printf("{\"op\": \"clean_summary\", \"out\": \"%s\"}\n", r ? r : "");
                free(r);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
