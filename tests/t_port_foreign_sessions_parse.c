/*
 * t_port_foreign_sessions_parse.c — parity harness for
 * foreign_sessions_parse_claude_session / foreign_sessions_parse_codex_session
 * in src/hermes_cli/foreign_sessions_pure.c
 * (hermes_cli/foreign_sessions.py: parse_claude_session, parse_codex_session).
 *
 * Reads a JSON fixture (argv[1]) with "ops": each op
 *   {"op":"claude","file":"<path>"} -> read file, call parse_claude_session
 *   {"op":"codex","file":"<path>"}  -> read file, call parse_codex_session
 * Emits one JSON line per op: {"op":..,"file":..,"out":<json>} matching
 * sta_oracle_foreign_sessions_parse.py.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *foreign_sessions_parse_claude_session(const char *jsonl);
char *foreign_sessions_parse_codex_session(const char *jsonl);

static char *read_all(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); size_t r = fread(b, 1, n, f); b[r] = '\0';
    fclose(f); return b;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_all(argv[1]);
    if (!raw) { fprintf(stderr, "cannot read fixture\n"); return 1; }
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *op = json_get_str(c, "op", "");
            const char *file = json_get_str(c, "file", "");
            char *content = read_all(file);
            if (!content) { printf("{\"op\":\"%s\",\"file\":\"%s\",\"out\":null}\n", op, file); continue; }
            char *out = NULL;
            if (strcmp(op, "claude") == 0) out = foreign_sessions_parse_claude_session(content);
            else if (strcmp(op, "codex") == 0) out = foreign_sessions_parse_codex_session(content);
            free(content);
            json_t *o = json_new_object();
            json_object_set(o, "op", json_string(op));
            json_object_set(o, "file", json_string(file));
            json_t *arr = out ? json_parse(out, NULL) : NULL;
            if (arr) json_object_set(o, "out", arr); else json_object_set(o, "out", json_new_null());
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line); json_free(o); free(out);
        }
    }
    json_free(root); free(raw);
    return 0;
}
