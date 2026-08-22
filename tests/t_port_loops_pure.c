/*
 * t_port_loops_pure.c — parity harness for src/hermes_cli/loops_pure.c
 * (hermes_cli/loops.py deterministic helpers).
 *
 * Reads a JSON fixture (argv[1]) with "ops": list of {kind, ...} and emits
 * one JSON line per op, mirroring tests/sta_oracle_loops_pure.py exactly so
 * the two outputs can be diffed.
 */
#include "hermes_json.h"
#include "loops_pure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helper: parse fixture file into a malloc'd string */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *op_str(json_t *c, const char *k) {
    return json_get_str(c, k, "");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = op_str(c, "kind");
            json_t *o = json_new_object();
            if (strcmp(kind, "parse_interval") == 0) {
                int v = loops_parse_interval_token(op_str(c, "token"));
                if (v < 0) json_object_set(o, "out", json_new_null());
                else json_object_set(o, "out", json_int(v));
            } else if (strcmp(kind, "format_interval") == 0) {
                double secs = json_get_num(c, "seconds", 0.0);
                char *r = loops_format_interval(secs);
                json_object_set(o, "out", json_string(r ? r : ""));
                free(r);
            } else if (strcmp(kind, "parse_loop_args") == 0) {
                char *r = loops_parse_loop_args(op_str(c, "text"));
                json_t *parsed = r ? json_parse(r, NULL) : NULL;
                json_object_set(o, "out", parsed ? parsed : json_new_null());
                free(r);
            } else if (strcmp(kind, "response_complete") == 0) {
                int v = loops_response_signals_complete(op_str(c, "text"));
                json_object_set(o, "out", json_bool(v ? 1 : 0));
            } else if (strcmp(kind, "digest") == 0) {
                char *r = loops_digest_response(op_str(c, "text"));
                json_object_set(o, "out", json_string(r ? r : ""));
                free(r);
            } else if (strcmp(kind, "state_label") == 0) {
                const char *raw2 = op_str(c, "raw");
                loop_state_t *s = loops_state_from_json(raw2);
                double now = json_get_num(c, "now", 0.0);
                const char *which = op_str(c, "which");
                char *r = NULL;
                if (strcmp(which, "cadence") == 0) r = loops_state_cadence_label(s);
                else if (strcmp(which, "remaining") == 0) r = loops_state_remaining_label(s, now);
                else if (strcmp(which, "status") == 0) r = loops_state_status_line(s, now);
                json_object_set(o, "out", json_string(r ? r : ""));
                free(r);
                loops_state_free(s);
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
