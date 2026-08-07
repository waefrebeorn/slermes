/*
 * Oracle harness for agent/context_breakdown.py renderers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "context_breakdown.h"

/* Serialize an object-valued case field to a malloc'd string. */
static char *serialize_field(json_t *c, const char *key, const char *fallback) {
    json_t *vj = json_obj_get(c, key);
    if (vj && vj->type == JSON_OBJECT) return json_serialize(vj);
    return strdup(fallback);
}

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

        if (strcmp(name, "cb_bytes_to_tokens") == 0) {
            json_t *vj = json_obj_get(c, "value");
            long v = vj && vj->type == JSON_NUMBER ? (long)vj->num_val : -1;
            long r = context_breakdown_bytes_to_tokens(v);
            if (r < 0) printf("null\n");
            else printf("%ld\n", r);
        } else if (strcmp(name, "cb_render_grid") == 0 ||
                   strcmp(name, "cb_render_category_lines") == 0) {
            char *payload = serialize_field(c, "payload", "{}");
            size_t nl = 0;
            char **lines = NULL;
            if (strcmp(name, "cb_render_grid") == 0)
                lines = context_breakdown_render_grid(payload, &nl);
            else
                lines = context_breakdown_render_category_lines(payload, &nl);
            for (size_t j = 0; j < nl; j++) {
                printf("%s\n", lines[j]);
                free(lines[j]);
            }
            free(lines);
            free(payload);
        } else if (strcmp(name, "cb_render_details_lines") == 0) {
            char *details = serialize_field(c, "details", "{}");
            size_t nl = 0;
            char **lines = context_breakdown_render_details_lines(details, &nl);
            for (size_t j = 0; j < nl; j++) {
                printf("%s\n", lines[j]);
                free(lines[j]);
            }
            free(lines);
            free(details);
        } else if (strcmp(name, "cb_render_lines") == 0) {
            char *payload = serialize_field(c, "payload", "{}");
            char *details = serialize_field(c, "details", "");
            int grid = json_get_bool(c, "grid", true) ? 1 : 0;
            size_t nl = 0;
            char **lines = context_breakdown_render_lines(payload,
                                                          *details ? details : NULL,
                                                          grid, &nl);
            for (size_t j = 0; j < nl; j++) {
                printf("%s\n", lines[j]);
                free(lines[j]);
            }
            free(lines);
            free(payload);
            free(details);
        }
    }
    json_free(root);
    return 0;
}
