/*
 * t_port_dashboard_pure.c — oracle harness for the pure helpers of
 * hermes_cli/dashboard_procs.py ported in port_main_wrappers.c.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"

extern int dashboard_procs_is_desktop_local_serve_cmdline(const char *command);
extern char *dashboard_procs_exclude_pids_from_env(void);
extern char *dashboard_procs_hermes_home_dir(void);
extern int dashboard_procs_valid_lockfile_payload(const char *parsed_json, const char *ownership_id);

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
    char *q = buf[cur]; cur = (cur + 1) % 4;
    char *base = q; *q++ = '"';
    if (s) { for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c < 32) { *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
            *q++ = (c > 15 ? '1' : '0'); *q++ = (c % 16 < 10 ? c % 16 + '0' : c % 16 - 10 + 'a'); }
        else if ((q - base) < 8170) *q++ = c;
    } }
    *q++ = '"'; *q = '\0';
    return base;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_dashboard_pure <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");

        if (strcmp(op, "is_desktop_serve") == 0) {
            int r = dashboard_procs_is_desktop_local_serve_cmdline(json_get_str(c, "cmd", ""));
            printf("{\"op\":\"is_desktop_serve\",\"out\":%s}\n", r ? "true" : "false");
        } else if (strcmp(op, "hermes_home") == 0) {
            char *r = dashboard_procs_hermes_home_dir();
            printf("{\"op\":\"hermes_home\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "valid_lockfile") == 0) {
            const char *j = json_get_str(c, "json", "{}");
            const char *oid = json_get_str(c, "ownership", "");
            int r = dashboard_procs_valid_lockfile_payload(j, oid);
            printf("{\"op\":\"valid_lockfile\",\"out\":%s}\n", r ? "true" : "false");
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
