/*
 * t_port_threat_patterns.c — oracle harness for the PURE threat-pattern
 * scanner in src/cli/port_tools_threat_patterns.c (faithful port of
 * tools/threat_patterns.py:scan_for_threats). Emits the SET of matched
 * pattern ids (sorted) so the comparison is order-independent.
 */

#include "threat_patterns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

/* Parse the JSON array returned by scan_for_threats ("[\"a\",\"b\",...]"),
 * collect the quoted strings, sort them, and emit a sorted JSON array. */
static void emit_sorted_matches(const char *arr) {
    char *ids[128];
    int n = 0;
    const char *p = arr;
    while (*p && n < 128) {
        const char *q = strchr(p, '"');
        if (!q) break;
        q++;
        const char *r = strchr(q, '"');
        if (!r) break;
        size_t len = (size_t)(r - q);
        char *id = (char *)malloc(len + 1);
        memcpy(id, q, len);
        id[len] = '\0';
        ids[n++] = id;
        p = r + 1;
    }
    /* simple insertion sort */
    for (int i = 1; i < n; i++) {
        char *key = ids[i];
        int j = i - 1;
        while (j >= 0 && strcmp(ids[j], key) > 0) { ids[j+1] = ids[j]; j--; }
        ids[j+1] = key;
    }
    putchar('[');
    for (int i = 0; i < n; i++) {
        if (i) putchar(',');
        emit_json_string(ids[i]);
        free(ids[i]);
    }
    putchar(']');
}

/* Decode \n token to a real newline. */
static char *decode_nl(const char *in) {
    if (!in) return NULL;
    size_t len = strlen(in);
    char *out = (char *)malloc(len + 1);
    size_t oi = 0;
    for (size_t i = 0; i < len; ) {
        if (strncmp(in + i, "\\n", 2) == 0) { out[oi++] = '\n'; i += 2; }
        else out[oi++] = in[i++];
    }
    out[oi] = '\0';
    return out;
}

int main(void) {
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        /* op 'scan' : scan <scope> | <content> */
        if (strncmp(line, "scan ", 5) == 0) {
            char *rest = line + 5;
            char *bar = strstr(rest, " | ");
            if (!bar) { printf("{\"op\":\"scan\",\"error\":\"bad-fixture\"}\n"); continue; }
            *bar = '\0';
            char *scope = rest;
            char *content = bar + 3;
            char *dec = decode_nl(content);
            char *res = cli_tools_threat_patterns_scan_for_threats(dec ? dec : "", scope);
            printf("{\"op\":\"scan\",\"scope\":");
            emit_json_string(scope);
            printf(",\"content\":");
            emit_json_string(dec ? dec : "");
            printf(",\"found\":");
            emit_sorted_matches(res ? res : "[]");
            printf("}\n");
            free(res); free(dec);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
