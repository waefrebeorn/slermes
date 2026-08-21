/*
 * test_ansi_strip.c — behavioral oracle for lib/libansi/ansi_strip.c.
 * Cross-checked against the real Python tools/ansi_strip.py:
 *   strip_ansi, sanitize_display_text, strip_unicode_tags.
 *
 * Inputs are built as explicit byte arrays to avoid C greedy-hex-escape
 * corruption and to carry multibyte UTF-8 / control bytes precisely.
 * Plane-14 Unicode TAG chars (U+E0000–U+E007F) are 4-byte UTF-8
 * (lead 0xF3, second byte 0xA0). U+1F3F4 (emoji base) is 0xF0 0x9F 0x8F 0xB4.
 *
 * Note: Python str is length-prefixed and can hold embedded NUL; C strings
 * are NUL-terminated, so the sd_ctrl/sd_full control cases use 0x02 (STX)
 * instead of 0x00 (NUL) — both are stripped identically by the C0 regex.
 */

#include "ansi_strip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;

static void check_str(const char *name, const char *got, const char *exp) {
    if (got && exp && strcmp(got, exp) == 0) { g_pass++; return; }
    if (!got && exp && exp[0] == '\0') { g_pass++; return; }
    if (got && !exp && got[0] == '\0') { g_pass++; return; }
    g_fail++;
    printf("FAIL %s:\n  got =%s\n  exp =%s\n", name, got ? got : "(null)", exp ? exp : "(null)");
}

/* helper: build a NUL-terminated byte array from a literal list */
#define A(...) ((char[]){__VA_ARGS__, 0})

int main(void) {
    /* --- strip_ansi (regression guard) --- */
    {
        char in[] = {0x1b, '[', '3', '1', 'm', 'r', 'e', 'd', 0x1b, '[', '0', 'm', ' ', 'n', 'o', 'r', 'm', 'a', 'l', 0};
        char *r = strip_ansi(in);
        check_str("ansi_basic", r, "red normal");
        free(r);
    }
    {
        char in[] = {0x1b, ']', '0', ';', 't', 'i', 't', 'l', 'e', 0x07, 'a', 'f', 't', 'e', 'r', 0};
        char *r = strip_ansi(in);
        check_str("ansi_osc_bel", r, "after");
        free(r);
    }
    {
        char *r = strip_ansi("plain");
        check_str("ansi_plain", r, "plain");
        free(r);
    }

    /* --- strip_unicode_tags --- */
    {
        char *r = strip_unicode_tags("hello world");
        check_str("ut_plain", r, "hello world");
        free(r);
    }
    {
        /* smuggle tags U+E0069 U+E0067 U+E006E stripped -> "inject" */
        char in[] = {0xF3, 0xA0, 0x81, 0xA9, 0xF3, 0xA0, 0x81, 0xA7,
                     0xF3, 0xA0, 0x81, 0xAE, 'i', 'n', 'j', 'e', 'c', 't', 0};
        char *r = strip_unicode_tags(in);
        check_str("ut_smuggle", r, "inject");
        free(r);
    }
    {
        /* valid emoji tag sequence (Scotland) preserved */
        char in[] = {0xF0, 0x9F, 0x8F, 0xB4,            /* U+1F3F4 */
                     0xF3, 0xA0, 0x81, 0xA7,            /* U+E0067 */
                     0xF3, 0xA0, 0x81, 0xA2,            /* U+E0062 */
                     0xF3, 0xA0, 0x81, 0xB3,            /* U+E0073 */
                     0xF3, 0xA0, 0x81, 0xA5,            /* U+E0065 */
                     0xF3, 0xA0, 0x81, 0xBF, 0};        /* U+E007F */
        char *r = strip_unicode_tags(in);
        check_str("ut_emoji_seq", r, in);
        free(r);
    }
    {
        /* mixed: lone tag stripped, emoji seq kept */
        char in[] = {'a', 0xF3, 0xA0, 0x81, 0x81, 'b',       /* U+E0001 */
                     0xF0, 0x9F, 0x8F, 0xB4,                 /* U+1F3F4 */
                     0xF3, 0xA0, 0x81, 0xA7, 0xF3, 0xA0, 0x81, 0xA2,
                     0xF3, 0xA0, 0x81, 0xB3, 0xF3, 0xA0, 0x81, 0xA5, 0xF3, 0xA0, 0x81, 0xBF,
                     'c', 0xF3, 0xA0, 0x81, 0xBF, 'd', 0};   /* trailing U+E007F */
        char exp[] = {'a', 'b',
                      0xF0, 0x9F, 0x8F, 0xB4,
                      0xF3, 0xA0, 0x81, 0xA7, 0xF3, 0xA0, 0x81, 0xA2,
                      0xF3, 0xA0, 0x81, 0xB3, 0xF3, 0xA0, 0x81, 0xA5, 0xF3, 0xA0, 0x81, 0xBF,
                      'c', 'd', 0};
        char *r = strip_unicode_tags(in);
        check_str("ut_mixed", r, exp);
        free(r);
    }
    {
        /* cancel tag alone stripped */
        char in[] = {'x', 0xF3, 0xA0, 0x81, 0xBF, 'y', 0};
        char *r = strip_unicode_tags(in);
        check_str("ut_cancel_only", r, "xy");
        free(r);
    }

    /* --- sanitize_display_text --- */
    {
        char *r = sanitize_display_text("clean text\twith tab\nand newline");
        check_str("sd_plain", r, "clean text\twith tab\nand newline");
        free(r);
    }
    {
        char in[] = {0x1b, '[', '3', '1', 'm', 'r', 'e', 'd', 0x1b, '[', '0', 'm', ' ', 'n', 'o', 'r', 'm', 'a', 'l', 0};
        char *r = sanitize_display_text(in);
        check_str("sd_ansi", r, "red normal");
        free(r);
    }
    {
        char *r = sanitize_display_text("line1\rline2\r\nline3");
        check_str("sd_cr", r, "line1\nline2\nline3");
        free(r);
    }
    {
        /* 0x02 (STX) substitutes for 0x00 (NUL): both stripped by C0 regex */
        char in[] = {'b', 'e', 0x02, 'l', 'l', 0x07, 'c', 'h', 0x1f, ' ', 'a', 'r', 0};
        char *r = sanitize_display_text(in);
        check_str("sd_ctrl", r, "bellch ar");
        free(r);
    }
    {
        char *r = sanitize_display_text("ring\x07me");
        check_str("sd_bel", r, "ringme");
        free(r);
    }
    {
        char in[] = {0x1b, '[', '1', 'm', 'h', 'i', 0x07, 0x02, '\r', '\n', 'b', 'y', 'e', '\t', 'o', 'k', 0};
        char *r = sanitize_display_text(in);
        check_str("sd_full", r, "hi\nbye\tok");
        free(r);
    }

    if (g_fail == 0) printf("ORACLE PASSED (%d checks)\n", g_pass);
    else printf("ORACLE FAILED (%d failures, %d passed)\n", g_fail, g_pass);
    return g_fail == 0 ? 0 : 1;
}
