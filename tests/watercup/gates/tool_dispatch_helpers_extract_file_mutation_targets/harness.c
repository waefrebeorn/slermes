#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/agent/tool_dispatch_helpers.c::extract_file_mutation_targets
 * (PoP: agent/tool_dispatch_helpers.py:_extract_file_mutation_targets).
 * Input encoding: tool\x1fmode\x1fpath\x1fpatch  ('-' = absent).
 * Value: targets joined '|' (empty string = no targets). */
static const char *F(const char *s) { return strcmp(s, "-") ? s : NULL; }

static void scan_headers(wcup_t *_wc, const char *patch, const char *kw1, const char *kw2, int move, char *out, size_t outsz) {
    /* line-based scan mirroring the C regexes:
     *   ^\*\*\*[ \t]+(Update|Add|Delete)[ \t]+File:[ \t]*(.+)$
     *   ^\*\*\*[ \t]+Move[ \t]+File:[ \t]*(.+?)[ \t]*->[ \t]*(.+)$ */
    const char *p = patch;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len > 3 && strncmp(p, "***", 3) == 0 &&
            (p[3] == ' ' || p[3] == '\t')) {
            char line[2048]; size_t l = len < sizeof line - 1 ? len : sizeof line - 1;
            memcpy(line, p, l); line[l] = '\0';
            char *q = line + 3;
            while (*q == ' ' || *q == '\t') q++;
            size_t kwl = strlen(kw1);
            if (strncmp(q, kw1, kwl) == 0) q += kwl; else if (kw2 && strncmp(q, kw2, strlen(kw2)) == 0) q += strlen(kw2); else goto next;
            while (*q == ' ' || *q == '\t') q++;
            if (strncmp(q, "File:", 5) != 0) goto next;
            q += 5;
            while (*q == ' ' || *q == '\t') q++;
            char *targets[2]; int nt = 0;
            if (!move) {
                char *e = line + l; while (e > q && (e[-1]==' '||e[-1]=='\t')) e--; *e = '\0';
                targets[nt++] = q;
            } else {
                char *arrow = strstr(q, "->");
                if (!arrow) goto next;
                char *se = arrow; while (se > q && (se[-1]==' '||se[-1]=='\t')) se--; *se = '\0';
                char *d = arrow + 2; while (*d == ' ' || *d == '\t') d++;
                char *de = line + l; while (de > d && (de[-1]==' '||de[-1]=='\t')) de--; *de = '\0';
                targets[nt++] = q; targets[nt++] = d;
            }
            for (int i = 0; i < nt; i++) {
                wcup_branch(_wc, targets[i][0] ? 1 : 0);
                if (targets[i][0]) {
                    if (out[0]) strncat(out, "|", outsz - strlen(out) - 1);
                    strncat(out, targets[i], outsz - strlen(out) - 1);
                }
            }
        }
next:
        if (!eol) break;
        p = eol + 1;
    }
}

static void run_one(const char *tool, const char *mode, const char *path, const char *patch) {
    char out[4096]; out[0] = '\0';
    WCUP_BEGIN("extract_file_mutation_targets");
    int mutating = tool && (strcmp(tool, "write_file") == 0 || strcmp(tool, "patch") == 0);
    if (!mutating) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        if (strcmp(tool, "write_file") == 0) {
            WCUP_BRANCH(1);
            WCUP_BRANCH(path && path[0] ? 1 : 0);
            if (path && path[0]) strcpy(out, path);
        } else {
            WCUP_BRANCH(0);
            const char *m = mode ? mode : "replace";
            if (strcmp(m, "replace") == 0) {
                WCUP_BRANCH(1);
                WCUP_BRANCH(path && path[0] ? 1 : 0);
                if (path && path[0]) strcpy(out, path);
            } else if (strcmp(m, "patch") == 0) {
                WCUP_BRANCH(1);
                WCUP_BRANCH(patch && patch[0] ? 1 : 0);
                if (patch && patch[0]) {
                    WCUP_LOOP_TICK();
                    scan_headers(&_wc, patch, "Update", NULL, 0, out, sizeof out);
                    scan_headers(&_wc, patch, "Add", "Delete", 0, out, sizeof out);
                    scan_headers(&_wc, patch, "Move", NULL, 1, out, sizeof out);
                }
            } else WCUP_BRANCH(0);
        }
    }
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("extract_file_mutation_targets");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract_file_mutation_targets <tool\\x1fmode\\x1fpath\\x1fpatch>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *f1 = strchr(s, '\x1f');   if (f1) { *f1 = 0; }
        char *f2 = f1 ? strchr(f1+1, '\x1f') : NULL; if (f2) { *f2 = 0; }
        char *f3 = f2 ? strchr(f2+1, '\x1f') : NULL; if (f3) { *f3 = 0; }
        #define FLD(x) ((x) && strcmp((x), "-") == 0 ? NULL : (x))
        run_one(s, FLD(f1 ? f1+1 : NULL), FLD(f2 ? f2+1 : NULL), FLD(f3 ? f3+1 : NULL));
        free(s);
    }
    return 0;
}
