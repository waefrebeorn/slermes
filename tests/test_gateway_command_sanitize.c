/*
 * test_gateway_command_sanitize.c — unit tests for the pure helpers in
 * src/cli/gateway_command_sanitize.c. Commands-name sanitize/clamp/dedupe are
 * covered by their existing callers; this test pins the genuinely-missing
 * commands_file_size_label (PoP: hermes_cli/commands.py:_file_size_label)
 * against a Python oracle, plus a sanity sweep of the existing sanitizers so
 * the reused module stays verified.
 */

#include "gateway_command_sanitize.h"
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
    /* file_size_label — derived from _file_size_label oracle */
    struct { long sz; const char *e; } sizes[] = {
        {0, "0B"}, {500, "500B"}, {1023, "1023B"}, {1024, "1K"}, {1536, "2K"},
        {1048575, "1024K"}, {1048576, "1.0M"}, {1073741824, "1.0G"}, {1500000000, "1.4G"},
    };
    for (int i = 0; i < 9; i++) {
        char *g = commands_file_size_label(sizes[i].sz);
        eq("file_size_label", g, sizes[i].e);
        free(g);
    }

    /* sanity sweep of the existing sanitizers (reused, must stay faithful) */
    struct { const char *raw; const char *tg; const char *sl; } san[] = {
        {"My-Cool Command!", "my_coolcommand", "my-coolcommand"},
        {"my_cool__command", "my_cool_command", "my_cool__command"},
        {"  __weird__  ", "weird", "weird"},
        {"aBc-123.xYz", "abc_123xyz", "abc-123xyz"},
    };
    for (int i = 0; i < 4; i++) {
        char *g = commands_sanitize_telegram_name(san[i].raw);
        eq("tg", g, san[i].tg); free(g);
        char *s = commands_sanitize_slack_name(san[i].raw);
        eq("sl", s, san[i].sl); free(s);
    }

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
