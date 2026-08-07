/*
 * Oracle harness for agent/model_metadata.py _msg_fingerprint.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"

/* Port lives in src/cli/port_agent_model_metadata.c */
char *mm_msg_fingerprint(const char *value_json, void *pins_out);

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char *err = NULL;
    json_t *root = json_parse_file(argv[1], &err);
    if (err) { free(err); }
    if (!root || root->type != JSON_ARRAY) {
        if (root) json_free(root);
        return 1;
    }
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        if (!c || c->type != JSON_OBJECT) continue;
        const char *op = json_get_str(c, "op", "");
        if (strcmp(op, "mm_msg_fingerprint") == 0) {
            char *in = NULL;
            json_t *vj = json_obj_get(c, "value");
            if (vj) in = json_serialize(vj);
            char *out = mm_msg_fingerprint(in, NULL);
            printf("%s\n", out ? out : "null");
            free(out);
            free(in);
        }
    }
    json_free(root);
    return 0;
}
