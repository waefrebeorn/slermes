/*
 * Oracle harness for hermes_cli/approvals_suggest.py parse_apply_indices.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "libjson/json.h"
#include "approval.h"

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
        json_t *vj = json_obj_get(c, "value");
        const char *spec = vj && vj->type == JSON_STRING ? vj->str_val : "";
        int total = (int)json_get_num(c, "total", 10);
        int indices[64];
        int r = approval_parse_apply_indices(spec, total, indices, 64);
        if (r < 0) {
            printf("null\n");
        } else {
            /* Print as Python list: [0, 2] */
            printf("[");
            for (int j = 0; j < r; j++) {
                if (j > 0) printf(", ");
                printf("%d", indices[j]);
            }
            printf("]\n");
        }
    }
    json_free(root);
    return 0;
}
