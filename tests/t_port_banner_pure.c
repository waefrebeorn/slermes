/*
 * t_port_banner_pure.c — oracle harness for the pure helpers of
 * hermes_cli/banner.py ported to C (port_banner_pure.c / banner_pure.h).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE hermes_cli/banner.py.
 *
 * Tested: _canonical_github_remote, _is_ssh_remote, _is_official_ssh_remote,
 * _format_context_length, _display_toolset_name.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "banner_pure.h"

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
    if (argc < 2) { fprintf(stderr, "usage: t_port_banner_pure <cases.in>\n"); return 1; }
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
        int arg_is_null = (arg && arg->type == JSON_NULL);
        const char *arglit = arg_is_null ? "null" : jesc(s);

        if (strcmp(op, "canonical") == 0) {
            char *r = banner_canonical_github_remote(s);
            printf("{\"op\":\"canonical\",\"arg\":%s,\"out\":%s}\n", arglit, jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "is_ssh") == 0) {
            printf("{\"op\":\"is_ssh\",\"arg\":%s,\"out\":%s}\n",
                   arglit, banner_is_ssh_remote(s) ? "true" : "false");
        } else if (strcmp(op, "is_official_ssh") == 0) {
            printf("{\"op\":\"is_official_ssh\",\"arg\":%s,\"out\":%s}\n",
                   arglit, banner_is_official_ssh_remote(s) ? "true" : "false");
        } else if (strcmp(op, "ctx_len") == 0) {
            long long v = (arg && arg->type == JSON_NUMBER) ? (long long)arg->num_val : 0;
            char *r = banner_format_context_length(v);
            printf("{\"op\":\"ctx_len\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "toolset") == 0) {
            char *r = banner_display_toolset_name(s);
            printf("{\"op\":\"toolset\",\"arg\":%s,\"out\":%s}\n", arglit, jesc(r ? r : ""));
            free(r);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
