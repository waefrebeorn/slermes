/*
 * t_port_linux_desktop_entry.c — oracle harness for the pure render/path
 * helpers of hermes_cli/linux_desktop_entry.py ported to C
 * (port_linux_desktop_entry.c / linux_desktop_entry.h).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, and emits one
 * JSON object per case. The Python oracle recomputes from LIVE
 * hermes_cli/linux_desktop_entry.py; the runner exact-compares.
 *
 * Tested: lde_is_supported, lde_xdg_data_home, lde_desktop_entry_path,
 * lde_icon_path, lde_quote_exec_arg, lde_render_desktop_entry.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "linux_desktop_entry.h"

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

/* JSON-escape a C string into a static buffer. */
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
    if (argc < 2) { fprintf(stderr, "usage: t_port_linux_desktop_entry <cases.in>\n"); return 1; }
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

        if (strcmp(op, "is_supported") == 0) {
            printf("{\"op\":\"is_supported\",\"out\":%s}\n",
                   lde_is_supported() ? "true" : "false");
        } else if (strcmp(op, "xdg_data_home") == 0) {
            char *r = lde_xdg_data_home();
            printf("{\"op\":\"xdg_data_home\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "desktop_entry_path") == 0) {
            char *r = lde_desktop_entry_path();
            printf("{\"op\":\"desktop_entry_path\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "icon_path") == 0) {
            char *r = lde_icon_path(s);
            printf("{\"op\":\"icon_path\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "quote") == 0) {
            char *r = lde_quote_exec_arg(s);
            printf("{\"op\":\"quote\",\"arg\":%s,\"out\":%s}\n", jesc(s), jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "render") == 0) {
            json_t *obj = s && *s ? json_parse(s, NULL) : NULL;
            const char *exec = "";
            const char *icon = "";
            if (obj) {
                json_t *e = json_obj_get(obj, "exec");
                json_t *ic = json_obj_get(obj, "icon");
                if (e && e->type == JSON_STRING) exec = e->str_val;
                if (ic && ic->type == JSON_STRING) icon = ic->str_val;
            }
            char *r = lde_render_desktop_entry(exec, icon);
            printf("{\"op\":\"render\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
            if (obj) json_free(obj);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
