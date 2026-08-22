/*
 * t_port_computer_use_tool.c — parity harness for src/tools/computer_use_tool.c
 * (tools/computer_use_tool.py shim). Ops:
 *   {"kind":"register"}  -> {"out":{"registered":bool,"name":...,"toolset":...,
 *                                  "description":...,"schema_ok":bool}}
 *   {"kind":"requirements"} -> {"out": bool}
 *   {"kind":"release","session_id":"..."} -> {"out": bool}
 */
#include "hermes_json.h"
#include "computer_use_tool.h"
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static json_t *describe_tool(const char *name) {
    char *raw = registry_describe(name);
    json_t *d = raw ? json_parse(raw, NULL) : NULL;
    free(raw);
    return d;  /* may be NULL */
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *rawf = read_file(argv[1]);
    json_t *root = json_parse(rawf, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(rawf); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "register") == 0) {
                int ok = computer_use_tool_register();
                json_t *info = json_new_object();
                json_object_set(info, "registered", json_bool(ok));
                if (ok) {
                    json_t *d = describe_tool("computer_use");
                    if (d) {
                        json_object_set(info, "name", json_copy(json_object_get(d, "name")));
                        json_object_set(info, "toolset", json_copy(json_object_get(d, "toolset")));
                        json_object_set(info, "description", json_copy(json_object_get(d, "description")));
                        json_t *schema = json_object_get(d, "schema");
                        int schema_ok = 0;
                        if (schema && schema->type == JSON_OBJECT) {
                            json_t *n = json_object_get(schema, "name");
                            json_t *params = json_object_get(schema, "parameters");
                            json_t *pt = params ? json_object_get(params, "type") : NULL;
                            schema_ok = (n && n->type == JSON_STRING &&
                                         strcmp(n->str_val, "computer_use") == 0 &&
                                         pt && pt->type == JSON_STRING &&
                                         strcmp(pt->str_val, "object") == 0);
                        }
                        json_object_set(info, "schema_ok", json_bool(schema_ok));
                        json_free(d);
                    }
                }
                json_object_set(o, "out", info);
            } else if (strcmp(kind, "requirements") == 0) {
                int v = computer_use_tool_check_computer_use_requirements();
                json_object_set(o, "out", json_bool(v ? 1 : 0));
            } else if (strcmp(kind, "release") == 0) {
                const char *sid = json_get_str(c, "session_id", "");
                int v = computer_use_tool_release_computer_use_session(sid);
                json_object_set(o, "out", json_bool(v ? 1 : 0));
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(rawf);
    return 0;
}
