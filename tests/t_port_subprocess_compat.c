/*
 * t_port_subprocess_compat.c — parity harness for
 * subprocess_compat_split_command_line in src/hermes_cli/subprocess_compat_pure.c
 * (hermes_cli/_subprocess_compat.py:split_command_line, POSIX branch).
 *
 * Reads a JSON fixture (argv[1]) with "ops": each op {"line": "..."} ->
 * call split, emit {"line":..,"out":[tokens] or null,"err":0/1}.
 * Mirrors sta_oracle_subprocess_compat.py.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *subprocess_compat_split_command_line(const char *line, int *err);

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = NULL;
    { FILE *f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr,"cannot read\n"); return 1; }
      fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
      raw=malloc(n+1); size_t r=fread(raw,1,n,f); raw[r]='\0'; fclose(f); }
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr,"bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *line = json_get_str(c, "line", "");
            int err = 0;
            char *res = subprocess_compat_split_command_line(line, &err);
            json_t *o = json_new_object();
            json_object_set(o, "line", json_string(line));
            json_object_set(o, "err", json_int(err));
            if (res) {
                json_t *arr = json_parse(res, NULL);
                json_object_set(o, "out", arr ? arr : json_new_null());
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *lineout = json_dumps(o, 0);
            printf("%s\n", lineout);
            free(lineout); json_free(o); free(res);
        }
    }
    json_free(root); free(raw);
    return 0;
}
