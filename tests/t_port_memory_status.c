/*
 * t_port_memory_status.c — parity harness for src/gateway/memory_status.c
 * (gateway/memory_status.py). Reads a JSON fixture (argv[1]) with "ops" and
 * emits one JSON line per op, mirroring tests/sta_oracle_memory_status.py.
 */
#include "memory_status.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    /* fixture dir = dirname(argv[1]); collect ops reference subdirs of it */
    char fixdir[4096];
    snprintf(fixdir, sizeof(fixdir), "%s", argv[1]);
    char *d = dirname(fixdir);

    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_obj_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_object();
            if (strcmp(kind, "classify") == 0) {
                const char *p = classify_pressure(
                    json_obj_get(c, "avail"), json_obj_get(c, "total"));
                json_object_set(o, "out", json_string(p));
            } else if (strcmp(kind, "collect") == 0) {
                const char *sub = json_get_str(c, "home", "");
                char home[4600];
                snprintf(home, sizeof(home), "%s/%s", d, sub);
                double now = json_get_num(c, "now", 0.0);
                json_t *status = collect_memory_status(home, now);
                json_object_set(o, "out", status);
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
