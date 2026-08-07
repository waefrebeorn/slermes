/* Oracle harness for agent/monitoring/gateway_health_export.py pure helpers. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "port_agent_monitoring_health_export_pure.h"

static void json_print_str(const char *s)
{
    putchar('"');
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
    putchar('"');
}

static char *to_raw_str(json_t *rj)
{
    if (!rj) return strdup("");
    if (rj->type == JSON_STRING) return strdup(rj->str_val);
    if (rj->type == JSON_NULL) return strdup("");
    char *s = json_serialize(rj);
    return s ? s : strdup("");
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

        if (strcmp(op, "_redact_string") == 0) {
            char *raw_str = to_raw_str(json_obj_get(c, "raw"));
            json_t *lj = json_obj_get(c, "limit");
            int limit = lj && lj->type == JSON_NUMBER ? (int)lj->num_val : 500;
            char *r = ghe_redact_string(raw_str, limit);
            json_print_str(r ? r : "");
            printf("\n");
            free(r); free(raw_str);
        } else if (strcmp(op, "_safe_resource_attributes") == 0) {
            json_t *rj = json_obj_get(c, "raw_json");
            const char *raw_json = rj && rj->type == JSON_STRING ? rj->str_val : "{}";
            char *r = ghe_safe_resource_attributes(raw_json);
            if (r) { printf("%s\n", r); free(r); }
            else { printf("{}\n"); }
        } else if (strcmp(op, "_version") == 0) {
            json_print_str(ghe_version()); printf("\n");
        } else if (strcmp(op, "_profile") == 0) {
            json_print_str(ghe_profile()); printf("\n");
        } else if (strcmp(op, "_supervision_mode") == 0) {
            json_print_str(ghe_supervision_mode()); printf("\n");
        } else if (strcmp(op, "_install_id") == 0) {
            json_t *cj = json_obj_get(c, "config");
            char *config_json = cj ? json_serialize(cj) : strdup("{}");
            char *r = ghe_install_id(config_json);
            json_print_str(r); printf("\n");
            free(r);
            free(config_json);
        } else if (strcmp(op, "_runtime_resource_attributes") == 0) {
            json_t *cj = json_obj_get(c, "config");
            char *config_json = cj ? json_serialize(cj) : strdup("{}");
            json_t *sj = json_obj_get(c, "telemetry_scope");
            const char *scope = sj && sj->type == JSON_STRING ? sj->str_val : "hermes.gateway.diagnostics";
            char *r = ghe_runtime_resource_attributes(config_json, scope);
            if (r) { printf("%s\n", r); free(r); }
            else { printf("{}\n"); }
            free(config_json);
        } else {
            printf("UNKNOWN_OP\n");
        }
    }

    json_free(root);
    return 0;
}
