/*
 * t_port_message_metadata.c — parity harness for src/agent/message_metadata.c
 * (agent/message_metadata.py).
 */
#include "hermes_json.h"
#include "message_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
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
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "is_persistence_only") == 0) {
                int v = is_persistence_only_message_field(json_get_str(c, "key", ""));
                json_object_set(o, "out", json_bool(v ? 1 : 0));
            } else if (strcmp(kind, "stamp") == 0) {
                json_t *msg = json_object_get(c, "message");
                json_t *copy = msg ? json_parse(json_dumps(msg, 0), NULL) : json_new_object();
                /* Python twin: absent timestamp -> "<wallclock>" sentinel;
                 * C wall clock maps to the same sentinel via ts==0. */
                double has_ts = json_object_get(c, "timestamp") ? json_get_num(c, "timestamp", 0.0) : 0.0;
                json_t *stamped = stamp_message_timestamp(copy, has_ts);
                json_t *tsv = json_object_get(stamped, "timestamp");
                if (has_ts == 0.0 && tsv && tsv->type == JSON_NUMBER)
                    json_object_set(stamped, "timestamp", json_string("<wallclock>"));
                json_object_set(o, "out", stamped ? stamped : json_new_null());
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line ? line : "{}");
            free(line);
        }
    }
    fflush(stdout);
    return 0;
}
