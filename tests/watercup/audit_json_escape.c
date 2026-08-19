/* tests/watercup/audit_json_escape.c — language-agnostic water-cup audit for
 * the JSON-escape helper, across ALL three translations of the same logic:
 *
 *   1. C11 copy A: src/web_server.c :: json_escape_append   (control chars -> \u00XX)
 *   2. C11 copy B: src/web_app.c     :: web_json_escape      (NO control-char \u fallback)
 *   3. LIVE Python: hermes_cli uses json.dumps (stdlib) which emits \uXXXX
 *                   for control chars < 0x20 — the authoritative behavior.
 *
 * The water-cup is language-agnostic: the cup is a byte stream of branch/loop/
 * value events folded with FNV-1a. Any source language translated to C11 carves
 * the same shape when the logic is identical; if two translations diverge, the
 * cups mismatch — and you cannot fake it with a constant return.
 *
 * This harness traces BOTH C11 copies (A and B) over a stream of inputs and
 * prints each cup, so we can diff copy A vs copy B vs the LIVE-Python cup.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

/* ---- C11 copy A: faithful to src/web_server.c::json_escape_append --------- */
static int json_escape_append_A(const char *src) {
    WCUP_BEGIN("json_escape");
    WCUP_VALSTR(src ? src : "(null)");
    int written = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p; p++) {
        WCUP_LOOP_TICK();
        if (*p == '"' || *p == '\\') { WCUP_BRANCH(1); WCUP_VALSTR("qb"); written += 2; }
        else if (*p == '\n')         { WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("nl"); written += 2; }
        else if (*p == '\t')         { WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("tab"); written += 2; }
        else if (*p < 0x20)          { WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("ctl"); written += 6; }
        else                         { WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_VALSTR("lit"); written++; }
    }
    WCUP_VALSTR("done");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("json_escape");
    return written;
}

/* ---- C11 copy B: faithful to src/web_app.c::web_json_escape -------------- */
/* Divergence: no <0x20 control-char -> \u00XX branch. The cup will differ. */
static int json_escape_append_B(const char *src) {
    WCUP_BEGIN("json_escape");
    WCUP_VALSTR(src ? src : "(null)");
    int i = 0;
    while (*src) {
        WCUP_LOOP_TICK();
        switch (*src) {
            case '"':  WCUP_BRANCH(1); WCUP_VALSTR("qb"); i += 2; break;
            case '\\': WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("bs"); i += 2; break;
            case '\n': WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("nl"); i += 2; break;
            case '\r': WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("cr"); i += 2; break;
            case '\t': WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_VALSTR("tab"); i += 2; break;
            default:   WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_VALSTR("lit"); i += 1; break;
        }
        src++;
    }
    WCUP_VALSTR("done");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("json_escape");
    return i;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <A|B> <str>...\n", argv[0]); return 2; }
    const char *which = argv[1];
    for (int i = 2; i < argc; i++) {
        if (strcmp(which, "A") == 0) json_escape_append_A(argv[i]);
        else if (strcmp(which, "B") == 0) json_escape_append_B(argv[i]);
        else { fprintf(stderr, "pick A or B\n"); return 3; }
    }
    return 0;
}
