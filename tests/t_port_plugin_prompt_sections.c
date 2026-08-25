/*
 * t_port_plugin_prompt_sections.c — oracle harness for the frozen plugin
 * system-prompt sections (format + restore round-trip).
 * Fixture: JSON array of {"op":"format"|"restore", ...} cases.
 */
#include "plugin_prompt_sections.h"

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

    char *jerr = NULL;
    json_t *cases = json_parse(buf, &jerr);
    free(buf);
    if (jerr) { fprintf(stderr, "parse: %s\n", jerr); free(jerr); return 2; }
    if (!cases) return 2;

    for (size_t i = 0; i < json_len(cases); i++) {
        json_t *c = json_get(cases, i);
        const char *op = json_get_str(c, "op", "");
        if (strcmp(op, "format") == 0) {
            json_t *secs = json_obj_get(c, "sections");
            size_t n = secs ? json_len(secs) : 0;
            plugin_section_t *rows = calloc(n + 1, sizeof(*rows));
            for (size_t k = 0; k < n; k++) {
                json_t *s = json_get(secs, k);
                rows[k].id = strdup(json_get_str(s, "id", ""));
                rows[k].content = strdup(json_get_str(s, "content", ""));
            }
            rows[n].id = NULL;
            char *out = plugin_sections_format(rows);
            json_t *js = json_string(out ? out : "");
            char *txt = json_serialize(js);
            printf("%s\n", txt ? txt : "\"\"");
            free(txt);
            json_free(js);
            free(out);
            for (size_t k = 0; k < n; k++) { free(rows[k].id); free(rows[k].content); }
            free(rows);
        } else if (strcmp(op, "restore") == 0) {
            const char *prompt = json_get_str(c, "prompt", "");
            json_t *restored = plugin_sections_restore(prompt);
            /* emit as JSON array of {id,content} */
            printf("[");
            for (size_t k = 0; restored && k < json_len(restored); k++) {
                json_t *o = json_get(restored, k);
                if (k) printf(",");
                const char *id = json_get_str(o, "id", "");
                const char *ct = json_get_str(o, "content", "");
                json_t *pair = json_object();
                json_set(pair, "id", json_string(id));
                json_set(pair, "content", json_string(ct));
                char *txt = json_serialize(pair);
                printf("%s", txt ? txt : "{}");
                free(txt);
                json_free(pair);
            }
            printf("]\n");
            if (restored) json_free(restored);
        }
    }
    json_free(cases);
    return 0;
}
