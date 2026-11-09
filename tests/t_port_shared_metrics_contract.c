/*
 * t_port_shared_metrics_contract.c -- oracle harness for
 * port_shared_metrics_contract.c
 *
 * `kind` mirrors the Python type: 0 = not a number, 1 = int, 2 = float,
 * 3 = bool.
 */
#include "port_shared_metrics_contract.h"

#include <stdio.h>
#include <stdlib.h>

static void emit(const char *fn, const char *label, const char *out)
{
    printf("{\"fn\":\"%s\",\"case\":", fn);
    printf("\"%s\"", label);
    printf(",\"out\":");
    if (!out) { fputs("null", stdout); }
    else {
        putchar('"');
        for (const char *q = out; *q; q++) {
            if (*q == '"' || *q == '\\') { putchar('\\'); putchar(*q); }
            else putchar(*q);
        }
        putchar('"');
    }
    printf("}\n");
}

/* EMIT_FN(fnname, label, call) - fnname is the JSON field name. */
#define EMIT_FN(fnname, label, call) \
    do { emit(fnname, label, (call)); } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- long_size_bucket: boundaries of every bucket ---- */
    EMIT_FN("smc_long_size_bucket", "0",     smc_long_size_bucket(0));
    EMIT_FN("smc_long_size_bucket", "1",     smc_long_size_bucket(1));
    EMIT_FN("smc_long_size_bucket", "2",     smc_long_size_bucket(2));
    EMIT_FN("smc_long_size_bucket", "3_to_5",smc_long_size_bucket(3));
    EMIT_FN("smc_long_size_bucket", "6_to_10",smc_long_size_bucket(6));
    EMIT_FN("smc_long_size_bucket", "11_to_25",smc_long_size_bucket(11));
    EMIT_FN("smc_long_size_bucket", "26_to_100",smc_long_size_bucket(26));
    EMIT_FN("smc_long_size_bucket", "101_to_250",smc_long_size_bucket(101));
    EMIT_FN("smc_long_size_bucket", "251_to_1000",smc_long_size_bucket(251));
    EMIT_FN("smc_long_size_bucket", "gte_1001",smc_long_size_bucket(1001));
    EMIT_FN("smc_long_size_bucket", "huge",  smc_long_size_bucket(999999));

    /* ---- update_duration_bucket ---- */
    EMIT_FN("smc_update_duration_bucket", "lt_30s", smc_update_duration_bucket(1, 0, 0));
    EMIT_FN("smc_update_duration_bucket", "lt_30s-2", smc_update_duration_bucket(1, 29999, 0));
    EMIT_FN("smc_update_duration_bucket", "30s_to_2m", smc_update_duration_bucket(1, 30000, 0));
    EMIT_FN("smc_update_duration_bucket", "5m_to_15m", smc_update_duration_bucket(1, 300000, 0));
    EMIT_FN("smc_update_duration_bucket", "gte_15m", smc_update_duration_bucket(1, 900000, 0));
    EMIT_FN("smc_update_duration_bucket", "float", smc_update_duration_bucket(2, 0, 45000.0));
    EMIT_FN("smc_update_duration_bucket", "notnum", smc_update_duration_bucket(0, 0, 0));
    EMIT_FN("smc_update_duration_bucket", "bool", smc_update_duration_bucket(3, 1, 0));
    EMIT_FN("smc_update_duration_bucket", "negative", smc_update_duration_bucket(1, -5, 0));

    /* ---- version_age_bucket ---- */
    EMIT_FN("smc_version_age_bucket", "lt_7d", smc_version_age_bucket(2, 0, 1000.0));
    EMIT_FN("smc_version_age_bucket", "7d_to_30d", smc_version_age_bucket(2, 0, 7 * 86400000.0));
    EMIT_FN("smc_version_age_bucket", "gte_90d", smc_version_age_bucket(2, 0, 90 * 86400000.0));
    EMIT_FN("smc_version_age_bucket", "unknown", smc_version_age_bucket(0, 0, 0));

    /* ---- desktop_active_minutes_bucket ---- */
    EMIT_FN("smc_desktop_active_minutes_bucket", "zero", smc_desktop_active_minutes_bucket(1, 0, 0));
    EMIT_FN("smc_desktop_active_minutes_bucket", "notnum-zero", smc_desktop_active_minutes_bucket(0, 0, 0));
    EMIT_FN("smc_desktop_active_minutes_bucket", "lt_5m", smc_desktop_active_minutes_bucket(1, 1, 0));
    EMIT_FN("smc_desktop_active_minutes_bucket", "5m_to_30m", smc_desktop_active_minutes_bucket(1, 300000, 0));
    EMIT_FN("smc_desktop_active_minutes_bucket", "2h_to_6h", smc_desktop_active_minutes_bucket(1, 7200000, 0));
    EMIT_FN("smc_desktop_active_minutes_bucket", "gte_6h", smc_desktop_active_minutes_bucket(1, 21600000, 0));

    /* ---- duration_bucket ---- */
    EMIT_FN("smc_duration_bucket", "lt_1s", smc_duration_bucket(0));
    EMIT_FN("smc_duration_bucket", "1s_to_5s", smc_duration_bucket(1000));
    EMIT_FN("smc_duration_bucket", "2m_to_10m", smc_duration_bucket(120000));
    EMIT_FN("smc_duration_bucket", "gte_10m", smc_duration_bucket(600000));
    EMIT_FN("smc_duration_bucket", "negative", smc_duration_bucket(-1));

    /* ---- count_bucket ---- */
    EMIT_FN("smc_count_bucket", "0", smc_count_bucket(0));
    EMIT_FN("smc_count_bucket", "1", smc_count_bucket(1));
    EMIT_FN("smc_count_bucket", "3_to_5", smc_count_bucket(3));
    EMIT_FN("smc_count_bucket", "6_to_10", smc_count_bucket(6));
    EMIT_FN("smc_count_bucket", "gte_11", smc_count_bucket(11));

    /* ---- size_bucket ---- */
    EMIT_FN("smc_size_bucket", "0", smc_size_bucket(0));
    EMIT_FN("smc_size_bucket", "101_to_250", smc_size_bucket(101));
    EMIT_FN("smc_size_bucket", "gte_251", smc_size_bucket(251));

    /* ---- tool_latency_bucket ---- */
    EMIT_FN("smc_tool_latency_bucket", "lt_100ms", smc_tool_latency_bucket(1, 0, 0, 0, 0, 0));
    EMIT_FN("smc_tool_latency_bucket", "100ms_to_250ms", smc_tool_latency_bucket(1, 100, 0, 0, 0, 0));
    EMIT_FN("smc_tool_latency_bucket", "10s_to_30s", smc_tool_latency_bucket(1, 10000, 0, 0, 0, 0));
    EMIT_FN("smc_tool_latency_bucket", "gte_30s", smc_tool_latency_bucket(1, 30000, 0, 0, 0, 0));
    EMIT_FN("smc_tool_latency_bucket", "unknown", smc_tool_latency_bucket(0, 0, 0, 0, 0, 0));
    EMIT_FN("smc_tool_latency_bucket", "fallback", smc_tool_latency_bucket(0, 0, 0, 1, 50, 0));

    /* ---- tool_retry_bucket ---- */
    EMIT_FN("smc_tool_retry_bucket", "0", smc_tool_retry_bucket(1, 0));
    EMIT_FN("smc_tool_retry_bucket", "3_to_5", smc_tool_retry_bucket(1, 3));
    EMIT_FN("smc_tool_retry_bucket", "bool", smc_tool_retry_bucket(3, 1));
    EMIT_FN("smc_tool_retry_bucket", "negative", smc_tool_retry_bucket(1, -1));
    EMIT_FN("smc_tool_retry_bucket", "notnum", smc_tool_retry_bucket(0, 0));

    /* ---- client_os_family ---- */
    EMIT_FN("smc_client_os_family", "darwin", smc_client_os_family("Darwin"));
    EMIT_FN("smc_client_os_family", "linux", smc_client_os_family("Linux"));
    EMIT_FN("smc_client_os_family", "macos", smc_client_os_family("macOS"));
    EMIT_FN("smc_client_os_family", "windows", smc_client_os_family("Windows"));
    EMIT_FN("smc_client_os_family", "unknown", smc_client_os_family("FreeBSD"));
    EMIT_FN("smc_client_os_family", "null", smc_client_os_family(NULL));
    EMIT_FN("smc_client_os_family", "padded", smc_client_os_family("  LINUX  "));

    /* ---- client_architecture ---- */
    EMIT_FN("smc_client_architecture", "amd64", smc_client_architecture("AMD64"));
    EMIT_FN("smc_client_architecture", "x64", smc_client_architecture("x64"));
    EMIT_FN("smc_client_architecture", "x86_64", smc_client_architecture("x86-64"));
    EMIT_FN("smc_client_architecture", "aarch64", smc_client_architecture("aarch64"));
    EMIT_FN("smc_client_architecture", "arm64", smc_client_architecture("arm64"));
    EMIT_FN("smc_client_architecture", "i386", smc_client_architecture("i386"));
    EMIT_FN("smc_client_architecture", "armv7", smc_client_architecture("armv7l"));
    EMIT_FN("smc_client_architecture", "unknown", smc_client_architecture("sparc"));
    EMIT_FN("smc_client_architecture", "null", smc_client_architecture(NULL));

    return 0;
}
