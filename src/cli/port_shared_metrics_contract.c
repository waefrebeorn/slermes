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

#include "libjson/json.h"

#include <math.h>
#include <stdio.h>
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

/* ===========================================================================
 *  The normalisation half of the module.
 *
 *  _allowlisted          (shared_metrics_contract.py:_allowlisted)
 *  client_install_method (shared_metrics_contract.py:client_install_method)
 *  client_resource       (shared_metrics_contract.py:client_resource)
 *  client_resource_is_valid (shared_metrics_contract.py:client_resource_is_valid)
 *  _metric_identifier    (shared_metrics_contract.py:_metric_identifier)
 *  skill_provenance      (shared_metrics_contract.py:skill_provenance)
 *  tool_category         (shared_metrics_contract.py:tool_category)
 *  tool_outcome          (shared_metrics_contract.py:tool_outcome)
 *  model_error_class     (shared_metrics_contract.py:model_error_class)
 *
 *  Every set-membership test returns a pointer into a static table (never the
 *  caller's buffer), so callers may free their own string immediately.
 * =========================================================================== */

/* shared_metrics_contract.py:CLIENT_INSTALL_METHODS */
static const char *const client_install_methods[] = {
    "apt", "docker", "git", "home-manager", "homebrew", "microsoft-store", "nixos", "pip",
    "unknown",
};
#define CLIENT_INSTALL_METHODS_N (sizeof(client_install_methods) / sizeof(client_install_methods[0]))

/* shared_metrics_contract.py:CLIENT_OS_FAMILIES */
static const char *const client_os_families[] = {"linux", "macos", "unknown", "windows"};
#define CLIENT_OS_FAMILIES_N (sizeof(client_os_families) / sizeof(client_os_families[0]))

/* shared_metrics_contract.py:CLIENT_ARCHITECTURES */
static const char *const client_architectures[] = {"arm", "arm64", "unknown", "x86", "x86_64"};
#define CLIENT_ARCHITECTURES_N (sizeof(client_architectures) / sizeof(client_architectures[0]))

/* shared_metrics_contract.py:CLIENT_RESOURCE_KEYS */
static const char *const client_resource_keys[] = {
    "architecture", "hermes_version", "install_method", "os_family",
};
#define CLIENT_RESOURCE_KEYS_N (sizeof(client_resource_keys) / sizeof(client_resource_keys[0]))

/* shared_metrics_contract.py:SKILL_PROVENANCES */
static const char *const skill_provenances[] = {
    "agent_created", "external", "installed", "local", "unknown",
};
#define SKILL_PROVENANCES_N (sizeof(skill_provenances) / sizeof(skill_provenances[0]))

/* shared_metrics_contract.py:MODEL_ERROR_CLASSES -- the classifier's closed enum
 * (FailoverReason) plus "none". Kept as data rather than derived from the C
 * failover_reason_t, whose enum is a smaller set. */
static const char *const model_error_classes[] = {
    "auth", "auth_permanent", "billing", "content_policy_blocked", "context_overflow",
    "data_policy_denied", "format_error", "image_corrupt", "image_too_large",
    "incomplete_response", "invalid_encrypted_content", "llama_cpp_grammar_pattern",
    "long_context_tier", "model_entitlement", "model_not_found",
    "multimodal_tool_content_unsupported", "oauth_long_context_beta_forbidden", "overloaded",
    "payload_too_large", "provider_policy_blocked", "rate_limit", "reasoning_mandatory",
    "role_alternation", "server_error", "ssl_cert_verification", "thinking_signature",
    "timeout", "unknown", "upstream_blocked", "upstream_rate_limit", "none",
};
#define MODEL_ERROR_CLASSES_N (sizeof(model_error_classes) / sizeof(model_error_classes[0]))

/* shared_metrics_contract.py:_METRIC_IDENTIFIER_CHARACTERS / _START_CHARACTERS */
#define METRIC_IDENT_CHARS "abcdefghijklmnopqrstuvwxyz0123456789._:/@+-"
#define METRIC_IDENT_START "abcdefghijklmnopqrstuvwxyz0123456789"

/* shared_metrics_contract.py:TOOL_CATEGORIES */
static const char *const tool_categories[] = {
    "browser", "code_execution", "communication", "computer_use", "connector", "delegation",
    "file", "home_automation", "mcp", "media", "memory", "other", "planning", "project",
    "scheduler", "skill", "terminal", "unknown", "web",
};
#define TOOL_CATEGORIES_N (sizeof(tool_categories) / sizeof(tool_categories[0]))

/* tools/connectors/gateway/names.py:CONNECTOR_NAME_PREFIX */
#define CONNECTOR_NAME_PREFIX "connectors__"

/* shared_metrics_contract.py:_TOOL_CATEGORY_EXACT -- the identity mapping for
 * every TOOL_CATEGORIES member, plus these aliases. */
static const char *const tool_category_exact[][2] = {
    {"clarify", "planning"}, {"kanban", "planning"}, {"todo", "planning"},
    {"session_search", "memory"}, {"cronjob", "scheduler"}, {"skills", "skill"},
    {"x_search", "web"},
};
#define TOOL_CATEGORY_EXACT_N (sizeof(tool_category_exact) / sizeof(tool_category_exact[0]))

/* shared_metrics_contract.py:_TOOL_CATEGORY_PREFIXES -- a prefix (or prefix group)
 * wins over the "other" fallback; the first matching rule wins. */
static const char *const tool_prefix_mcp[] = {"mcp"};
static const char *const tool_prefix_browser[] = {"browser"};
static const char *const tool_prefix_media[] = {"image", "tts", "video", "vision"};
static const char *const tool_prefix_home[] = {"homeassistant"};
static const char *const tool_prefix_comm[] = {
    "discord", "email", "feishu", "hermes-yuanbao", "slack", "sms",
};
typedef struct {
    const char *const *prefixes;
    size_t n;
    const char *category;
} smc_prefix_rule_t;
static const smc_prefix_rule_t tool_prefix_rules[] = {
    {tool_prefix_mcp, 1, "mcp"},
    {tool_prefix_browser, 1, "browser"},
    {tool_prefix_media, 4, "media"},
    {tool_prefix_home, 1, "home_automation"},
    {tool_prefix_comm, 6, "communication"},
};
#define TOOL_PREFIX_RULES_N (sizeof(tool_prefix_rules) / sizeof(tool_prefix_rules[0]))

/* shared_metrics_contract.py:_TOOL_STATUS_OUTCOMES */
static const char *const tool_status_outcomes[][2] = {
    {"blocked", "blocked"}, {"cancelled", "cancelled"}, {"failed", "failed"},
    {"success", "success"}, {"timed_out", "timed_out"},
    {"error", "failed"}, {"ok", "success"}, {"timeout", "timed_out"},
};
#define TOOL_STATUS_N (sizeof(tool_status_outcomes) / sizeof(tool_status_outcomes[0]))

static int smc_in_set(const char *value, const char *const *set, size_t n)
{
    if (!value) return 0;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(value, set[i]) == 0) return 1;
    }
    return 0;
}

/* str.strip() for a fresh malloc'd copy (no case folding). */
static char *smc_strip_dup(const char *value)
{
    if (!value) return NULL;
    const char *a = value;
    while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\f' || *a == '\v') a++;
    size_t n = strlen(a);
    while (n > 0 && (a[n - 1] == ' ' || a[n - 1] == '\t' || a[n - 1] == '\n'
                     || a[n - 1] == '\r' || a[n - 1] == '\f' || a[n - 1] == '\v')) n--;
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, a, n);
    s[n] = '\0';
    return s;
}

/* ===========================================================================
 *  _allowlisted  (shared_metrics_contract.py:_allowlisted)
 *
 *      return normalized if normalized in allowed else "unknown"
 *
 *  Returns the matching STATIC table entry, never `normalized`, so the result
 *  stays valid after the caller frees its own string.
 * =========================================================================== */

/* PoP: _allowlisted @ hermes_cli/observability/shared_metrics_contract.py:_allowlisted */
const char *smc_allowlisted(const char *normalized, const char *const *allowed, size_t n)
{
    if (normalized) {
        for (size_t i = 0; i < n; i++) {
            if (strcmp(normalized, allowed[i]) == 0) return allowed[i];
        }
    }
    return "unknown";
}

/* ===========================================================================
 *  client_install_method  (shared_metrics_contract.py:client_install_method)
 *
 *      normalized = _norm(value)
 *      return _allowlisted("nixos" if normalized == "nix" else normalized, CLIENT_INSTALL_METHODS)
 * =========================================================================== */

/* PoP: client_install_method @ hermes_cli/observability/shared_metrics_contract.py:client_install_method */
const char *smc_client_install_method(const char *value)
{
    char *n = smc_norm(value);
    if (!n) return "unknown";
    const char *key = (strcmp(n, "nix") == 0) ? "nixos" : n;
    const char *result = smc_allowlisted(key, client_install_methods, CLIENT_INSTALL_METHODS_N);
    free(n);
    return result;
}

/* ===========================================================================
 *  client_resource  (shared_metrics_contract.py:client_resource)
 *
 *      version = str(hermes_version or "").strip()
 *      {"architecture": ..., "hermes_version": version if 0 < len(version) <= 64
 *                        else "unknown", "install_method": ..., "os_family": ...}
 *
 *  `hermes_version` is stripped but NOT case-folded, unlike the other fields.
 * =========================================================================== */

/* PoP: client_resource @ hermes_cli/observability/shared_metrics_contract.py:client_resource */
json_t *smc_client_resource(const char *hermes_version, const char *os_name,
                            const char *architecture, const char *install_method)
{
    char *version = smc_strip_dup(hermes_version);
    json_t *o = json_object();
    if (!o) { free(version); return NULL; }
    json_set_hdr(o, "architecture", smc_client_architecture(architecture));
    json_set_hdr(o, "hermes_version",
                 (version && *version && strlen(version) <= 64) ? version : "unknown");
    json_set_hdr(o, "install_method", smc_client_install_method(install_method));
    json_set_hdr(o, "os_family", smc_client_os_family(os_name));
    free(version);
    return o;
}

/* ===========================================================================
 *  client_resource_is_valid  (shared_metrics_contract.py:client_resource_is_valid)
 *
 *      set(resource) == CLIENT_RESOURCE_KEYS  (exactly those four keys)
 *      and hermes_version is a 1..64 char string
 *      and os_family / architecture / install_method are in their taxonomies
 * =========================================================================== */

/* PoP: client_resource_is_valid @ hermes_cli/observability/shared_metrics_contract.py:client_resource_is_valid */
int smc_client_resource_is_valid(json_t *resource)
{
    if (!resource || resource->type != JSON_OBJECT) return 0;
    if (resource->c.count != CLIENT_RESOURCE_KEYS_N) return 0;
    for (size_t i = 0; i < CLIENT_RESOURCE_KEYS_N; i++) {
        if (!json_obj_get(resource, client_resource_keys[i])) return 0;
    }
    json_t *version = json_obj_get(resource, "hermes_version");
    if (!version || version->type != JSON_STRING) return 0;
    size_t vlen = strlen(version->str_val);
    if (!(0 < vlen && vlen <= 64)) return 0;
    return smc_in_set(json_get_str(resource, "os_family", NULL),
                      client_os_families, CLIENT_OS_FAMILIES_N)
        && smc_in_set(json_get_str(resource, "architecture", NULL),
                      client_architectures, CLIENT_ARCHITECTURES_N)
        && smc_in_set(json_get_str(resource, "install_method", NULL),
                      client_install_methods, CLIENT_INSTALL_METHODS_N);
}

/* ===========================================================================
 *  _metric_identifier  (shared_metrics_contract.py:_metric_identifier)
 *
 *      identifier = value.strip().lower()
 *      not identifier or len > max_length or identifier[0] not a start char
 *      or any char outside the allowed set  ->  "unknown"
 *
 *  Caller frees.
 * =========================================================================== */

/* PoP: _metric_identifier @ hermes_cli/observability/shared_metrics_contract.py:_metric_identifier */
char *smc_metric_identifier(const char *value, size_t max_length)
{
    if (!value) return strdup("unknown");       /* not isinstance(value, str) */
    char *identifier = smc_norm(value);
    if (!identifier) return strdup("unknown");
    size_t len = strlen(identifier);
    int ok = (len > 0 && len <= max_length
              && strchr(METRIC_IDENT_START, identifier[0]) != NULL);
    for (size_t i = 0; ok && i < len; i++) {
        if (!strchr(METRIC_IDENT_CHARS, identifier[i])) ok = 0;
    }
    if (!ok) {
        free(identifier);
        return strdup("unknown");
    }
    return identifier;
}

/* ===========================================================================
 *  skill_provenance  (shared_metrics_contract.py:skill_provenance)
 *
 *      return _allowlisted(_norm(value), SKILL_PROVENANCES)
 * =========================================================================== */

/* PoP: skill_provenance @ hermes_cli/observability/shared_metrics_contract.py:skill_provenance */
const char *smc_skill_provenance(const char *value)
{
    char *n = smc_norm(value);
    if (!n) return "unknown";
    const char *result = smc_allowlisted(n, skill_provenances, SKILL_PROVENANCES_N);
    free(n);
    return result;
}

/* `_norm(kwargs.get(key))`: Python's `str(value or "")` -- a FALSY value reads
 * as "", and a container stringifies as Python repr (single-quoted strings),
 * not as JSON. */
typedef struct {
    char *p;
    size_t n, cap;
} smc_sb_t;

static int sb_put(smc_sb_t *sb, const char *s, size_t n)
{
    if (sb->n + n + 1 > sb->cap) {
        size_t cap = sb->cap ? sb->cap : 64;
        while (cap < sb->n + n + 1) cap *= 2;
        char *p = (char *)realloc(sb->p, cap);
        if (!p) return 0;
        sb->p = p;
        sb->cap = cap;
    }
    memcpy(sb->p + sb->n, s, n);
    sb->n += n;
    sb->p[sb->n] = '\0';
    return 1;
}

static int sb_puts(smc_sb_t *sb, const char *s)
{
    return sb_put(sb, s, strlen(s));
}

static int py_repr(json_t *v, smc_sb_t *sb)
{
    switch (v->type) {
    case JSON_NULL:
        return sb_puts(sb, "None");
    case JSON_BOOL:
        return sb_puts(sb, v->bool_val ? "True" : "False");
    case JSON_NUMBER: {
        char buf[64];
        double d = v->num_val;
        if (isfinite(d) && d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof(buf), "%.0f", d);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", d);
        }
        return sb_puts(sb, buf);
    }
    case JSON_STRING:
        if (!sb_puts(sb, "'")) return 0;
        for (const char *p = v->str_val; p && *p; p++) {
            if (*p == '\'' || *p == '\\') {
                if (!sb_puts(sb, "\\")) return 0;
            }
            if (!sb_put(sb, p, 1)) return 0;
        }
        return sb_puts(sb, "'");
    case JSON_ARRAY:
        if (!sb_puts(sb, "[")) return 0;
        for (size_t i = 0; i < v->c.count; i++) {
            if (i && !sb_puts(sb, ", ")) return 0;
            if (!py_repr(v->c.items[i], sb)) return 0;
        }
        return sb_puts(sb, "]");
    case JSON_OBJECT:
        if (!sb_puts(sb, "{")) return 0;
        for (size_t i = 0; i < v->c.count; i++) {
            if (i && !sb_puts(sb, ", ")) return 0;
            if (!sb_puts(sb, "'")) return 0;
            for (const char *p = v->c.keys[i]; p && *p; p++) {
                if (*p == '\'' || *p == '\\') {
                    if (!sb_puts(sb, "\\")) return 0;
                }
                if (!sb_put(sb, p, 1)) return 0;
            }
            if (!sb_puts(sb, "': ")) return 0;
            if (!py_repr(v->c.items[i], sb)) return 0;
        }
        return sb_puts(sb, "}");
    }
    return 0;
}

static int json_falsy(const json_t *v)
{
    switch (v->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return !v->bool_val;
    case JSON_NUMBER: return v->num_val == 0;
    case JSON_STRING: return v->str_val == NULL || !*v->str_val;
    case JSON_ARRAY: return v->c.count == 0;
    case JSON_OBJECT: return v->c.count == 0;
    }
    return 1;
}

static char *smc_kw_str(json_t *kwargs, const char *key)
{
    json_t *v = (kwargs && kwargs->type == JSON_OBJECT)
                    ? json_obj_get(kwargs, key) : NULL;
    if (!v || json_falsy(v)) return smc_norm("");
    if (v->type == JSON_STRING) return smc_norm(v->str_val);
    smc_sb_t sb = {NULL, 0, 0};
    if (!py_repr(v, &sb)) {
        free(sb.p);
        return smc_norm("");
    }
    char *out = smc_norm(sb.p);
    free(sb.p);
    return out;
}

/* ===========================================================================
 *  tool_category  (shared_metrics_contract.py:tool_category)
 *
 *      _norm(tool_name).startswith(CONNECTOR_NAME_PREFIX) -> "connector"
 *      toolset = _norm(toolset);  empty -> "unknown"
 *      exact (identity for every category, plus aliases) -> that category
 *      first matching prefix rule -> its category
 *      else "other"
 * =========================================================================== */

/* PoP: tool_category @ hermes_cli/observability/shared_metrics_contract.py:tool_category */
const char *smc_tool_category(json_t *kwargs)
{
    char *tool_name = smc_kw_str(kwargs, "tool_name");
    if (tool_name && strncmp(tool_name, CONNECTOR_NAME_PREFIX,
                             strlen(CONNECTOR_NAME_PREFIX)) == 0) {
        free(tool_name);
        return "connector";
    }
    free(tool_name);

    char *toolset = smc_kw_str(kwargs, "toolset");
    if (!toolset || !*toolset) { free(toolset); return "unknown"; }

    const char *result = NULL;
    for (size_t i = 0; !result && i < TOOL_CATEGORY_EXACT_N; i++) {
        if (strcmp(toolset, tool_category_exact[i][0]) == 0) result = tool_category_exact[i][1];
    }
    for (size_t i = 0; !result && i < TOOL_CATEGORIES_N; i++) {
        if (strcmp(toolset, tool_categories[i]) == 0) result = tool_categories[i];
    }
    for (size_t i = 0; !result && i < TOOL_PREFIX_RULES_N; i++) {
        const smc_prefix_rule_t *rule = &tool_prefix_rules[i];
        for (size_t j = 0; j < rule->n; j++) {
            if (strncmp(toolset, rule->prefixes[j], strlen(rule->prefixes[j])) == 0) {
                result = rule->category;
                break;
            }
        }
    }
    free(toolset);
    return result ? result : "other";
}

/* ===========================================================================
 *  tool_outcome  (shared_metrics_contract.py:tool_outcome)
 *
 *      return _TOOL_STATUS_OUTCOMES.get(_norm(status), "unknown")
 * =========================================================================== */

/* PoP: tool_outcome @ hermes_cli/observability/shared_metrics_contract.py:tool_outcome */
const char *smc_tool_outcome(json_t *kwargs)
{
    char *status = smc_kw_str(kwargs, "status");
    if (!status) return "unknown";
    const char *result = "unknown";
    for (size_t i = 0; i < TOOL_STATUS_N; i++) {
        if (strcmp(status, tool_status_outcomes[i][0]) == 0) {
            result = tool_status_outcomes[i][1];
            break;
        }
    }
    free(status);
    return result;
}

/* ===========================================================================
 *  model_error_class  (shared_metrics_contract.py:model_error_class)
 *
 *      reason = _norm(reason)
 *      return reason if reason in MODEL_ERROR_CLASSES - {"none"} else "unknown"
 * =========================================================================== */

/* PoP: model_error_class @ hermes_cli/observability/shared_metrics_contract.py:model_error_class */
const char *smc_model_error_class(json_t *kwargs)
{
    char *reason = smc_kw_str(kwargs, "reason");
    if (!reason) return "unknown";
    const char *result = "unknown";
    for (size_t i = 0; i < MODEL_ERROR_CLASSES_N; i++) {
        if (strcmp(model_error_classes[i], "none") == 0) continue;
        if (strcmp(reason, model_error_classes[i]) == 0) {
            result = model_error_classes[i];
            break;
        }
    }
    free(reason);
    return result;
}
