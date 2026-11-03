/* Focused check of the two functions changed in this pass. The full
 * test_gateway_escape.c uses GNU nested functions whose trampolines crash on
 * PE, and its binary is intermittently refused by the OS, so the behaviour is
 * verified directly here. */
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, fail;
static void ck(const char *name, int cond) {
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", name);
    cond ? pass++ : fail++;
}

int main(void)
{
    printf("=== gateway escape primitives ===\n");

    ck("utf16 NULL = 0", gw_utf16_len(NULL) == 0);
    ck("utf16 empty = 0", gw_utf16_len("") == 0);
    ck("utf16 ascii 5", gw_utf16_len("hello") == 5);
    ck("utf16 accented = 5", gw_utf16_len("h\xc3\xa9llo") == 5);
    ck("utf16 CJK = 2", gw_utf16_len("\xe4\xb8\xad\xe6\x96\x87") == 2);
    ck("utf16 emoji = 2", gw_utf16_len("\xf0\x9f\x98\x80") == 2);
    ck("utf16 emoji in context = 12",
       gw_utf16_len("hello\xf0\x9f\x98\x80world") == 12);
    ck("utf16 two emoji = 4",
       gw_utf16_len("\xf0\x9f\x98\x80\xf0\x9f\x98\x8e") == 4);

    char *r;

    ck("prefix NULL -> NULL", gw_prefix_within_utf16_limit(NULL, 5) == NULL);
    r = gw_prefix_within_utf16_limit("hello", 0);
    ck("prefix limit 0 -> NULL", r == NULL);
    free(r);

    r = gw_prefix_within_utf16_limit("hello", 5);
    ck("prefix ascii 5", r && strcmp(r, "hello") == 0);
    free(r);

    r = gw_prefix_within_utf16_limit("hello", 3);
    ck("prefix ascii 3", r && strcmp(r, "hel") == 0);
    free(r);

    r = gw_prefix_within_utf16_limit("h\xc3\xa9llo", 3);
    ck("prefix accented boundary", r && strcmp(r, "h\xc3\xa9l") == 0);
    free(r);

    /* Over-budget characters must be dropped, not leaked into the prefix. */
    r = gw_prefix_within_utf16_limit("hi\xf0\x9f\x98\x80world", 6);
    ck("prefix emoji within 6", r && strcmp(r, "hi\xf0\x9f\x98\x80wo") == 0);
    free(r);

    r = gw_prefix_within_utf16_limit("hi\xf0\x9f\x98\x80world", 4);
    ck("prefix emoji exact 4", r && strcmp(r, "hi\xf0\x9f\x98\x80") == 0);
    free(r);

    r = gw_prefix_within_utf16_limit("hi\xf0\x9f\x98\x80world", 3);
    ck("prefix emoji over budget stops before", r && strcmp(r, "hi") == 0);
    free(r);

    r = gw_prefix_within_utf16_limit("\xe4\xb8\xad\xe6\x96\x87", 1);
    ck("prefix CJK at 1", r && strcmp(r, "\xe4\xb8\xad") == 0);
    free(r);

    ck("float_env NULL -> default", gw_float_env(NULL, 3.14) == 3.14);
    ck("float_env missing -> default",
       gw_float_env("GW_FLOAT_ENV_NONEXISTENT", 1.5) == 1.5);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
