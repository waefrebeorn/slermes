/*
 * Oracle harness for agent/monitoring/otlp_exporter.py pure helpers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "port_otlp_exporter.h"

static void json_print_str(const char *s)
{
    printf("\"");
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"') printf("\\\"");
            else if (c == '\\') printf("\\\\");
            else if (c == '\n') printf("\\n");
            else if (c == '\r') printf("\\r");
            else if (c == '\t') printf("\\t");
            else if (c < 0x20) printf("\\u%04x", c);
            else printf("%c", c);
        }
    }
    printf("\"");
}

int main(int argc, char **argv)
{
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
        json_t *oj = json_obj_get(c, "op");
        const char *op = oj && oj->type == JSON_STRING ? oj->str_val : "";
        json_t *vj = json_obj_get(c, "value");
        const char *val = vj && vj->type == JSON_STRING ? vj->str_val : "";

        if (strcmp(op, "_otlp_config") == 0) {
            char *r = otlp_exporter_otlp_config(val ? val : "{}");
            printf("%s\n", r ? r : "{}");
            free(r);
        } else if (strcmp(op, "_resolve_headers") == 0) {
            char *r = otlp_exporter_resolve_headers(val ? val : "{}");
            printf("%s\n", r ? r : "{}");
            free(r);
        } else if (strcmp(op, "_span_attrs") == 0) {
            char *r = otlp_exporter_span_attrs(val ? val : "{}");
            printf("%s\n", r ? r : "{}");
            free(r);
        } else if (strcmp(op, "is_enabled") == 0) {
            printf("%s\n", otlp_exporter_is_enabled(val ? val : "{}") ? "True" : "False");
        } else {
            printf("UNKNOWN_OP\n");
        }
    }
    json_free(root);
    return 0;
}
