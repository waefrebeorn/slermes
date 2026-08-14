/*
 * t_port_read_extract_pure.c — oracle harness for the pure range/format
 * helpers of tools/read_extract.py ported to C (port_tools_read_extract.c).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, and emits one
 * JSON object per case. The Python oracle recomputes from LIVE
 * tools/read_extract.py; the runner exact-compares.
 *
 * Tested: _group_ranges, _page_ranges, _gap_map, _human_size, _base64_bytes,
 * _clean_stream_text.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "read_extract.h"

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
    static char buf[4][32768];
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
            else if (c == '\r') { *q++ = '\\'; *q++ = 'r'; }
            else if (c < 32) { *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
                *q++ = (c > 15 ? '1' : '0'); *q++ = (c % 16 < 10 ? c % 16 + '0' : c % 16 - 10 + 'a'); }
            else if ((q - base) < 32750) *q++ = c;
        }
    }
    *q++ = '"'; *q = '\0';
    return base;
}

/* parse an int array field from a JSON object */
static void read_int_array(json_t *obj, const char *key, int *out, int *n, int cap) {
    *n = 0;
    json_t *arr = json_obj_get(obj, key);
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count && (int)i < cap; i++) {
        json_t *e = arr->c.items[i];
        if (e->type == JSON_NUMBER) out[*n] = (int)e->num_val;
        else if (e->type == JSON_STRING) out[*n] = atoi(e->str_val);
        (*n)++;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_read_extract_pure <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    int pages[1024], counts[1024], empty[1024];
    int n, nc, ne;
    const char *texts[1024];

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");
        json_t *arg = json_obj_get(c, "arg");
        const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";

        if (strcmp(op, "group_ranges") == 0) {
            read_int_array(arg, "pages", pages, &n, 1024);
            int runs[2048]; int cap = 1024;
            int nr = read_extract_group_ranges(pages, n, runs, &cap);
            printf("{\"op\":\"group_ranges\",\"runs\":[");
            for (int k = 0; k < nr; k++) {
                if (k) printf(",");
                printf("[%d,%d]", runs[2*k], runs[2*k+1]);
            }
            printf("]}\n");
        } else if (strcmp(op, "page_ranges") == 0) {
            read_int_array(arg, "pages", pages, &n, 1024);
            char *r = read_extract_page_ranges(pages, n);
            printf("{\"op\":\"page_ranges\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "gap_map") == 0) {
            read_int_array(arg, "counts", counts, &nc, 1024);
            read_int_array(arg, "empty", empty, &ne, 1024);
            json_t *txtarr = json_obj_get(arg, "texts");
            for (int k = 0; k < 1024; k++) texts[k] = "";
            if (txtarr && txtarr->type == JSON_ARRAY) {
                for (size_t k = 0; k < txtarr->c.count && (int)k < 1024; k++) {
                    json_t *e = txtarr->c.items[k];
                    texts[k] = (e->type == JSON_STRING) ? e->str_val : "";
                }
            }
            char *r = read_extract_gap_map(counts, nc, texts, empty, ne);
            printf("{\"op\":\"gap_map\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "human_size") == 0) {
            long long nb = (arg && arg->type == JSON_NUMBER) ? (long long)arg->num_val : 0;
            char *r = read_extract_human_size(nb);
            printf("{\"op\":\"human_size\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "base64_bytes") == 0) {
            long long r = read_extract_base64_bytes(s);
            printf("{\"op\":\"base64_bytes\",\"out\":%lld}\n", r);
        } else if (strcmp(op, "clean_stream") == 0) {
            char *r = read_extract_clean_stream_text(s);
            printf("{\"op\":\"clean_stream\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "nb_output_text") == 0) {
            char *r = read_extract_notebook_output_text(s);
            printf("{\"op\":\"nb_output_text\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "nb_outputs") == 0) {
            json_t *obj = s && *s ? json_parse(s, NULL) : NULL;
            json_t *cell = obj ? json_obj_get(obj, "cell") : NULL;
            const char *jq = obj ? json_get_str(obj, "jq", "") : "";
            const char *fn = obj ? json_get_str(obj, "filename", "") : "";
            char *cellstr = cell ? json_serialize(cell) : NULL;
            char *r = read_extract_notebook_outputs(cellstr ? cellstr : "",
                                                     jq, fn);
            printf("{\"op\":\"nb_outputs\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r); free(cellstr);
            if (obj) json_free(obj);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
