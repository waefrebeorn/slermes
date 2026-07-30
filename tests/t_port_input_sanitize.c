/*
 * t_port_input_sanitize.c — oracle harness for input_sanitize helpers in
 * src/cli/port_input_sanitize.c (ports of hermes_cli/input_sanitize.py).
 * Reads the fixture from argv[1] (one text per line), decodes \e -> ESC and
 * \n -> newline exactly like the Python oracle, then calls
 * sanitize_user_prompt_text() and emits {"in":...,"out":...} (raw bytes).
 */

#include "input_sanitize.h"
#include "hermes_json.h"
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

/* Decode \e -> ESC (0x1b) and \n -> newline, returning a fresh string. */
static char *decode_escapes(const char *s) {
    size_t slen = strlen(s);
    char *out = malloc(slen + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < slen; i++) {
        if (s[i] == '\\' && i + 1 < slen) {
            if (s[i+1] == 'e') { out[oi++] = '\x1b'; i++; continue; }
            if (s[i+1] == 'n') { out[oi++] = '\n'; i++; continue; }
        }
        out[oi++] = s[i];
    }
    out[oi] = '\0';
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.in>\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char *line;
    while ((line = read_line_alloc(fp)) != NULL) {
        if (!*line || line[0] == '#') { free(line); continue; }
        char *decoded = decode_escapes(line);
        char *result = sanitize_user_prompt_text(decoded);

        /* Emit {"in":...,"out":...} as JSON strings (mirrors oracle
         * json.dumps ensure_ascii=True; libjson escapes control chars the
         * same way). */
        printf("{\"in\":%s,\"out\":%s}\n",
               json_dumps(json_string(decoded), 0),
               result ? json_dumps(json_string(result), 0) : json_dumps(json_string(""), 0));

        free(result);
        free(decoded);
        free(line);
    }
    fclose(fp);
    return 0;
}
