/*
 * Oracle harness for hermes_cli/approvals_suggest.py pure helpers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "approval.h"
#include "hermes_regex.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char *err = NULL;
    json_t *root = json_parse_file(argv[1], &err);
    if (err) { free(err); return 1; }
    if (!root || root->type != JSON_ARRAY) {
        if (root) json_free(root);
        return 1;
    }
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        json_t *opj = json_obj_get(c, "op");
        const char *op = opj && opj->type == JSON_STRING ? opj->str_val : "";

        if (strcmp(op, "is_unsafe_class") == 0) {
            json_t *vj = json_obj_get(c, "value");
            const char *desc = vj && vj->type == JSON_STRING ? vj->str_val : "";
            bool r = approval_is_unsafe_class(desc);
            printf("%s\n", r ? "true" : "false");
        } else if (strcmp(op, "derive_glob") == 0) {
            json_t *vj = json_obj_get(c, "value");
            const char *cmd = vj && vj->type == JSON_STRING ? vj->str_val : "";
            char *r = approval_derive_glob(cmd);
            printf("%s\n", r ? r : "null");
            free(r);
        }
    }
    json_free(root);
    return 0;
}
