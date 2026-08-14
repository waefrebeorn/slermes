/*
 * t_port_plugins_pure.c — oracle harness for the pure plugin helpers of
 * hermes_cli/plugins.py ported to C (port_plugins_wrappers.c).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE hermes_cli/plugins.py.
 *
 * Tested: _portable_skill_namespace, _display_author.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"

extern char *plugins_portable_skill_namespace(const char *key);
extern char *plugins_display_author(const char *value_json);

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

static char *jesc(const char *s) {
    static char buf[4][8192];
    static int cur = 0;
    char *q = buf[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
            else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
            else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
            else if (c < 32) { *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
                *q++ = (c > 15 ? '1' : '0'); *q++ = (c % 16 < 10 ? c % 16 + '0' : c % 16 - 10 + 'a'); }
            else if ((q - base) < 8170) *q++ = c;
        }
    }
    *q++ = '"'; *q = '\0';
    return base;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_plugins_pure <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");
        json_t *arg = json_obj_get(c, "arg");
        const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
        int null_arg = (arg && arg->type == JSON_NULL);

        if (strcmp(op, "namespace") == 0) {
            char *r = plugins_portable_skill_namespace(null_arg ? NULL : s);
            printf("{\"op\":\"namespace\",\"arg\":%s,\"out\":%s}\n",
                   null_arg ? "null" : jesc(s), jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "display_author") == 0) {
            char *r = plugins_display_author(null_arg ? NULL : s);
            printf("{\"op\":\"display_author\",\"arg\":%s,\"out\":%s}\n",
                   null_arg ? "null" : jesc(s), jesc(r ? r : ""));
            free(r);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
