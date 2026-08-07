/*
 * Oracle harness for gateway/channel_directory.py _normalize_adapter_channels.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_gateway_channel_directory.h"

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
        if (strcmp(op, "normalize_adapter_channels") == 0) {
            char *in = NULL;
            json_t *vj = json_obj_get(c, "value");
            if (vj) in = json_serialize(vj);
            json_t *out = normalize_adapter_channels(in);
            char *s = out ? json_serialize(out) : strdup("null");
            printf("%s\n", s);
            free(s);
            if (out) json_free(out);
            if (in) free(in);
        }
    }
    json_free(root);
    return 0;
}
