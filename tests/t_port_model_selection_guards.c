/*
 * t_port_model_selection_guards.c — parity harness for
 * src/hermes_cli/model_selection_guards.c. Ops:
 *   {"kind":"warnings","model":m,"provider":p,"base_url":b,"api_key":k,
 *    "model_info":{...}|null,"include_kinds":[...]|null}
 *   {"kind":"combined", ...same fields...}
 *   {"kind":"combined_message","warnings":[SelectionWarning,...]}
 */
#include "model_selection_guards.h"
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

/* optional-string: JSON string value or NULL when absent/non-string */
static const char *ostr(json_t *c, const char *key) {
    json_t *v = json_obj_get(c, key);
    return (v && v->type == JSON_STRING && v->str_val[0]) ? v->str_val : NULL;
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
            if (strcmp(kind, "warnings") == 0 || strcmp(kind, "combined") == 0) {
                json_t *model_info = json_obj_get(c, "model_info");
                if (model_info && model_info->type != JSON_OBJECT)
                    model_info = NULL;
                json_t *include_kinds = json_obj_get(c, "include_kinds");
                if (include_kinds && include_kinds->type != JSON_ARRAY)
                    include_kinds = NULL;
                if (strcmp(kind, "warnings") == 0) {
                    json_t *list = selection_warnings(
                        ostr(c, "model"), ostr(c, "provider"),
                        ostr(c, "base_url"), ostr(c, "api_key"),
                        model_info, include_kinds, NULL);
                    json_object_set(o, "out", list ? list : json_new_null());
                } else {
                    json_t *w = combined_selection_warning(
                        ostr(c, "model"), ostr(c, "provider"),
                        ostr(c, "base_url"), ostr(c, "api_key"),
                        model_info, NULL);
                    json_object_set(o, "out", w ? w : json_new_null());
                }
            } else if (strcmp(kind, "combined_message") == 0) {
                json_t *warnings = json_obj_get(c, "warnings");
                char *msg = combined_message(warnings);
                json_object_set(o, "out",
                                json_string(msg ? msg : ""));
                free(msg);
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
