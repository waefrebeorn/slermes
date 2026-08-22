/*
 * t_port_command_token_source.c — parity harness for
 * src/agent/command_token_source.c (agent/command_token_source.py).
 *
 * Fixture ops (JSON list "ops", argv[1]):
 *   {"kind":"mint","command":"printf abc","label":"prov"}
 *   {"kind":"build_provider","key_cmd":" echo hi ","label":"p"}
 * Emits one JSON envelope per op; mirrors tests/sta_oracle_command_token_source.py.
 */
#include "hermes_json.h"
#include "command_token_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
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
            if (strcmp(kind, "mint") == 0) {
                const char *cmd = json_get_str(c, "command", "");
                const char *label = json_get_str(c, "label", "");
                char err[1024];
                double ttl = -1.0;
                char *tok = command_token_source__mint(cmd, label, err,
                                                       sizeof err, &ttl);
                if (tok)
                    json_object_set(o, "out", json_string(tok));
                else
                    json_object_set(o, "out", json_new_null());
                if (err[0])
                    json_object_set(o, "error", json_string(err));
                else
                    json_object_set(o, "error", json_new_null());
                if (ttl > 0)
                    json_object_set(o, "ttl", json_int((long long)(ttl > 1e9 ? -1 : ttl)));
                else
                    json_object_set(o, "ttl", json_new_null());
                free(tok);
            } else if (strcmp(kind, "build_provider") == 0) {
                const char *kc = json_get_str(c, "key_cmd", "");
                const char *label = json_get_str(c, "label", "");
                command_token_source_t *p =
                    build_command_token_provider(kc, label);
                if (!p) {
                    json_object_set(o, "out", json_new_null());
                } else {
                    json_object_set(o, "out",
                                    json_string(p->command ? p->command : ""));
                    command_token_source_free(p);
                }
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
