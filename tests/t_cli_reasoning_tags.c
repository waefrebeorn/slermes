/*
 * t_cli_reasoning_tags.c — differential oracle harness for
 * cli_strip_reasoning_tags (port of cli.py:_strip_reasoning_tags).
 *
 * Contract: reads tests/oracle/fixtures/cli_reasoning_tags/cases.json,
 * runs each input through the C port, prints one JSON result per case:
 *   {"i": N, "got": "...", "expected": "...", "match": true|false}
 * Exit 0 iff all match. The Python oracle recomputes `expected` from the
 * LIVE cli.py and compares semantically.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *cli_strip_reasoning_tags(const char *text);

static char *slurp(const char *path) {
    FILE *fh = fopen(path, "rb");
    if (!fh) return NULL;
    fseek(fh, 0, SEEK_END);
    long n = ftell(fh);
    fseek(fh, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(fh); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, fh);
    buf[rd] = '\0';
    fclose(fh);
    return buf;
}

/* Minimal JSON string-field extractor for our own fixture file:
 * finds "key": "value" and returns a malloc'd unescaped copy. */
static char *json_string_field(const char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\": \"", key);
    const char *hit = strstr(json, pat);
    if (!hit) {
        snprintf(pat, sizeof(pat), "\"%s\":\"", key);
        hit = strstr(json, pat);
        if (!hit) return NULL;
    }
    const char *s = hit + strlen(pat);
    /* walk with escape awareness */
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    while (*s && *s != '"') {
        char c = *s;
        if (c == '\\' && s[1]) {
            s++;
            switch (*s) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            default: c = *s; break;
            }
            s++;
        } else {
            c = *s++;
        }
        if (len + 2 > cap) { cap *= 2; char *tmp = realloc(out, cap); if (!tmp) { free(out); return NULL; } out = tmp; }
        out[len++] = c;
    }
    out[len] = '\0';
    return out;
}

int main(void) {
    const char *fixture = "tests/oracle/fixtures/cli_reasoning_tags/cases.json";
    char *doc = slurp(fixture);
    if (!doc) { fprintf(stderr, "cannot read %s\n", fixture); return 2; }

    int all_match = 1;
    const char *p = doc;
    int idx = 0;
    while ((p = strstr(p, "\"input\"")) != NULL) {
        char *input = json_string_field(p, "input");
        /* find the expected that follows this input */
        const char *rest = p + 6;
        const char *exp_hit = strstr(rest, "\"expected\"");
        char *expected = exp_hit ? json_string_field(exp_hit, "expected") : NULL;
        p = rest;

        if (!input || !expected) { free(input); free(expected); continue; }

        char *got = cli_strip_reasoning_tags(input);
        int match = got && strcmp(got, expected) == 0;
        if (!match) all_match = 0;

        /* emit JSON-escaped output line */
        printf("{\"i\":%d,\"match\":%s,\"input\":\"", idx, match ? "true" : "false");
        for (const char *q = input; *q; q++) {
            unsigned char c = (unsigned char)*q;
            if (c == '"' || c == '\\') printf("\\%c", c);
            else if (c == '\n') printf("\\n");
            else if (c == '\t') printf("\\t");
            else if (c < 0x20) printf("\\u%04x", c);
            else putchar(c);
        }
        printf("\",\"got\":\"");
        for (const char *q = got ? got : ""; *q; q++) {
            unsigned char c = (unsigned char)*q;
            if (c == '"' || c == '\\') printf("\\%c", c);
            else if (c == '\n') printf("\\n");
            else if (c < 0x20) printf("\\u%04x", c);
            else putchar(c);
        }
        printf("\"}\n");

        free(input);
        free(expected);
        free(got);
        idx++;
    }
    free(doc);
    return all_match ? 0 : 1;
}
