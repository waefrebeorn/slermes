/*
 * t_port_process_bootstrap_pure.c — oracle harness for
 * pbs_interleave_addrinfos (agent/process_bootstrap.py).
 * Reads a JSON array of addrinfo rows per line, prints the interleaved
 * sockaddr sequence as one JSON array per line.
 */
#include "process_bootstrap_pure.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    char *buf = NULL;
    size_t cap = 0, len = 0;
    for (;;) {
        if (len + 4096 > cap) { cap = cap ? cap * 2 : 8192; buf = realloc(buf, cap); }
        size_t r = fread(buf + len, 1, cap - len, f);
        if (!r) break;
        len += r;
    }
    fclose(f);
    if (!buf) return 2;

    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        char *jerr = NULL;
        json_t *arr = json_parse(line, &jerr);
        if (jerr) { free(jerr); continue; }
        if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); continue; }

        size_t n = json_len(arr);
        pbs_addrinfo_t *rows = calloc(n ? n : 1, sizeof(*rows));
        for (size_t i = 0; i < n; i++) {
            json_t *o = json_get(arr, i);
            rows[i].family = (int)json_get_num(o, "family", 0);
            rows[i].socktype = (int)json_get_num(o, "socktype", 0);
            rows[i].proto = (int)json_get_num(o, "proto", 0);
            const char *sa = json_get_str(o, "sockaddr", "");
            snprintf(rows[i].sockaddr, sizeof(rows[i].sockaddr), "%s", sa);
        }
        size_t out_n = 0;
        size_t *order = pbs_interleave_addrinfos(rows, n, &out_n);

        /* emit as JSON array of sockaddrs */
        printf("[");
        for (size_t k = 0; k < out_n; k++) {
            if (k) printf(",");
            json_t *s = json_string(rows[order[k]].sockaddr);
            char *txt = json_serialize(s);
            printf("%s", txt ? txt : "\"\"");
            free(txt);
            json_free(s);
        }
        printf("]\n");
        free(order);
        free(rows);
        if (arr) json_free(arr);
    }
    free(buf);
    return 0;
}
