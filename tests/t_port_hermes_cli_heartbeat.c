/*
 * t_port_hermes_cli_heartbeat.c — Oracle harness for hermes_cli/heartbeat.py.
 *
 * Reads packed test vectors from the fixture file (argv[1]).
 * Each line: op|arg0|arg1|...
 * Outputs one JSON result object per line to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

#include "port_hermes_cli_heartbeat.h"
#include "hermes_json.h"

static char *json_esc(const char *s) {
    if (!s) return strdup("null");
    size_t need = 2;
    for (const char *p = s; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') need += 2;
        else if (c < 0x20) need += 6;
        else need++;
    }
    char *out = malloc(need + 1);
    if (!out) return NULL;
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c < 0x20) {
            *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
            *q++ = "0123456789abcdef"[c >> 4];
            *q++ = "0123456789abcdef"[c & 15];
        } else *q++ = c;
    }
    *q++ = '"'; *q = 0;
    return out;
}

static void emit(const char *op, const char *args_json, const char *result_json) {
    printf("{\"op\":\"%s\",\"args\":%s,\"result\":%s}\n", op, args_json ? args_json : "[]", result_json ? result_json : "null");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fixture"); return 2; }
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* strip trailing newline */
        size_t len = strlen(line);
        while (len && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (!*line || line[0] == '#') continue;
        /* split on '|' */
        char *parts[8] = {0}; int n = 0;
        char *p = line;
        char *saveptr = NULL;
        /* manual split to handle empty pieces */
        char *tok = p;
        for (int i = 0; i < 8 && n < 8; i++) {
            char *bar = strchr(tok, '|');
            if (bar) { *bar = 0; parts[n++] = tok; tok = bar + 1; }
            else { parts[n++] = tok; break; }
        }
        if (n == 0) continue;
        const char *op = parts[0];

        if (strcmp(op, "parse_interval") == 0) {
            const char *text = n > 1 ? parts[1] : "";
            int out = 0;
            int rc = hb_parse_interval(text, &out);
            /* Python returns None (null), -1, or int. */
            char result[64];
            if (rc == 0) strcpy(result, "null");
            else if (out == INT_MIN) strcpy(result, "-1");
            else snprintf(result, sizeof(result), "%d", out);
            char args[512];
            snprintf(args, sizeof(args), "[%s]", json_esc(text));
            emit(op, args, result);

        } else if (strcmp(op, "format_interval") == 0) {
            int seconds = n > 1 && parts[1] ? atoi(parts[1]) : 0;
            char *result = hb_format_interval(seconds);
            char args[64];
            snprintf(args, sizeof(args), "[%d]", seconds);
            char *esc = json_esc(result);
            printf("{\"op\":\"%s\",\"args\":%s,\"result\":%s}\n", op, args, esc);
            free(esc); free(result);

        } else if (strcmp(op, "is_due") == 0) {
            json_t *j = json_parse(parts[1], NULL);
            hb_state_t *state = hb_state_from_json(parts[1]);
            double now = (n > 2 && parts[2]) ? atof(parts[2]) : 0.0;
            bool due = state ? hb_state_is_due(state, now) : false;
            /* Emit args as a real JSON array [ <object>, now ] so the
             * normalizer canonicalizes the nested object identically on
             * both C and Python sides (mirroring the python oracle). */
            char *obj_json = j ? json_dumps(j, 0) : strdup("{}");
            char args[8192];
            snprintf(args, sizeof(args), "[%s,%g]", obj_json ? obj_json : "{}", now);
            free(obj_json);
            char result[8];
            snprintf(result, sizeof(result), "%s", due ? "true" : "false");
            emit(op, args, result);
            hb_state_free(state);
            json_free(j);

        } else if (strcmp(op, "render_prompt") == 0) {
            json_t *j = json_parse(parts[1], NULL);
            hb_state_t *state = hb_state_from_json(parts[1]);
            char *result = state ? hb_state_render_prompt(state) : NULL;
            char *obj_json = j ? json_dumps(j, 0) : strdup("{}");
            char args[8192];
            snprintf(args, sizeof(args), "[%s]", obj_json ? obj_json : "{}");
            free(obj_json);
            char *rjs = json_esc(result);
            emit(op, args, rjs);
            free(rjs); free(result);
            hb_state_free(state);
            json_free(j);

        } else {
            emit(op, "[]", "{\"error\":\"unknown op\"}");
        }
    }
    fclose(f);
    return 0;
}
