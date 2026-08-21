/*
 * t_port_kanban_db_pure.c — faithful parity harness for
 * src/hermes_cli/kanban_db_pure.c (hermes_cli/kanban_db.py pure helpers).
 *
 * Reads a JSON fixture from argv[1] describing ops; calls the C helpers and
 * emits one JSON line per op (same shape as sta_oracle_kanban_db_pure.py).
 * run_oracle.sh diffs them.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "kanban_db_pure.h"

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
            if (strcmp(op, "normalize_effort") == 0) {
                char *r = kanban_normalize_reasoning_effort(js(c, "effort", ""));
                if (!r) printf("{\"op\": \"normalize_effort\", \"out\": null}\n");
                else { printf("{\"op\": \"normalize_effort\", \"out\": \"%s\"}\n", r); free(r); }
            } else if (strcmp(op, "claim_ttl") == 0) {
                json_t *t = json_object_get(c, "ttl");
                int tv = (t && t->type == JSON_NUMBER) ? (int)t->num_val : 0;
                printf("{\"op\": \"claim_ttl\", \"out\": %d}\n", kanban_resolve_claim_ttl_seconds(tv));
            } else if (strcmp(op, "crash_grace") == 0) {
                json_t *e = json_object_get(c, "env");
                const char *ev = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                printf("{\"op\": \"crash_grace\", \"out\": %d}\n", kanban_resolve_crash_grace_seconds(ev));
            } else if (strcmp(op, "rate_cooldown") == 0) {
                json_t *e = json_object_get(c, "env");
                const char *ev = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                printf("{\"op\": \"rate_cooldown\", \"out\": %d}\n", kanban_resolve_rate_limit_cooldown_seconds(ev));
            } else if (strcmp(op, "relative_age") == 0) {
                long ts = (long)json_get_num(c, "ts", 0);
                json_t *nw = json_object_get(c, "now");
                long now = (nw && nw->type == JSON_NUMBER) ? (long)nw->num_val : -1;
                char *r = kanban_relative_age(ts, now);
                printf("{\"op\": \"relative_age\", \"out\": \"%s\"}\n", r ? r : ""); free(r);
            } else if (strcmp(op, "normalize_slug") == 0) {
                char *r = kanban_normalize_board_slug(js(c, "slug", ""));
                if (!r) printf("{\"op\": \"normalize_slug\", \"out\": null}\n");
                else { printf("{\"op\": \"normalize_slug\", \"out\": \"%s\"}\n", r); free(r); }
            } else if (strcmp(op, "display_name") == 0) {
                char *r = kanban_default_board_display_name(js(c, "slug", ""));
                printf("{\"op\": \"display_name\", \"out\": \"%s\"}\n", r ? r : ""); free(r);
            }
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
