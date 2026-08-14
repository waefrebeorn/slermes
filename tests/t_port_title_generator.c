/*
 * t_port_title_generator.c — oracle harness for the pure title helpers of
 * agent/title_generator.py ported to C (port_agent_title_generator.c).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE agent/title_generator.py.
 *
 * Tested: strip_control_wrappers, _clean_title, _extract_title_text,
 * is_titleable_user_message, derive_title.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "title_generator_helpers.h"

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
    static char buf[4][16384];
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
            else if ((q - base) < 16360) *q++ = c;
        }
    }
    *q++ = '"'; *q = '\0';
    return base;
}

static void emit_str(const char *op, const char *r) {
    if (r) printf("{\"op\":\"%s\",\"out\":%s}\n", op, jesc(r));
    else printf("{\"op\":\"%s\",\"out\":null}\n", op);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_title_generator <cases.in>\n"); return 1; }
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

        if (strcmp(op, "strip_wrappers") == 0) {
            char *r = title_strip_control_wrappers(s);
            emit_str("strip_wrappers", r); free(r);
        } else if (strcmp(op, "clean_title") == 0) {
            char *r = title_clean_title(null_arg ? NULL : s);
            emit_str("clean_title", r); free(r);
        } else if (strcmp(op, "extract_title") == 0) {
            char *r = title_extract_title_text(null_arg ? NULL : s);
            emit_str("extract_title", r); free(r);
        } else if (strcmp(op, "is_titleable") == 0) {
            printf("{\"op\":\"is_titleable\",\"out\":%s}\n",
                   title_is_titleable_user_message(null_arg ? NULL : s) ? "true" : "false");
        } else if (strcmp(op, "derive_title") == 0) {
            char *r = title_derive_title(null_arg ? NULL : s);
            emit_str("derive_title", r); free(r);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
