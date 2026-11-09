/*
 * port_shared_metrics_contract.c
 *
 * Pure helpers ported from
 * hermes_cli/observability/shared_metrics_contract.py (72 defs, previously 0
 * ported). The module defines the shared-metrics taxonomy: fixed low-cardinality
 * buckets and platform normalisations.
 *
 *   smc_long_size_bucket           <- long_size_bucket
 *   smc_update_duration_bucket     <- update_duration_bucket
 *   smc_version_age_bucket         <- version_age_bucket
 *   smc_desktop_active_minutes_bucket  <- desktop_active_minutes_bucket
 *   smc_duration_bucket            <- duration_bucket
 *   smc_count_bucket               <- count_bucket
 *   smc_tool_latency_bucket        <- tool_latency_bucket
 *   smc_tool_retry_bucket          <- tool_retry_bucket
 *   smc_size_bucket                <- size_bucket
 *   smc_client_os_family           <- client_os_family
 *   smc_client_architecture        <- client_architecture
 *
 * `_bucket` is the shared primitive: the first threshold whose UPPER bound
 * exceeds the value wins, else the `last` label. Thresholds are half-open
 * [prev, upper), exactly as Python's `value < upper` test makes them.
 */

#include "port_shared_metrics_contract.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SMC_DAY_MS 86400000.0

/* shared_metrics_contract.py:_LONG_SIZE_THRESHOLDS */
static const smc_threshold_t long_size_thr[] = {
    {1, "0"}, {2, "1"}, {3, "2"}, {6, "3_to_5"}, {11, "6_to_10"}, {26, "11_to_25"},
    {101, "26_to_100"}, {251, "101_to_250"}, {1001, "251_to_1000"},
};
#define LONG_SIZE_N (sizeof(long_size_thr) / sizeof(long_size_thr[0]))

/* shared_metrics_contract.py:_UPDATE_DURATION_THRESHOLDS */
static const smc_threshold_t update_dur_thr[] = {
    {30000, "lt_30s"}, {120000, "30s_to_2m"}, {300000, "2m_to_5m"}, {900000, "5m_to_15m"},
};
#define UPDATE_DUR_N (sizeof(update_dur_thr) / sizeof(update_dur_thr[0]))

/* shared_metrics_contract.py:_VERSION_AGE_THRESHOLDS */
static const smc_threshold_t version_age_thr[] = {
    {7 * SMC_DAY_MS, "lt_7d"}, {30 * SMC_DAY_MS, "7d_to_30d"}, {90 * SMC_DAY_MS, "30d_to_90d"},
};
#define VERSION_AGE_N (sizeof(version_age_thr) / sizeof(version_age_thr[0]))

/* shared_metrics_contract.py:_DESKTOP_ACTIVE_THRESHOLDS */
static const smc_threshold_t desktop_thr[] = {
    {300000, "lt_5m"}, {1800000, "5m_to_30m"}, {7200000, "30m_to_2h"},
    {21600000, "2h_to_6h"},
};
#define DESKTOP_N (sizeof(desktop_thr) / sizeof(desktop_thr[0]))

/* shared_metrics_contract.py:_DURATION_THRESHOLDS */
static const smc_threshold_t duration_thr[] = {
    {1000, "lt_1s"}, {5000, "1s_to_5s"}, {30000, "5s_to_30s"},
    {120000, "30s_to_2m"}, {600000, "2m_to_10m"},
};
#define DURATION_N (sizeof(duration_thr) / sizeof(duration_thr[0]))

/* shared_metrics_contract.py:_COUNT_THRESHOLDS */
static const smc_threshold_t count_thr[] = {
    {1, "0"}, {2, "1"}, {3, "2"}, {6, "3_to_5"}, {11, "6_to_10"},
};
#define COUNT_N (sizeof(count_thr) / sizeof(count_thr[0]))

/* shared_metrics_contract.py:_LATENCY_THRESHOLDS */
static const smc_threshold_t latency_thr[] = {
    {100, "lt_100ms"}, {250, "100ms_to_250ms"}, {500, "250ms_to_500ms"},
    {1000, "500ms_to_1s"}, {2000, "1s_to_2s"}, {5000, "2s_to_5s"},
    {10000, "5s_to_10s"}, {30000, "10s_to_30s"},
};
#define LATENCY_N (sizeof(latency_thr) / sizeof(latency_thr[0]))

/* shared_metrics_contract.py:_SIZE_THRESHOLDS */
static const smc_threshold_t size_thr[] = {
    {1, "0"}, {2, "1"}, {3, "2"}, {6, "3_to_5"}, {11, "6_to_10"}, {26, "11_to_25"},
    {101, "26_to_100"}, {251, "101_to_250"},
};
#define SIZE_N (sizeof(size_thr) / sizeof(size_thr[0]))

/* ===========================================================================
 *  _bucket  (shared_metrics_contract.py:_bucket)
 *
 *      return next((label for upper, label in thresholds if value < upper), last)
 *
 *  Strict `<`: a value exactly ON a threshold falls into the NEXT bucket.
 * =========================================================================== */

/* PoP: _bucket @ hermes_cli/observability/shared_metrics_contract.py:_bucket */
const char *smc_bucket(double value, const smc_threshold_t *thresholds, size_t n,
                       const char *last)
{
    for (size_t i = 0; i < n; i++) {
        if (value < thresholds[i].upper) return thresholds[i].label;
    }
    return last;
}

/* ===========================================================================
 *  _non_negative_number  (shared_metrics_contract.py:_non_negative_number)
 *
 *      if isinstance(value, bool) or not isinstance(value, (int, float)):
 *          return None
 *      try: number = float(value)
 *      except (OverflowError, TypeError, ValueError): return None
 *      return number if isfinite(number) and number >= 0 else None
 *
 *  `kind` carries the Python type: 0 = not a number, 1 = int, 2 = float,
 *  3 = bool (rejected outright - bool is a subclass of int in Python, so the
 *  isinstance(value, bool) guard must run FIRST). Returns 1 when `out` is set.
 * =========================================================================== */

/* PoP: _non_negative_number @ hermes_cli/observability/shared_metrics_contract.py:_non_negative_number */
int smc_non_negative_number(int kind, long long ival, double dval, double *out)
{
    double number;
    if (kind == 3) return 0;                 /* bool - rejected before the int check */
    if (kind == 1) number = (double)ival;
    else if (kind == 2) number = dval;
    else return 0;                           /* not int/float */

    if (!isfinite(number) || number < 0) return 0;
    if (out) *out = number;
    return 1;
}

/* ===========================================================================
 *  The ten public bucket functions.
 * =========================================================================== */

/* PoP: long_size_bucket @ hermes_cli/observability/shared_metrics_contract.py:long_size_bucket */
const char *smc_long_size_bucket(long long count)
{
    double v = (double)(count > 0 ? count : 0);
    return smc_bucket(v, long_size_thr, LONG_SIZE_N, "gte_1001");
}

/* PoP: update_duration_bucket @ hermes_cli/observability/shared_metrics_contract.py:update_duration_bucket */
const char *smc_update_duration_bucket(int kind, long long ival, double dval)
{
    double value;
    /* `or 0`: a non-number counts as instant */
    if (!smc_non_negative_number(kind, ival, dval, &value)) value = 0;
    return smc_bucket(value, update_dur_thr, UPDATE_DUR_N, "gte_15m");
}

/* PoP: version_age_bucket @ hermes_cli/observability/shared_metrics_contract.py:version_age_bucket */
const char *smc_version_age_bucket(int kind, long long ival, double dval)
{
    double value;
    /* no `or 0` here: unknown stays unknown */
    if (!smc_non_negative_number(kind, ival, dval, &value)) return "unknown";
    return smc_bucket(value, version_age_thr, VERSION_AGE_N, "gte_90d");
}

/* PoP: desktop_active_minutes_bucket @ hermes_cli/observability/shared_metrics_contract.py:desktop_active_minutes_bucket */
const char *smc_desktop_active_minutes_bucket(int kind, long long ival, double dval)
{
    double value;
    if (!smc_non_negative_number(kind, ival, dval, &value)) value = 0;
    if (value <= 0) return "0";
    return smc_bucket(value, desktop_thr, DESKTOP_N, "gte_6h");
}

/* PoP: duration_bucket @ hermes_cli/observability/shared_metrics_contract.py:duration_bucket */
const char *smc_duration_bucket(long long duration_ms)
{
    double v = (double)(duration_ms > 0 ? duration_ms : 0);
    return smc_bucket(v, duration_thr, DURATION_N, "gte_10m");
}

/* PoP: count_bucket @ hermes_cli/observability/shared_metrics_contract.py:count_bucket */
const char *smc_count_bucket(long long count)
{
    double v = (double)(count > 0 ? count : 0);
    return smc_bucket(v, count_thr, COUNT_N, "gte_11");
}

/* PoP: tool_retry_bucket @ hermes_cli/observability/shared_metrics_contract.py:tool_retry_bucket */
const char *smc_tool_retry_bucket(int kind, long long ival)
{
    /* only explicit int retries; bool / non-int / negative -> unknown */
    if (kind != 1 || ival < 0) return "unknown";
    return smc_count_bucket(ival);
}

/* PoP: tool_latency_bucket @ hermes_cli/observability/shared_metrics_contract.py:tool_latency_bucket */
const char *smc_tool_latency_bucket(int kind, long long ival, double dval,
                                    int fb_kind, long long fb_ival, double fb_dval)
{
    double duration_ms;
    if (!smc_non_negative_number(kind, ival, dval, &duration_ms)) {
        if (!smc_non_negative_number(fb_kind, fb_ival, fb_dval, &duration_ms)) return "unknown";
    }
    return smc_bucket(duration_ms, latency_thr, LATENCY_N, "gte_30s");
}

/* PoP: size_bucket @ hermes_cli/observability/shared_metrics_contract.py:size_bucket */
const char *smc_size_bucket(long long count)
{
    double v = (double)(count > 0 ? count : 0);
    return smc_bucket(v, size_thr, SIZE_N, "gte_251");
}

/* ===========================================================================
 *  _norm / client_os_family / client_architecture
 *
 *      _norm(value) = str(value or "").strip().lower()
 *      client_os_family = _OS_FAMILIES.get(_norm(value), "unknown")
 *      client_architecture: _norm(value).replace("-", "_"), then the alias
 *      table, then armv* -> "arm", else "unknown"
 *
 *  `str(value or "")` means None / "" / 0 / false all become "".
 * =========================================================================== */

/* PoP: _norm @ hermes_cli/observability/shared_metrics_contract.py:_norm */
char *smc_norm(const char *value)
{
    if (!value) value = "";
    size_t n = strlen(value);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    for (size_t i = 0; i < n; i++) s[i] = (char)tolower((unsigned char)value[i]);
    s[n] = '\0';
    /* strip - the C locale's isspace matches Python's str.strip() for ASCII */
    char *a = s, *b = s + n;
    while (*a && (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\f' || *a == '\v')) a++;
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r'
                     || b[-1] == '\f' || b[-1] == '\v')) *--b = '\0';
    memmove(s, a, (size_t)(b - a) + 1);
    return s;
}

/* shared_metrics_contract.py:_OS_FAMILIES */
static const char *const os_families[][2] = {
    {"darwin", "macos"}, {"linux", "linux"}, {"macos", "macos"}, {"windows", "windows"},
};
#define OS_FAMILIES_N (sizeof(os_families) / sizeof(os_families[0]))

/* shared_metrics_contract.py:_ARCHITECTURE_ALIASES */
static const char *const arch_aliases[][2] = {
    {"amd64", "x86_64"}, {"x64", "x86_64"}, {"x86_64", "x86_64"},
    {"aarch64", "arm64"}, {"arm64", "arm64"},
    {"i386", "x86"}, {"i486", "x86"}, {"i586", "x86"}, {"i686", "x86"}, {"x86", "x86"},
};
#define ARCH_ALIASES_N (sizeof(arch_aliases) / sizeof(arch_aliases[0]))

/* PoP: client_os_family @ hermes_cli/observability/shared_metrics_contract.py:client_os_family */
const char *smc_client_os_family(const char *value)
{
    char *n = smc_norm(value);
    if (!n) return "unknown";
    const char *result = "unknown";
    for (size_t i = 0; i < OS_FAMILIES_N; i++) {
        if (strcmp(n, os_families[i][0]) == 0) { result = os_families[i][1]; break; }
    }
    free(n);
    return result;
}

/* PoP: client_architecture @ hermes_cli/observability/shared_metrics_contract.py:client_architecture */
const char *smc_client_architecture(const char *value)
{
    char *n = smc_norm(value);
    if (!n) return "unknown";
    for (size_t i = 0; n[i]; i++) if (n[i] == '-') n[i] = '_';

    const char *result = NULL;
    for (size_t i = 0; i < ARCH_ALIASES_N; i++) {
        if (strcmp(n, arch_aliases[i][0]) == 0) { result = arch_aliases[i][1]; break; }
    }
    if (!result && strncmp(n, "armv", 4) == 0) result = "arm";
    free(n);
    return result ? result : "unknown";
}
