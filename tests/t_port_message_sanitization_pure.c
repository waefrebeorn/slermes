/*
 * Oracle harness for agent/message_sanitization.py pure helpers.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "message_sanitization_pure.h"

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

        if (strcmp(name, "ms_family_rule") == 0) {
            const char *fam = json_get_str(c, "family", "");
            const void *r = msg_sanitize_family_rule(fam);
            printf(r ? "found\n" : "none\n");
        } else if (strcmp(name, "ms_matches_reasoning_echo_family") == 0) {
            const char *fam = json_get_str(c, "family", "");
            const char *prov = json_get_str(c, "provider", "");
            const char *model = json_get_str(c, "model", "");
            const char *bu = json_get_str(c, "base_url", "");
            bool r = msg_sanitize_matches_reasoning_echo_family(fam, prov, model, bu);
            printf(r ? "true\n" : "false\n");
        } else if (strcmp(name, "ms_reasoning_echo_family") == 0) {
            const char *prov = json_get_str(c, "provider", "");
            const char *model = json_get_str(c, "model", "");
            const char *bu = json_get_str(c, "base_url", "");
            const char *r = msg_sanitize_reasoning_echo_family(prov, model, bu);
            printf(r ? "%s\n" : "null\n", r);
        } else if (strcmp(name, "ms_needs_reasoning_echo") == 0) {
            const char *prov = json_get_str(c, "provider", "");
            const char *model = json_get_str(c, "model", "");
            const char *bu = json_get_str(c, "base_url", "");
            bool r = msg_sanitize_needs_reasoning_echo(prov, model, bu);
            printf(r ? "true\n" : "false\n");
        } else if (strcmp(name, "ms_deterministic_call_id") == 0) {
            const char *fn = json_get_str(c, "fn_name", "");
            const char *args = json_get_str(c, "arguments", "");
            int idx = (int)json_get_num(c, "index", 0);
            char *r = msg_sanitize_deterministic_call_id(fn, args, idx);
            printf("%s\n", r);
            free(r);
        } else if (strcmp(name, "ms_coalesce_tool_call_id") == 0) {
            json_t *vj = json_obj_get(c, "value");
            const char *r = msg_sanitize_coalesce_tool_call_id(vj);
            printf("%s\n", r);
        }
    }
    json_free(root);
    return 0;
}
