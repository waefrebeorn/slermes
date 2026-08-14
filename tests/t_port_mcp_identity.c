/*
 * t_port_mcp_identity.c — oracle harness for the pure trust/identity-header
 * helpers of tools/mcp_tool.py ported to C (mcp_tool_pure.c).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE tools/mcp_tool.py.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "mcp_tool_pure.h"
#include "port_gateway_run_deps.h"  /* gw_active_profile_name */

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

static json_t *parse_arg(const json_t *c, const char *key) {
    json_t *v = json_obj_get(c, key);
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) {
        char *s = strdup(v->str_val);
        json_t *p = json_parse(s, NULL);
        free(s);
        return p;
    }
    return json_copy(v);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_mcp_identity <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");

        if (strcmp(op, "normalize_trust") == 0) {
            json_t *arg = parse_arg(c, "arg");
            char *r = mcp_normalize_server_trust(arg);
            printf("{\"op\":\"normalize_trust\",\"out\":%s}\n", jesc(r));
            free(r);
            if (arg) json_free(arg);
        } else if (strcmp(op, "read_only_hint") == 0) {
            json_t *arg = parse_arg(c, "arg");
            bool r = mcp_annotation_read_only_hint(arg);
            printf("{\"op\":\"read_only_hint\",\"out\":%s}\n", r ? "true" : "false");
            if (arg) json_free(arg);
        } else if (strcmp(op, "resolve_identity") == 0) {
            const char *srv = json_get_str(c, "server", "");
            json_t *cfg = parse_arg(c, "cfg");
            json_t *r = mcp_resolve_identity_header(srv, cfg);
            char *out = r ? json_serialize(r) : NULL;
            printf("{\"op\":\"resolve_identity\",\"out\":%s}\n", out ? out : "null");
            free(out);
            if (r) json_free(r);
            if (cfg) json_free(cfg);
        } else if (strcmp(op, "apply_identity") == 0) {
            const char *srv = json_get_str(c, "server", "");
            json_t *cfg = parse_arg(c, "cfg");
            json_t *hdr = parse_arg(c, "headers");
            json_t *r = mcp_apply_identity_header(srv, cfg, hdr ? hdr : json_object());
            char *out = json_serialize(r);
            printf("{\"op\":\"apply_identity\",\"out\":%s}\n", out ? out : "null");
            free(out);
            if (hdr) json_free(hdr);
            if (cfg) json_free(cfg);
            json_free(r);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
