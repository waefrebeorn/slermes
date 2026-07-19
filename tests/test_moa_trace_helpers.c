/*
 * test_moa_trace_helpers.c — unit tests for the pure agent/moa_trace.py
 * helpers. Expected values derived from a Python oracle of _sanitize_session_id.
 */

#include "moa_trace_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static void eq(const char *label, const char *got, const char *exp)
{
    if (!got || strcmp(got, exp) != 0) {
        printf("FAIL: %s\n  expected=[%s]\n  got    =[%s]\n", label, exp, got ? got : "(null)");
        g_fail++;
    } else {
        printf("ok: %s -> [%s]\n", label, got);
    }
}

int main(void)
{
    char *g;
    g = moa_trace_sanitize_session_id(NULL);  eq("NULL", g, "unknown-session"); free(g);
    g = moa_trace_sanitize_session_id("");    eq("empty", g, "unknown-session"); free(g);
    g = moa_trace_sanitize_session_id("abc123"); eq("abc123", g, "abc123"); free(g);
    g = moa_trace_sanitize_session_id("sess/with/slashes"); eq("slashes", g, "sess_with_slashes"); free(g);
    g = moa_trace_sanitize_session_id("weird!@#name"); eq("weird", g, "weird___name"); free(g);
    g = moa_trace_sanitize_session_id("a.b-c_d"); eq("keep", g, "a.b-c_d"); free(g);
    g = moa_trace_sanitize_session_id("spaces in id"); eq("spaces", g, "spaces_in_id"); free(g);
    g = moa_trace_sanitize_session_id("UPPER"); eq("upper", g, "UPPER"); free(g);
    g = moa_trace_sanitize_session_id("../escape"); eq("escape", g, ".._escape"); free(g);
    g = moa_trace_sanitize_session_id("ok.id-1"); eq("ok", g, "ok.id-1"); free(g);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
