/*
 * t_port_gateway_response_filters.c — faithful verification harness for
 * port_gateway_response_filters.c. Compiled separately, links against the
 * real object (not the slermes binary). Each check asserts C output equals
 * the LIVE Python gateway/response_filters.py:is_partial_silence_marker.
 */
#include "port_gateway_response_filters.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(const char *label, int got, int exp)
{
    if (got == exp) printf("  PASS  %s (got=%d)\n", label, got);
    else { printf("  FAIL  %s got=%d exp=%d\n", label, got, exp); failures++; }
}

int main(void)
{
    /* cases: text -> expected (LIVE Python is_partial_silence_marker) */
    struct { const char *in; int exp; } cases[] = {
        {"NO", 1},          /* prefix of NO_REPLY / NO REPLY */
        {"NO_", 1},         /* partial */
        {"NO_REPLY", 1},    /* exact marker */
        {"NO REPLY", 1},    /* exact marker */
        {"SIL", 1},         /* prefix of SILENT */
        {"SILENT", 1},      /* exact */
        {"[SILENT]", 1},    /* exact */
        {"[SIL", 1},        /* prefix */
        {"hello", 0},       /* diverged */
        {"NO_REPLY now", 0},/* diverged after marker */
        {"", 0},            /* blank */
        {"   ", 0},         /* whitespace only */
        {"NO_REPLY_EXTRA_LONG_TEXT", 0}, /* >64 not the case here but diverged */
        {"xyz NO_REPLY", 0},/* leading prose -> canonical != prefix */
        {"noreply", 0},     /* canonicalizes to NOREPLY (single token) -> not a marker prefix */
        {"no reply", 1},    /* canonicalizes to NO REPLY */
        {"silent", 1},      /* canonicalizes to SILENT */
    };
    for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
        int got = cli_gateway_response_filters_is_partial_silence_marker(cases[i].in);
        char lbl[80]; snprintf(lbl,sizeof(lbl),"\"%s\"",cases[i].in);
        check(lbl, got, cases[i].exp);
    }
    printf("\nC HARNESS (%d failures)\n", failures);
    return failures ? 1 : 0;
}
