/*
 * t_port_working_diff.c — oracle harness for src/tools/port_working_diff.c
 * (port of tools/working_diff.py).
 *
 * Reads the fixture from argv[1] (one case per line), substitutes the
 * literal token "REPO" with the fixture directory (argv[1]'s dirname),
 * runs wd_collect_working_diff, and emits one JSON object per line with
 * success/error/empty/stat/diff/untracked — mirroring the Python oracle's
 * output shape so the runner diff is byte-identical.
 */
#include "tools/port_working_diff.h"
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

/* Substitute leading "REPO" token with fixture repo dir; else return input copy.
 * The runner feeds a temp copy of cases.in, so the real fixture dir comes
 * from the ORACLE_FIXDIR env (exported by run_oracle.sh). */
static char *subst_repo(const char *line, const char *fixture_dir) {
    if (strncmp(line, "REPO", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
        size_t need = strlen(fixture_dir) + strlen(line) + 16;
        char *out = malloc(need);
        if (!out) return NULL;
        snprintf(out, need, "%s/repo%s", fixture_dir, line + 4);
        return out;
    }
    return strdup(line);
}

/* Emit a JSON string, escaping control chars / quotes / backslashes. */
static void emit_json_string(const char *s) {
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c == '\n') fputs("\\n", stdout);
        else if (c == '\r') fputs("\\r", stdout);
        else if (c == '\t') fputs("\\t", stdout);
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.in>\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    /* fixture dir = ORACLE_FIXDIR (runner export) else dirname of cases.in */
    char fixture_dir[1024];
    const char *ofd = getenv("ORACLE_FIXDIR");
    if (ofd && *ofd) {
        snprintf(fixture_dir, sizeof(fixture_dir), "%s", ofd);
    } else {
        snprintf(fixture_dir, sizeof(fixture_dir), "%s", argv[1]);
        char *slash = strrchr(fixture_dir, '/');
        if (slash) *slash = '\0';
        else strcpy(fixture_dir, ".");
    }

    char *line;
    while ((line = read_line_alloc(fp)) != NULL) {
        if (!*line || line[0] == '#') { free(line); continue; }
        char *sub = subst_repo(line, fixture_dir);
        free(line);
        if (!sub) continue;

        char *save = NULL;
        char *cwd = strtok_r(sub, " ", &save);
        char *mode = strtok_r(NULL, " ", &save);
        char *paths[8];
        int path_count = 0;
        char *tok;
        while ((tok = strtok_r(NULL, " ", &save)) != NULL && path_count < 8) {
            if (strcmp(tok, "--stat") == 0) continue; /* stat flag: same result shape */
            paths[path_count++] = tok;
        }

        wd_diff_result_t r = wd_collect_working_diff(cwd, mode, paths, path_count);

        printf("{\"success\":%s,", r.success ? "true" : "false");
        if (!r.success) {
            printf("\"error\":");
            emit_json_string(r.error ? r.error : "");
            printf(",\"empty\":false,\"stat\":\"\",\"diff\":\"\",\"untracked\":[]}\n");
            wd_diff_result_free(&r);
            free(sub);
            continue;
        }
        printf("\"error\":null,\"empty\":%s,\"stat\":", r.empty ? "true" : "false");
        emit_json_string(r.stat ? r.stat : "");
        printf(",\"diff\":");
        emit_json_string(r.diff ? r.diff : "");
        printf(",\"untracked\":[");
        for (int i = 0; i < r.untracked_count; i++) {
            if (i) putchar(',');
            emit_json_string(r.untracked[i]);
        }
        printf("]}\n");
        wd_diff_result_free(&r);
        free(sub);
    }
    fclose(fp);
    return 0;
}
