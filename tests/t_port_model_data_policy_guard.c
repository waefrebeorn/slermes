/*
 * t_port_model_data_policy_guard.c — parity harness for
 * src/hermes_cli/model_data_policy_guard.c. Ops:
 *   {"kind":"warn","model":m,"provider":p|null}
 */
#include "model_data_policy_guard.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_obj_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_object();
            if (strcmp(kind, "warn") == 0) {
                json_t *prov = json_obj_get(c, "provider");
                const char *provider =
                    (prov && prov->type == JSON_STRING) ? prov->str_val : NULL;
                DataTrainingWarning *w = data_training_warning(
                    json_get_str(c, "model", ""), provider, NULL);
                if (w) {
                    json_t *wr = json_object();
                    json_set(wr, "model", json_string(w->model));
                    json_set(wr, "provider", json_string(w->provider));
                    json_set(wr, "message", json_string(w->message));
                    json_object_set(o, "out", wr);
                    data_training_warning_free(w);
                } else {
                    json_object_set(o, "out", json_new_null());
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
