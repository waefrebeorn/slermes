/*
 * t_port_skills_hub_filter.c — oracle harness for the PURE
 * hub_filter_results_by_provider helper in src/skills_hub.c (faithful port of
 * tools/skills_hub.py:_filter_results_by_provider). Exact, case-insensitive
 * provider match over a hub_skill_meta_t array.
 */

#include "hermes_skills_hub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXN 64

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

int main(void) {
    char line[4096];
    hub_skill_meta_t entries[MAXN];
    int n = 0;
    char pending_filter[256] = "";

    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line) continue;

        if (strncmp(line, "filter ", 7) == 0) {
            snprintf(pending_filter, sizeof(pending_filter), "%s", line + 7);
            continue;
        }

        if (strncmp(line, "===", 3) == 0) {
            const char *prov = pending_filter;
            hub_skill_meta_t *work = malloc(sizeof(hub_skill_meta_t) * (size_t)(n > 0 ? n : 1));
            for (int i = 0; i < n; i++) work[i] = entries[i];

            bool ok = hub_filter_results_by_provider(work, n, prov);
            printf("{\"provider\":");
            emit_json_string(prov);
            printf(",\"matched\":%s,\"kept\":[", ok ? "true" : "false");
            for (int i = 0; i < n; i++) {
                if (i) printf(",");
                printf("{");
                printf("\"slug\":"); emit_json_string(work[i].slug);
                printf(",\"provider\":"); emit_json_string(work[i].provider);
                printf("}");
            }
            printf("]}\n");
            free(work);
            n = 0;
            pending_filter[0] = '\0';
            continue;
        }

        /* entry: slug|name|provider */
        char *e = line;
        char *bar = strchr(e, '|');
        char *name = "", *prov = "";
        if (bar) {
            *bar = '\0';
            char *rest = bar + 1;
            char *bar2 = strchr(rest, '|');
            if (bar2) { *bar2 = '\0'; name = rest; prov = bar2 + 1; }
            else name = rest;
        }
        memset(&entries[n], 0, sizeof(entries[n]));
        snprintf(entries[n].slug, sizeof(entries[n].slug), "%s", e);
        snprintf(entries[n].name, sizeof(entries[n].name), "%s", name);
        snprintf(entries[n].provider, sizeof(entries[n].provider), "%s", prov);
        n++;
    }
    return 0;
}
