/*
 * Oracle harness for agent/billing_view.py pure helpers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "billing_pure.h"

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
        const char *name = opj && opj->type == JSON_STRING ? opj->str_val : "";

        if (strcmp(name, "ts_parse_payment_method") == 0) {
            json_t *vj = json_obj_get(c, "value");
            char *js = vj ? json_serialize(vj) : strdup("{}");
            char *r = ts_parse_payment_method(js);
            printf("%s\n", r);
            free(r);
            free(js);
        }
    }
    json_free(root);
    return 0;
}
