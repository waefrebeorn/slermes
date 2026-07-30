/*
 * test_checkpoints_format.c — unit tests for the pure hermes_cli/checkpoints.py
 * format helpers. fmt_ts expected values are computed in UTC (the C port uses
 * gmtime_r) to match the %Y-%m-%d %H:%M format.
 */

#include "port_checkpoints_format.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_fail = 0;

static void eq(const char *label, const char *got, const char *exp)
{
    if (strcmp(got ? got : "", exp) != 0) {
        printf("FAIL: %s\n  expected=[%s]\n  got    =[%s]\n", label, exp, got ? got : "(null)");
        g_fail++;
    } else {
        printf("ok: %s -> [%s]\n", label, got);
    }
}

int main(void)
{
    char buf[64];

    /* fmt_bytes */
    hermes_cli_checkpoints_fmt_bytes(0, buf, sizeof(buf));     eq("bytes(0)", buf, "0 B");
    hermes_cli_checkpoints_fmt_bytes(512, buf, sizeof(buf));   eq("bytes(512)", buf, "512 B");
    hermes_cli_checkpoints_fmt_bytes(1536, buf, sizeof(buf));  eq("bytes(1536)", buf, "1.5 KB");
    hermes_cli_checkpoints_fmt_bytes(1048576, buf, sizeof(buf)); eq("bytes(1048576)", buf, "1.0 MB");
    hermes_cli_checkpoints_fmt_bytes(1073741824L, buf, sizeof(buf)); eq("bytes(1G)", buf, "1.0 GB");

    /* fmt_age (now = 1_700_000_000) */
    double now = 1700000000.0;
    hermes_cli_checkpoints_fmt_age(now - 30, now, buf, sizeof(buf));        eq("age(30s)", buf, "30s ago");
    hermes_cli_checkpoints_fmt_age(now - 3600, now, buf, sizeof(buf));      eq("age(1h)", buf, "1h ago");
    hermes_cli_checkpoints_fmt_age(now - 86400, now, buf, sizeof(buf));     eq("age(1d)", buf, "1d ago");
    hermes_cli_checkpoints_fmt_age(now + 100, now, buf, sizeof(buf));       eq("age(future)", buf, "now");
    hermes_cli_checkpoints_fmt_age(NAN, now, buf, sizeof(buf));             eq("age(nan)", buf, "—");

    /* fmt_ts (UTC) */
    hermes_cli_checkpoints_fmt_ts(0, buf, sizeof(buf));                     eq("ts(0)", buf, "1970-01-01 00:00");
    hermes_cli_checkpoints_fmt_ts(1700000000.0, buf, sizeof(buf));          eq("ts(1700000000)", buf, "2023-11-14 22:13");
    hermes_cli_checkpoints_fmt_ts(1609459200.0, buf, sizeof(buf));          eq("ts(1609459200)", buf, "2021-01-01 00:00");
    hermes_cli_checkpoints_fmt_ts(NAN, buf, sizeof(buf));                   eq("ts(nan)", buf, "—");

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
