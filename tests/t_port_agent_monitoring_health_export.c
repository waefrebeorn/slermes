/*
 * Oracle harness for agent/monitoring/gateway_health_export.py pure helpers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "port_agent_monitoring_health_export.h"

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
        if (strcmp(op, "he_resolve_headers") == 0) {
            /* Set test env vars from fixture["env"] */
            json_t *envj = json_obj_get(c, "env");
            if (envj && envj->type == JSON_OBJECT) {
                for (size_t j = 0; j < envj->c.count; j++) {
                    const char *name = envj->c.keys[j];
                    json_t *vj = envj->c.items[j];
                    if (vj && vj->type == JSON_STRING)
                        setenv(name, vj->str_val, 1);
                }
            }
            char *in = NULL;
            json_t *vj = json_obj_get(c, "value");
            if (vj) in = json_serialize(vj);
            char *out = he_resolve_headers(in);
            printf("%s\n", out ? out : "null");
            if (out) free(out);
            if (in) free(in);
            /* Clear test env vars */
            if (envj && envj->type == JSON_OBJECT) {
                for (size_t j = 0; j < envj->c.count; j++)
                    unsetenv(envj->c.keys[j]);
            }
        }
    }
    json_free(root);
    return 0;
}
