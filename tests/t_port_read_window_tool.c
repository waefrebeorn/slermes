/*
 * t_port_read_window_tool.c — parity harness for src/tools/read_window_tool.c
 * (tools/read_window_tool.py). Ops:
 *   {"kind":"read_window","callback":"null|error|empty|json|text","payload":"..."}
 *   {"kind":"register"}
 */
#include "hermes_json.h"
#include "read_window_tool.h"
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

static const char *g_mode = "";
static const char *g_payload = "";

static int fake_cb(void *ctx, char **out, char **err) {
    (void)ctx;
    if (strcmp(g_mode, "error") == 0) {
        size_t n = strlen(g_payload);
        *err = malloc(n + 1);
        memcpy(*err, g_payload, n + 1);
        return -1;
    }
    *out = strdup(g_payload);
    return 0;
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
            if (strcmp(kind, "read_window") == 0) {
                g_mode = json_get_str(c, "callback", "null");
                g_payload = json_get_str(c, "payload", "");
                char *res;
                if (strcmp(g_mode, "null") == 0)
                    res = read_window_below_tool(NULL, NULL);
                else
                    res = read_window_below_tool(fake_cb, NULL);
                json_object_set(o, "out",
                                json_copy(json_parse(res ? res : "null", NULL)));
                free(res);
            } else if (strcmp(kind, "register") == 0) {
                int ok = read_window_tool_register();
                json_t *info = json_new_object();
                json_object_set(info, "registered", json_bool(ok));
                if (ok) {
                    char *raw = registry_describe("read_window_below");
                    json_t *d = raw ? json_parse(raw, NULL) : NULL;
                    if (d) {
                        json_object_set(info, "name", json_copy(json_object_get(d, "name")));
                        json_object_set(info, "toolset", json_copy(json_object_get(d, "toolset")));
                        json_object_set(info, "description", json_copy(json_object_get(d, "description")));
                        json_free(d);
                    }
                    free(raw);
                }
                json_object_set(o, "out", info);
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
