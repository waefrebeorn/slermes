/*
 * t_port_terminal_breadcrumbs_pure.c — faithful parity harness for
 * src/hermes_cli/terminal_breadcrumbs_pure.c.
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "terminal_breadcrumbs_pure.h"

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
            if (strcmp(op, "sanitize") == 0) {
                char *r = tbr_sanitize(js(c, "raw", ""));
                printf("{\"op\": \"sanitize\", \"out\": %s}\n", r ? r : "\"\"");
                free(r);
            } else if (strcmp(op, "is_stale") == 0) {
                double ts = json_get_num(c, "ts", 0.0);
                double now = json_get_num(c, "now", 1000000000000.0);
                printf("{\"op\": \"is_stale\", \"out\": %s}\n", tbr_is_stale_payload(ts, now) ? "true" : "false");
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
