/*
 * t_port_battery.c — oracle harness for the battery helpers in
 * src/agent/port_battery.c (ports of agent/battery.py).
 * Reads the fixture from argv[1] (one op per line), emits one JSON object
 * per line. Op:  status <available> <percent|null> <plugged|null>
 * Ops mirror sta_oracle_battery.py (category / glyph / format_battery).
 */

#include "battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_line_alloc(FILE *fp) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, fp);
    if (n < 0) { free(line); return NULL; }
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
    return line;
}

static int parse_bool(const char *s, bool *out) {
    if (strcmp(s, "true") == 0) { *out = true; return 1; }
    if (strcmp(s, "false") == 0) { *out = false; return 1; }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.in>\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char *line;
    while ((line = read_line_alloc(fp)) != NULL) {
        if (!*line || line[0] == '#') { free(line); continue; }

        char op[32];
        const char *rest = "";
        size_t i = 0;
        while (line[i] == ' ') i++;
        size_t s = i;
        while (line[i] && line[i] != ' ') op[i - s] = line[i], i++;
        op[i - s] = '\0';
        if (line[i] == ' ') rest = line + i + 1;

        if (strcmp(op, "status") == 0) {
            char av[16] = "", pc[16] = "", pl[16] = "";
            sscanf(rest, "%15s %15s %15s", av, pc, pl);
            bool available = false;
            if (!parse_bool(av, &available)) { free(line); continue; }

            int pct = 0;
            bool has_pct = (pc[0] != '\0' && strcmp(pc, "null") != 0);
            int pct_v = 0;
            if (has_pct) pct_v = atoi(pc);
            bool has_pl = (pl[0] != '\0' && strcmp(pl, "null") != 0);
            bool pl_v = false;
            if (has_pl) parse_bool(pl, &pl_v);

            battery_status_t *st = battery_status_make(
                available, has_pct ? &pct_v : NULL, has_pl ? &pl_v : NULL);

            int out_pct = 0;
            bool got_pct = battery_status_percent(st, &out_pct);
            bool out_pl = false;
            bool got_pl = battery_status_plugged(st, &out_pl);

            printf("{\"available\":%s,",
                   battery_status_available(st) ? "true" : "false");
            printf("\"percent\":");
            if (got_pct) printf("%d", out_pct);
            else printf("null");
            printf(",\"plugged\":");
            if (got_pl) printf("%s", out_pl ? "true" : "false");
            else printf("null");
            printf(",\"category\":\"%s\",", battery_category(st));
            /* glyph: emit raw UTF-8 bytes (matches oracle ensure_ascii=False) */
            printf("\"glyph\":\"%s\",", battery_glyph(st));
            char *fmt = format_battery(st);
            printf("\"format\":\"%s\"}", fmt ? fmt : "");
            free(fmt);
            battery_status_free(st);
            printf("\n");
        } else {
            printf("{\"op\":\"unknown\",\"raw\":%s}\n", op);
        }
        free(line);
    }
    fclose(fp);
    return 0;
}
