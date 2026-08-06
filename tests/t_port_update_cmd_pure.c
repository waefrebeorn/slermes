/*
 * t_port_update_cmd_pure.c — oracle harness for the PURE, deterministic helpers
 * in src/cli/port_hermes_cli_update_cmd.c (port of hermes_cli/update_cmd.py).
 *
 * Exercises the REAL C functions and emits stable JSON (one object per line)
 * that the Python oracle (tests/sta_oracle_update_cmd_pure.py) reproduces.
 */
#include "port_hermes_cli_update_cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

/* split a line into op + rest (first space separates) */
static void split_op(const char *line, char *op, size_t opsz, const char **rest) {
    size_t i = 0;
    while (*line && *line != ' ' && i + 1 < opsz) op[i++] = *line++;
    op[i] = '\0';
    if (*line == ' ') line++;
    *rest = line;
}

/* Decode \\t -> tab, \\n -> newline (for numstat_paths args). */
static char *decode_escapes(const char *in) {
    if (!in) return NULL;
    size_t n = strlen(in);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < n; ) {
        if (i + 1 < n && in[i] == '\\' && in[i+1] == 't') { out[oi++] = '\t'; i += 2; }
        else if (i + 1 < n && in[i] == '\\' && in[i+1] == 'n') { out[oi++] = '\n'; i += 2; }
        else out[oi++] = in[i++];
    }
    out[oi] = '\0';
    return out;
}

int main(int argc, char **argv) {
    FILE *in = stdin;
    if (argc > 1) {
        in = fopen(argv[1], "r");
        if (!in) { fprintf(stderr, "cannot open fixture %s\n", argv[1]); return 1; }
    }
    char line[16384];
    while (fgets(line, sizeof(line), in)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_op(line, op, sizeof(op), &rest);
        const char *v = rest[0] ? rest : "";

        if (strcmp(op, "backup_mode") == 0) {
            /* Extract raw= value if present */
            const char *raw_eq = strstr(v, "raw=");
            const char *raw_val = "";
            if (raw_eq) {
                raw_val = raw_eq + 4;
            }
            const char *mode = uc_resolve_pre_update_backup_mode(
                strstr(v, "-no-backup-") != NULL,
                strstr(v, "-backup-") != NULL,
                raw_val);
            printf("{\"op\":\"backup_mode\",\"args\":");
            emit_json_string(v);
            printf(",\"result\":");
            emit_json_string(mode);
            printf("}\n");

        } else if (strcmp(op, "is_android") == 0) {
            bool r = uc_is_android_python();
            printf("{\"op\":\"is_android\",\"result\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "npm_bin_exists") == 0) {
            /* args = 'bin_dir|name' — we test against real filesystem */
            char *dir = strdup(v);
            char *pipe = strchr(dir, '|');
            char *name = "";
            if (pipe) { *pipe = '\0'; name = pipe + 1; }
            bool r = uc_npm_bin_exists(dir, name);
            printf("{\"op\":\"npm_bin_exists\",\"args\":");
            emit_json_string(v);
            printf(",\"result\":%s}\n", r ? "true" : "false");
            free(dir);

        } else if (strcmp(op, "web_toolchain_roots") == 0) {
            char **roots = uc_web_toolchain_roots(v[0] ? v : "");
            printf("{\"op\":\"web_toolchain_roots\",\"args\":");
            emit_json_string(v);
            printf(",\"result\":[");
            if (roots) {
                for (size_t i = 0; roots[i]; i++) {
                    if (i > 0) printf(",");
                    emit_json_string(roots[i]);
                }
            }
            printf("]}\n");
            uc_free_string_array(roots);

        } else if (strcmp(op, "web_toolchain_ready") == 0) {
            /* args = 'root1|root2|...' */
            char *copy = strdup(v);
            char **toks = calloc(16, sizeof(char *));
            size_t n = 0;
            char *save = NULL;
            char *tok = strtok_r(copy, "|", &save);
            while (tok) {
                toks[n++] = strdup(tok);
                tok = strtok_r(NULL, "|", &save);
            }
            bool r = uc_web_build_toolchain_ready(n > 0 ? (const char **)toks : NULL);
            printf("{\"op\":\"web_toolchain_ready\",\"args\":");
            emit_json_string(v);
            printf(",\"result\":%s}\n", r ? "true" : "false");
            for (size_t i = 0; i < n; i++) free(toks[i]);
            free(toks);
            free(copy);

        } else if (strcmp(op, "venv_holders") == 0) {
            /* args = 'pid1|name1|cmdline1;pid2|name2|cmdline2;...' */
            char *copy = strdup(v);
            const char **matches = calloc(16, sizeof(char *));
            size_t n = 0;
            char *save = NULL;
            char *entry = strtok_r(copy, ";", &save);
            while (entry) {
                if (n < 15) matches[n++] = entry;
                entry = strtok_r(NULL, ";", &save);
            }
            char *msg = uc_format_venv_python_holders_message(matches, n);
            printf("{\"op\":\"venv_holders\",\"args\":");
            emit_json_string(v);
            printf(",\"message\":");
            emit_json_string(msg);
            printf("}\n");
            free(msg);
            free(matches);
            free(copy);

        } else if (strcmp(op, "numstat_paths") == 0) {
            /* args = raw numstat output with \\t and \\n escape tokens */
            char *decoded = decode_escapes(v);
            char **paths = uc_parse_numstat_paths(decoded ? decoded : v);
            printf("{\"op\":\"numstat_paths\",\"args\":");
            emit_json_string(v);
            printf(",\"paths\":[");
            if (paths) {
                for (size_t i = 0; paths[i]; i++) {
                    if (i > 0) printf(",");
                    emit_json_string(paths[i]);
                }
            }
            printf("]}\n");
            uc_free_string_array(paths);
            free(decoded);

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
    }
    return 0;
}
