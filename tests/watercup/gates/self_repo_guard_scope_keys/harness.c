/* gate: self_repo_guard_scope_keys — traced copy of
 * src/tools/self_repo_guard.c::srg_scope_keys
 * (PoP: tools/self_repo_guard.py::_scope_keys).
 *
 * Input encoding (argv-safe): "<start_csv>|<command>"
 *   e.g.  "0,9|echo 'hi' && cd /tmp"
 * starts are sorted+deduped before tracing (mirrors both impls' callers).
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   per start:  B(cursor<start) per while check ... inner char branches ...
 *   V(scope tuple as comma-joined openers)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

#define SRG_MAX_CONTEXT 32

/* Faithful traced copy of srg_scope_keys body. */
static void trace_scope_keys(const char *command, const size_t *starts, size_t nstarts) {
    WCUP_BEGIN("srg_scope_keys");
    struct { int kind; int opener; char quote; } contexts[SRG_MAX_CONTEXT];
    int nctx = 1;
    contexts[0].kind = 0; contexts[0].opener = -1; contexts[0].quote = 0;
    size_t cursor = 0;

    for (size_t si = 0; si < nstarts; si++) {
        size_t start = starts[si];
        for (;;) {
            int have = cursor < start;
            WCUP_BRANCH(have);
            if (!have) break;
            int ctx = nctx - 1;
            char quote = contexts[ctx].quote;
            char c = command[cursor];
            if (quote == '\'') {                                   /* B in-squote */
                WCUP_BRANCH(1);
                WCUP_BRANCH(c == '\'');                            /* B close */
                if (c == '\'') contexts[ctx].quote = 0;
                cursor++;
                continue;
            }
            WCUP_BRANCH(0);
            if (quote == '"') {                                    /* B in-dquote */
                WCUP_BRANCH(1);
                WCUP_BRANCH(c == '\\' && cursor + 1 < start);      /* B esc */
                if (c == '\\' && cursor + 1 < start) { cursor += 2; continue; }
                WCUP_BRANCH(c == '"');                             /* B close */
                if (c == '"') { contexts[ctx].quote = 0; cursor++; continue; }
                WCUP_BRANCH(strncmp(command + cursor, "$(", 2) == 0); /* B cmdsub */
                if (strncmp(command + cursor, "$(", 2) == 0) {
                    if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 2; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                    cursor += 2; continue;
                }
                WCUP_BRANCH(c == '`');                             /* B backtick */
                if (c == '`') {
                    if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 3; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                }
                cursor++;
                continue;
            }
            WCUP_BRANCH(0);
            WCUP_BRANCH(c == '\'' || c == '"');                    /* B open-quote */
            if (c == '\'' || c == '"') { contexts[ctx].quote = c; cursor++; continue; }
            WCUP_BRANCH(c == '\\' && cursor + 1 < start);          /* B esc */
            if (c == '\\' && cursor + 1 < start) { cursor += 2; continue; }
            WCUP_BRANCH(strncmp(command + cursor, "$(", 2) == 0);  /* B cmdsub */
            if (strncmp(command + cursor, "$(", 2) == 0) {
                if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 2; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                cursor += 2; continue;
            }
            WCUP_BRANCH(c == '(');                                 /* B paren */
            if (c == '(') {
                if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 1; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
                cursor++; continue;
            }
            WCUP_BRANCH(c == ')' && nctx > 1 && (contexts[nctx-1].kind == 1 || contexts[nctx-1].kind == 2)); /* B close-paren */
            if (c == ')' && nctx > 1 && (contexts[nctx-1].kind == 1 || contexts[nctx-1].kind == 2)) {
                nctx--; cursor++; continue;
            }
            WCUP_BRANCH(c == '`');                                 /* B backtick */
            if (c == '`') {
                WCUP_BRANCH(nctx > 1 && contexts[nctx-1].kind == 3); /* B close-backtick */
                if (nctx > 1 && contexts[nctx-1].kind == 3) nctx--;
                else if (nctx < SRG_MAX_CONTEXT) { contexts[nctx].kind = 3; contexts[nctx].opener = (int)cursor; contexts[nctx].quote = 0; nctx++; }
            }
            cursor++;
        }
        /* record openers of contexts[1:] as comma-joined string */
        char sbuf[256]; int off = 0;
        size_t cnt = (size_t)(nctx - 1);
        for (size_t i = 0; i < cnt && i < SRG_MAX_CONTEXT; i++) {
            if (off) sbuf[off++] = ',';
            off += snprintf(sbuf + off, sizeof sbuf - off, "%d", contexts[i+1].opener);
        }
        sbuf[off] = '\0';
        WCUP_VALSTR(sbuf);
    }
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("srg_scope_keys");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s srg_scope_keys <start_csv|command>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[1024];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strchr(probe, '|');
        if (!bar) { fprintf(stderr, "bad probe (need start_csv|command): %s\n", probe); return 2; }
        *bar = '\0';
        const char *command = bar + 1;
        size_t starts[64]; size_t nstarts = 0;
        char *tok = strtok(probe, ",");
        while (tok && nstarts < 64) { starts[nstarts++] = (size_t)strtoul(tok, NULL, 10); tok = strtok(NULL, ","); }
        /* sort + dedupe */
        for (size_t a = 1; a < nstarts; a++) {
            size_t v = starts[a]; size_t b = a;
            while (b > 0 && starts[b-1] > v) { starts[b] = starts[b-1]; b--; }
            starts[b] = v;
        }
        size_t uniq = 0;
        for (size_t a = 0; a < nstarts; a++)
            if (uniq == 0 || starts[a] != starts[uniq-1]) starts[uniq++] = starts[a];
        trace_scope_keys(command, starts, uniq);
    }
    return 0;
}
