/* gate: memory_tool_apply_batch — traced copy of
 * src/tools/memory_tool.c::memory_store_apply_batch
 * (LIVE twin: tools/memory_tool.py::MemoryStore.apply_batch).
 *
 * The C port runs on an in-memory store whose threat scanner is a function
 * pointer; this harness seeds entries/limit from argv and uses scanner=NULL
 * (fail-open), which the port explicitly supports. All decision rungs of the
 * port's ladder are reproduced in the same order:
 *
 *   B0: operations list empty?
 *   scan pre-pass (L tick per op):
 *     B1: act in {add,replace} and content nonempty?
 *     B2: scan blocked?            (scanner=NULL => always clean here)
 *   apply loop (L tick per op):
 *     B3: act missing?
 *     B4: act == "add"?      B5: content empty?   B6: duplicate (skip)?
 *     B7: act == "replace"?  B8: old_text empty?  B9: content empty?
 *         match scan (L tick per entry, B10: substring hit?)
 *         B11: no matches?   B12: multiple distinct matches?
 *     B13: act == "remove"?  B14: old_text empty?
 *         match scan (L tick per entry, B15: substring hit?)
 *         B16: no matches?   B17: multiple distinct matches?
 *     (else: unknown action -> error)
 *   B18: final size over budget?  (consolidation failure #42405)
 *
 * Success => V: committed entry list joined with '|'-free separator ";".
 * Failure => V: "ERR".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

#define MAXE 64
#define MAXO 16

typedef struct {
    const char *action;
    const char *content;
    const char *old_text;
} op_t;

/* split s by sep (2-char); returns count, fills out */
static int split2(const char *s, const char *sep, char out[][256], int max) {
    int n = 0;
    const char *p = s;
    size_t sl = strlen(sep);
    while (n < max) {
        const char *hit = strstr(p, sep);
        if (!hit) { snprintf(out[n++], 256, "%s", p); break; }
        size_t len = (size_t)(hit - p);
        if (len > 255) len = 255;
        memcpy(out[n], p, len); out[n][len] = '\0'; n++;
        p = hit + sl;
    }
    return n;
}

static void trace_apply_batch(const char *scenario) {
    WCUP_BEGIN("apply_batch");
    /* ---- parse scenario: target|limit|e1;;e2||op~content~old||op... ---- */
    char parts[24][512];
    int nparts = 0;
    {
        const char *p = scenario;
        const char *hit;
        while ((hit = strstr(p, "||")) && nparts < 23) {
            size_t len = (size_t)(hit - p);
            if (len > 511) len = 511;
            memcpy(parts[nparts], p, len); parts[nparts][len] = '\0'; nparts++;
            p = hit + 2;
        }
        snprintf(parts[nparts++], 512, "%s", p);
    }
    char hdr[512];
    snprintf(hdr, sizeof hdr, "%s", parts[0]);
    char hdrp[3][256];
    int nh = split2(hdr, "|", hdrp, 3);
    const char *target = hdrp[0];
    int limit = atoi(nh > 1 ? hdrp[1] : "2200");
    char entries[MAXE][256];
    int en = 0;
    if (nh > 2 && hdrp[2][0]) en = split2(hdrp[2], ";;", entries, MAXE);

    char *working[MAXE];
    int wn = 0;
    const char *result_v = "ERR";
    bool committed = false;

    int opn = nparts - 1;
    op_t ops[MAXO];
    static char opbuf[MAXO][3][256];
    for (int i = 0; i < opn && i < MAXO; i++) {
        char f[3][256];
        split2(parts[i + 1], "~", f, 3);
        for (int k = 0; k < 3; k++) memcpy(opbuf[i][k], f[k], 256);
        ops[i].action = opbuf[i][0][0] ? opbuf[i][0] : NULL;
        ops[i].content = opbuf[i][1];
        ops[i].old_text = opbuf[i][2];
    }

    if (opn == 0) { WCUP_BRANCH(1); goto out; }          /* B0: empty ops */
    WCUP_BRANCH(0);

    {   /* scan pre-pass */
        for (int i = 0; i < opn; i++) {
            WCUP_LOOP_TICK();
            const char *act = ops[i].action;
            const char *content = ops[i].content;
            int addrepl = act && (strcmp(act, "add") == 0 || strcmp(act, "replace") == 0) && content && *content;
            WCUP_BRANCH(addrepl);                        /* B1 */
            if (addrepl) {
                int blocked = 0;                          /* scanner=NULL fail-open */
                WCUP_BRANCH(blocked);                     /* B2 */
                if (blocked) goto out;
            }
        }
    }
    {   /* work on a copy */
        for (int i = 0; i < en; i++) working[wn++] = strdup(entries[i]);
        for (int i = 0; i < opn; i++) {
            WCUP_LOOP_TICK();
            const char *act = ops[i].action;
            const char *content = ops[i].content;
            const char *old_text = ops[i].old_text;
            WCUP_BRANCH(!act);                           /* B3 */
            if (!act) goto out;
            if (strcmp(act, "add") == 0) {               /* B4 */
                WCUP_BRANCH(1);
                int empty = !content || !*content;
                WCUP_BRANCH(empty);                      /* B5 */
                if (empty) goto out;
                int dup = 0;
                for (int j = 0; j < wn; j++)
                    if (strcmp(working[j], content) == 0) { dup = 1; break; }
                WCUP_BRANCH(dup);                        /* B6 */
                if (!dup) working[wn++] = strdup(content);
            } else {
                WCUP_BRANCH(0);
                if (strcmp(act, "replace") == 0) {       /* B7 */
                    WCUP_BRANCH(1);
                    int e1 = !old_text || !*old_text;
                    WCUP_BRANCH(e1);                     /* B8 */
                    if (e1) goto out;
                    int e2 = !content || !*content;
                    WCUP_BRANCH(e2);                     /* B9 */
                    if (e2) goto out;
                    int idx[MAXE], m = 0;
                    for (int j = 0; j < wn; j++) {
                        WCUP_LOOP_TICK();
                        int hit = strstr(working[j], old_text) != NULL;
                        WCUP_BRANCH(hit);                /* B10 */
                        if (hit) idx[m++] = j;
                    }
                    WCUP_BRANCH(m == 0);                 /* B11 */
                    if (m == 0) goto out;
                    int distinct = 0;
                    for (int j = 1; j < m; j++)
                        if (strcmp(working[idx[j]], working[idx[0]]) != 0) distinct++;
                    WCUP_BRANCH(m > 1 && distinct);      /* B12 */
                    if (m > 1 && distinct) goto out;
                    free(working[idx[0]]);
                    working[idx[0]] = strdup(content);
                } else {
                    WCUP_BRANCH(0);
                    if (strcmp(act, "remove") == 0) {    /* B13 */
                        WCUP_BRANCH(1);
                        int e1 = !old_text || !*old_text;
                        WCUP_BRANCH(e1);                 /* B14 */
                        if (e1) goto out;
                        int idx[MAXE], m = 0;
                        for (int j = 0; j < wn; j++) {
                            WCUP_LOOP_TICK();
                            int hit = strstr(working[j], old_text) != NULL;
                            WCUP_BRANCH(hit);            /* B15 */
                            if (hit) idx[m++] = j;
                        }
                        WCUP_BRANCH(m == 0);             /* B16 */
                        if (m == 0) goto out;
                        int distinct = 0;
                        for (int j = 1; j < m; j++)
                            if (strcmp(working[idx[j]], working[idx[0]]) != 0) distinct++;
                        WCUP_BRANCH(m > 1 && distinct);  /* B17 */
                        if (m > 1 && distinct) goto out;
                        free(working[idx[0]]);
                        for (int j = idx[0]; j < wn - 1; j++) working[j] = working[j + 1];
                        wn--;
                    } else {
                        WCUP_BRANCH(0);                  /* unknown action */
                        goto out;
                    }
                }
            }
        }
        /* budget check against FINAL state: sum(len) + delim*(n-1), delim="\n§\n" (3 bytes) */
        int new_total = 0;
        for (int j = 0; j < wn; j++) new_total += (int)strlen(working[j]);
        if (wn > 1) new_total += 3 * (wn - 1);
        int over = new_total > limit;
        WCUP_BRANCH(over);                               /* B18 */
        if (over) goto out;                              /* consolidation failure */
        committed = true;
    }
out:
    if (committed) {
        static char joined[4096];
        joined[0] = '\0';
        for (int j = 0; j < wn; j++) {
            if (j) strcat(joined, ";");
            strcat(joined, working[j]);
        }
        result_v = joined;
    }
    WCUP_VALSTR(target ? target : "");
    WCUP_VALSTR(result_v);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apply_batch");
    for (int j = 0; j < wn; j++) free(working[j]);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apply_batch <scenario>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_apply_batch(argv[i]);
    return 0;
}
