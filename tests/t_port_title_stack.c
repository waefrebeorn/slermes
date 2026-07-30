/*
 * t_port_title_stack.c — oracle harness for the title-generation stack:
 *   is_truthy_value            (src/cli/port_utils_truthy.c)
 *   session_title_sanitize     (src/cli/port_session_title.c)
 *   describe_skill_invocation / extract_user_instruction_from_skill_message
 *                              (src/agent/port_agent_skill_commands.c)
 *   summarize_user_message     (src/agent/port_agent_title_generator.c)
 *
 * Reads ops from argv[1] (one per line, "# " comments skipped):
 *   truthy <value|__none__> <default>
 *   sanitize <text> | describe <text> | extract <text> | summarize <text>
 * Decodes \e -> ESC, \n -> newline, \x1e -> RS like the Python oracle, and
 * emits one compact JSON object per line (libjson escapes match
 * json.dumps ensure_ascii=True).
 */

#include "truthy.h"
#include "session_title.h"
#include "skill_scaffolding.h"
#include "title_generator_helpers.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char *read_line_alloc(FILE *fp) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, fp);
    if (n < 0) { free(line); return NULL; }
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
    return line;
}

/* Decode \e -> ESC, \n -> newline, \x1e -> RS. */
static char *decode_escapes(const char *s) {
    size_t slen = strlen(s);
    char *out = malloc(slen + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < slen; i++) {
        if (s[i] == '\\' && i + 1 < slen) {
            if (s[i+1] == 'e') { out[oi++] = '\x1b'; i++; continue; }
            if (s[i+1] == 'n') { out[oi++] = '\n'; i++; continue; }
            if (s[i+1] == 'x' && strncmp(s + i, "\\x1e", 4) == 0) {
                out[oi++] = '\x1e'; i += 3; continue;
            }
        }
        out[oi++] = s[i];
    }
    out[oi] = '\0';
    return out;
}

/* Emit a JSON value: string or null. */
static void emit_str_or_null(const char *s) {
    if (s) {
        char *d = json_dumps(json_string(s), 0);
        fputs(d ? d : "null", stdout);
    } else {
        fputs("null", stdout);
    }
}

static int line_is_blank(const char *s) {
    for (; *s; s++) if (*s != ' ' && *s != '\t') return 0;
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.in>\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char *line;
    while ((line = read_line_alloc(fp)) != NULL) {
        if (line_is_blank(line) || line[0] == '#') { free(line); continue; }

        /* op, _, rest = line.partition(" ") */
        char *sp = strchr(line, ' ');
        const char *op = line;
        const char *rest = "";
        if (sp) { *sp = '\0'; rest = sp + 1; }

        if (strcmp(op, "truthy") == 0) {
            /* val, _, dflt = rest.partition(" ") */
            char *rcopy = strdup(rest);
            char *sp2 = strchr(rcopy, ' ');
            const char *dflt = "";
            if (sp2) { *sp2 = '\0'; dflt = sp2 + 1; }
            bool def = strcmp(dflt, "true") == 0;
            bool out;
            if (strcmp(rcopy, "__none__") == 0) {
                out = is_truthy_value(NULL, def);
            } else {
                char *v = decode_escapes(rcopy);
                out = is_truthy_value(v, def);
                free(v);
            }
            printf("{\"op\":\"truthy\",\"out\":%s}\n", out ? "true" : "false");
            free(rcopy);
        } else if (strcmp(op, "sanitize") == 0) {
            char *text = decode_escapes(rest);
            bool invalid = false;
            char *out = session_title_sanitize(text, &invalid);
            fputs("{\"op\":\"sanitize\",\"out\":", stdout);
            emit_str_or_null(invalid ? NULL : out);
            fputs(",\"error\":", stdout);
            fputs(invalid ? "\"too_long\"" : "null", stdout);
            fputs("}\n", stdout);
            free(out); free(text);
        } else if (strcmp(op, "describe") == 0) {
            char *text = decode_escapes(rest);
            char *out = describe_skill_invocation(text);
            fputs("{\"op\":\"describe\",\"out\":", stdout);
            emit_str_or_null(out);
            fputs("}\n", stdout);
            free(out); free(text);
        } else if (strcmp(op, "extract") == 0) {
            char *text = decode_escapes(rest);
            char *out = extract_user_instruction_from_skill_message(text);
            fputs("{\"op\":\"extract\",\"out\":", stdout);
            emit_str_or_null(out);
            fputs("}\n", stdout);
            free(out); free(text);
        } else if (strcmp(op, "summarize") == 0) {
            char *text = decode_escapes(rest);
            char *out = summarize_user_message(text);
            fputs("{\"op\":\"summarize\",\"out\":", stdout);
            emit_str_or_null(out);
            fputs("}\n", stdout);
            free(out); free(text);
        }
        free(line);
    }
    fclose(fp);
    return 0;
}
