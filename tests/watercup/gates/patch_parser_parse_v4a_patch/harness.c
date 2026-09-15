/* gate: patch_parser_parse_v4a_patch — traced copy of
 * slermes src/tools/patch_parser.c::patch_parser_parse_v4a
 * (faithful to that port's exact control flow; the port itself is a
 * self-contained .c that we could link, but the trace must sit INSIDE the
 * branch ladder, so we mirror it line-for-line here — see BATCH_CUPS.md
 * "faithful traced copy" clause; logic identical to the linked port).
 *
 * LIVE twin: tools/patch_parser.py::parse_v4a_patch.
 *
 * Rung contract (identical order both sides):
 *   B0 begin-marker found / V start_idx / B1 end-marker found / V end_idx
 *   per body line: L tick
 *     B2 update?  [B closecond] V "U|path"
 *     B add?      [B closecond] V "A|path"
 *     B delete?   [B closecond] V "D|path" [B closecond2]
 *     B move?     [B closecond] V "M|src|dst" [B closecond2]
 *     B @@?       [B finalizecond] [B hintfound] V hint|""
 *     B content(cur&&nonempty): B '+' B '-' B ' ' B '\' else implicit; V pre+content
 *   B last-op-open? [B closecond]
 *   B ops-empty? V "EMPTY"
 *   validation loop: L tick, B emptypath, B update-no-hunks, B move-no-dst
 *   B error? V errstr | V "OK" V n_ops
 *
 * Known divergences (documented, avoided in inputs): C finds markers via
 * strstr anywhere in the line (Python: column-0 full-line regex); C's '@@'
 * branch runs without a current op (Python ignores); whitespace-only file
 * paths. Inputs stay inside the agreed subset.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static char *xstrndup(const char *s, size_t n) {
    char *p = malloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}

/* mirror of match_file_marker in src/tools/patch_parser.c */
static char *match_file_marker(const char *line, const char *opkw) {
    if (strncmp(line, "***", 3) != 0) return NULL;
    const char *p = line + 3;
    while (*p == ' ') p++;
    size_t kwl = strlen(opkw);
    if (strncmp(p, opkw, kwl) != 0) return NULL;
    p += kwl;
    while (*p == ' ') p++;
    if (strncmp(p, "File:", 5) != 0) return NULL;
    p += 5;
    while (*p == ' ') p++;
    char *rest = strdup(p);
    return strdup(trim(rest));
}

/* mirror of match_move_marker */
static int match_move_marker(const char *line, char **src, char **dst) {
    if (strncmp(line, "***", 3) != 0) return 0;
    const char *p = line + 3;
    while (*p == ' ') p++;
    if (strncmp(p, "Move", 4) != 0) return 0;
    p += 4;
    while (*p == ' ') p++;
    if (strncmp(p, "File:", 5) != 0) return 0;
    p += 5;
    while (*p == ' ') p++;
    const char *arrow = strstr(p, "->");
    if (!arrow) return 0;
    size_t srclen = (size_t)(arrow - p);
    while (srclen > 0 && (p[srclen-1] == ' ' || p[srclen-1] == '\t')) srclen--;
    *src = xstrndup(p, srclen);
    const char *d = arrow + 2;
    while (*d == ' ' || *d == '\t') d++;
    char *d2 = strdup(d);
    *dst = strdup(trim(d2));
    free(d2);
    return 1;
}

/* traced copy of patch_parser_parse_v4a */
static void parse_v4a_patch(const char *patch_content) {
    WCUP_BEGIN("parse_v4a_patch");

    size_t len = strlen(patch_content);
    size_t nlines = 1;
    for (size_t i = 0; i < len; i++) if (patch_content[i] == '\n') nlines++;
    char **lines = malloc(nlines * sizeof(char *));
    size_t li = 0;
    const char *start = patch_content;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || patch_content[i] == '\n') {
            size_t l = (size_t)(&patch_content[i] - start);
            char *cpy = xstrndup(start, l);
            if (l > 0 && cpy[l-1] == '\r') cpy[l-1] = '\0';
            lines[li++] = cpy;
            start = patch_content + i + 1;
        }
    }
    nlines = li;

    long start_idx = -2;
    long end_idx = (long)nlines;
    int end_found = 0;
    for (size_t i = 0; i < nlines; i++) {
        if (strstr(lines[i], "*** Begin Patch") || strstr(lines[i], "***Begin Patch")) {
            start_idx = (long)i;
        } else if (strstr(lines[i], "*** End Patch") || strstr(lines[i], "***End Patch")) {
            end_idx = (long)i;
            end_found = 1;
            break;
        }
    }
    WCUP_BRANCH(start_idx != -2);                       /* B0 */
    if (start_idx == -2) start_idx = -1;
    char idxbuf[32];
    snprintf(idxbuf, sizeof idxbuf, "%ld", start_idx);
    WCUP_VALSTR(idxbuf);                                 /* V start_idx */
    WCUP_BRANCH(end_found);                              /* B1 */
    snprintf(idxbuf, sizeof idxbuf, "%ld", end_idx);
    WCUP_VALSTR(idxbuf);                                 /* V end_idx */

    /* op/hunk state */
    #define MAXOPS 16
    typedef struct { char path[256]; char new_path[256]; int type; int n_hunks; } op_t;
    op_t ops[MAXOPS]; int n_ops = 0;
    int cur = -1;                 /* index of open op, -1 = none */
    int cur_hunk_has_hint = 0;
    int cur_hunk_nlines = 0;

    for (long i = start_idx + 1; i < end_idx; i++) {
        WCUP_LOOP_TICK();
        char *line = lines[i];
        char *upd = match_file_marker(line, "Update");
        char *add = match_file_marker(line, "Add");
        char *del = match_file_marker(line, "Delete");
        char *mv_src = NULL, *mv_dst = NULL;
        int is_move = match_move_marker(line, &mv_src, &mv_dst);

        if (upd) {
            WCUP_BRANCH(1);                                          /* B2 */
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);            /* closecond */
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur = n_ops++;
            strcpy(ops[cur].path, upd); ops[cur].new_path[0] = '\0';
            ops[cur].type = 1; ops[cur].n_hunks = 0;
            cur_hunk_nlines = 0; cur_hunk_has_hint = 0;
            char v[300]; snprintf(v, sizeof v, "U|%s", upd); WCUP_VALSTR(v);
        } else if (add) {
            WCUP_BRANCH(0); WCUP_BRANCH(1);
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur = n_ops++;
            strcpy(ops[cur].path, add); ops[cur].new_path[0] = '\0';
            ops[cur].type = 0; ops[cur].n_hunks = 0;
            cur_hunk_nlines = 0; cur_hunk_has_hint = 0;
            char v[300]; snprintf(v, sizeof v, "A|%s", add); WCUP_VALSTR(v);
        } else if (del) {
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur = n_ops++;
            strcpy(ops[cur].path, del); ops[cur].new_path[0] = '\0';
            ops[cur].type = 2; ops[cur].n_hunks = 0;
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);            /* closecond2 */
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur = -1; cur_hunk_nlines = 0; cur_hunk_has_hint = 0;
            char v[300]; snprintf(v, sizeof v, "D|%s", del); WCUP_VALSTR(v);
        } else if (is_move) {
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur = n_ops++;
            strcpy(ops[cur].path, mv_src); strcpy(ops[cur].new_path, mv_dst);
            ops[cur].type = 3; ops[cur].n_hunks = 0;
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur = -1; cur_hunk_nlines = 0; cur_hunk_has_hint = 0;
            char v[600]; snprintf(v, sizeof v, "M|%s|%s", mv_src, mv_dst); WCUP_VALSTR(v);
        } else if (line[0] == '@' && line[1] == '@') {
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
            WCUP_BRANCH(1);
            WCUP_BRANCH(cur >= 0 && cur_hunk_nlines > 0);            /* finalizecond */
            if (cur >= 0 && cur_hunk_nlines > 0) { ops[cur].n_hunks++; cur_hunk_nlines = 0; }
            cur_hunk_nlines = 0;
            char *a = (char *)line + 2;
            while (*a == ' ') a++;
            char *b = strstr(a, "@@");
            WCUP_BRANCH(b != NULL);                                  /* hintfound */
            char hint[256] = "";
            if (b) {
                size_t hl = (size_t)(b - a);
                while (hl > 0 && (a[hl-1] == ' ' || a[hl-1] == '\t')) hl--;
                memcpy(hint, a, hl); hint[hl] = '\0';
            }
            cur_hunk_has_hint = 1;
            WCUP_VALSTR(hint);
        } else if (cur >= 0 && line[0] != '\0') {
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
            WCUP_BRANCH(0); WCUP_BRANCH(1);
            if (line[0] == '+') {
                WCUP_BRANCH(1);
                char v[300]; snprintf(v, sizeof v, "+%s", line + 1); WCUP_VALSTR(v);
                cur_hunk_nlines++;
            } else if (line[0] == '-') {
                WCUP_BRANCH(0); WCUP_BRANCH(1);
                char v[300]; snprintf(v, sizeof v, "-%s", line + 1); WCUP_VALSTR(v);
                cur_hunk_nlines++;
            } else if (line[0] == ' ') {
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
                char v[300]; snprintf(v, sizeof v, " %s", line + 1); WCUP_VALSTR(v);
                cur_hunk_nlines++;
            } else if (line[0] == '\\') {
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
                /* skip */
            } else {
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
                char v[300]; snprintf(v, sizeof v, " %s", line); WCUP_VALSTR(v);
                cur_hunk_nlines++;
            }
        } else {
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
            WCUP_BRANCH(0); WCUP_BRANCH(0);
        }
        free(upd); free(add); free(del); free(mv_src); free(mv_dst);
    }

    WCUP_BRANCH(cur >= 0);                                           /* last op open */
    if (cur >= 0) {
        WCUP_BRANCH(cur_hunk_nlines > 0);
        if (cur_hunk_nlines > 0) ops[cur].n_hunks++;
    }
    WCUP_BRANCH(n_ops == 0);                                         /* ops empty */
    if (n_ops == 0) { WCUP_VALSTR("EMPTY"); }
    else {
        int has_err = 0;
        char errbuf[2048] = "";
        for (int i = 0; i < n_ops; i++) {
            WCUP_LOOP_TICK();
            WCUP_BRANCH(ops[i].path[0] == '\0');
            if (ops[i].path[0] == '\0') { has_err = 1; strcat(errbuf, "Operation with empty file path; "); }
            WCUP_BRANCH(ops[i].type == 1 && ops[i].n_hunks == 0);
            if (ops[i].type == 1 && ops[i].n_hunks == 0) {
                has_err = 1;
                char tmp[400]; snprintf(tmp, sizeof tmp, "UPDATE '%s': no hunks found; ", ops[i].path);
                strcat(errbuf, tmp);
            }
            WCUP_BRANCH(ops[i].type == 3 && ops[i].new_path[0] == '\0');
            if (ops[i].type == 3 && ops[i].new_path[0] == '\0') {
                has_err = 1;
                char tmp[500]; snprintf(tmp, sizeof tmp, "MOVE '%s': missing destination path (expected 'src -> dst'); ", ops[i].path);
                strcat(errbuf, tmp);
            }
        }
        WCUP_BRANCH(has_err);
        if (has_err) {
            size_t el = strlen(errbuf);
            if (el >= 2) errbuf[el-2] = '\0';
            char v[2200]; snprintf(v, sizeof v, "Parse error: %s", errbuf);
            WCUP_VALSTR(v);
        } else {
            char nb[16]; snprintf(nb, sizeof nb, "%d", n_ops);
            WCUP_VALSTR("OK");
            WCUP_VALSTR(nb);
        }
    }

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_v4a_patch");

    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_v4a_patch <patch>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) parse_v4a_patch(argv[i]);
    return 0;
}
