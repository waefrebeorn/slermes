/*
 * t_port_billing_links.c — oracle harness for the billing_links helpers in
 * src/agent/port_billing_links.c (ports of agent/billing_links.py).
 * Reads the fixture from argv[1] (one op per line), emits one JSON object
 * per line. Ops mirror sta_oracle_billing_links.py:
 *   is_nous <provider> <base_url>          -> billing_links_is_nous_inference_route
 *   build <provider> <base_url> <model> [msg] -> billing_block_build + to_json
 */

#include "billing_links.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_bool(const char *op, const char *k1, const char *v1,
                           const char *k2, const char *v2, bool out) {
    printf("{\"op\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\",\"out\":%s}\n",
           op, k1, v1, k2, v2, out ? "true" : "false");
}

static char *read_line_alloc(FILE *fp) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, fp);
    if (n < 0) { free(line); return NULL; }
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
    return line;
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

        if (strcmp(op, "is_nous") == 0) {
            char prov[256] = "", bu[1024] = "";
            sscanf(rest, "%255s %1023s", prov, bu);
            bool out = billing_links_is_nous_inference_route(prov, bu);
            emit_json_bool("is_nous", "provider", prov, "base_url", bu, out);
        } else if (strcmp(op, "build") == 0) {
            char prov[256] = "", bu[1024] = "", mdl[256] = "", msg[1024] = "";
            int consumed = 0;
            sscanf(rest, "%255s %1023s %255s%n", prov, bu, mdl, &consumed);
            const char *m = (consumed > 0) ? rest + consumed : "";
            while (*m == ' ') m++;   /* skip the separator space(s) */
            snprintf(msg, sizeof(msg), "%s", m);
            billing_block_t *b = billing_block_build(prov, bu, mdl, msg);
            char *js = billing_block_to_json(b);
            printf("{\"op\":\"build\",\"out\":%s}\n", js ? js : "{}");
            free(js);
            billing_block_free(b);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":%s}\n", op);
        }
        free(line);
    }
    fclose(fp);
    return 0;
}
