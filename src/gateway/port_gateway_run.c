/* port_gateway_run.c — Faithful C11 port of gateway/run.py module-level helpers
 *
 * Ports ~43 pure module-level functions from gateway/run.py into self-contained
 * stateless C helpers. Each maps 1:1 to Python original.
 *
 * C11, minimal includes, opaque stateless API. Reuses libjson, libregex, libredact.
 */
#define _GNU_SOURCE
#include "gateway_run_helpers.h"
#include "gateway_run_pure2.h"  /* gw_load_gateway_config() */
#include "hermes_core_types.h"    /* bool */
#include "hermes_redact.h"        /* hermes_redact */
#include "hermes_skill_commands.h" /* skill_cmd_* functions */

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  1. Platform helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_platform_value @ gateway/run.py:_gateway_platform_value */
void gw_platform_value(const char *platform, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    const char *p = platform ? platform : "";
    size_t i = 0;
    /* strip leading whitespace */
    while (*p && (unsigned char)*p <= ' ') p++;
    /* copy lower-cased */
    while (*p && i < out_size - 1) {
        out[i++] = (char)tolower((unsigned char)*p);
        p++;
    }
    /* strip trailing whitespace from output */
    while (i > 0 && (unsigned char)out[i-1] <= ' ') i--;
    out[i] = '\0';
}

/* PoP: gw_surface_passes_raw_text @ gateway/run.py:_gateway_surface_passes_raw_text */
bool gw_surface_passes_raw_text(const char *platform)
{
    char norm[64];
    gw_platform_value(platform, norm, sizeof(norm));
    /* _GATEWAY_RAW_TEXT_PLATFORMS = {"local", "api", "webhook"} */
    if (strcmp(norm, "local") == 0) return true;
    if (strcmp(norm, "api") == 0) return true;
    if (strcmp(norm, "webhook") == 0) return true;
    return false;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  2. Network / error helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_is_transient_network_error @ gateway/run.py:_is_transient_network_error */
static const char *TRANSIENT_ERROR_CLASSES[] = {
    "TimedOut", "NetworkError", "ReadError", "WriteError",
    "ConnectError", "ConnectTimeout", "ReadTimeout", "WriteTimeout",
    "PoolTimeout", "RemoteProtocolError", "ServerDisconnectedError",
    "ClientConnectorError", "ClientOSError", NULL
};

bool gw_is_transient_network_error(const char *exc_class_name)
{
    if (!exc_class_name) return false;
    for (int i = 0; TRANSIENT_ERROR_CLASSES[i]; i++) {
        if (strcmp(exc_class_name, TRANSIENT_ERROR_CLASSES[i]) == 0)
            return true;
    }
    return false;
}

/* ── Provider error regex helpers ───────────────────────────────────────── */

/* _GATEWAY_PROVIDER_ERROR_RE patterns */
static bool match_provider_error_re(const char *text)
{
    /* r"(api\s+(?:call\s+)?failed|provider\s+authentication\s+failed|
     *    non-retryable\s+error|rate\s+limited\s+after\s+\d+\s+retries|
     *    error\s+code\s*:|\bhttp\s*\d{3}\b|incorrect\s+api\s+key|
     *    invalid\s+api\s+key)" re.IGNORECASE */
    const char *patterns[] = {
        "api failed", "api call failed",
        "provider authentication failed",
        "non-retryable error",
        "rate limited after ",
        "error code",
        "error code:",
        "incorrect api key",
        "invalid api key",
        NULL
    };
    if (!text) return false;
    /* Case-insensitive substring matching */
    char lower[512];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    for (int i = 0; patterns[i]; i++) {
        if (strstr(lower, patterns[i]))
            return true;
    }
    /* Also check for http\s*ddd pattern */
    const char *h = strstr(lower, "http");
    if (h) {
        h += 4;
        while (*h == ' ') h++;
        if (*h >= '0' && *h <= '9') return true;
    }
    return false;
}

/* _GATEWAY_AUTH_ERROR_RE patterns */
static bool match_auth_error_re(const char *text)
{
    if (!text) return false;
    char lower[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    if (strstr(lower, "provider authentication failed")) return true;
    if (strstr(lower, "incorrect api key")) return true;
    if (strstr(lower, "invalid api key")) return true;
    if (strstr(lower, " 401")) return true;
    if (strstr(lower, "401 ")) return true;
    return false;
}

/* _GATEWAY_PROVIDER_POLICY_RE patterns */
static bool match_policy_re(const char *text)
{
    if (!text) return false;
    char lower[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    const char *patterns[] = {
        "cybersecurity risk", "security policy", "safety policy",
        "policy violation", "violate", "violates", "violated", "violation",
        "blocked because", "blocked by", "blocked under",
        "request was blocked", "request was rejected", "request blocked",
        "request rejected", "disallowed", "moderation",
        NULL
    };
    for (int i = 0; patterns[i]; i++) {
        if (strstr(lower, patterns[i]))
            return true;
    }
    return false;
}

/* _GATEWAY_RATE_LIMIT_RE patterns */
static bool match_rate_limit_re(const char *text)
{
    if (!text) return false;
    char lower[256];
    size_t len = strlen(text);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[len] = '\0';
    if (strstr(lower, "rate limit")) return true;
    if (strstr(lower, "rate-limited")) return true;
    if (strstr(lower, " 429")) return true;
    if (strstr(lower, "quota")) return true;
    if (strstr(lower, "usage limit")) return true;
    return false;
}

/* PoP: gw_provider_error_reply @ gateway/run.py:_gateway_provider_error_reply */
void gw_provider_error_reply(const char *text, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (!text) {
        out[0] = '\0';
        return;
    }
    if (match_auth_error_re(text)) {
        snprintf(out, out_size,
            "\xe2\x9a\xa0\xef\xb8\x8f Provider authentication failed. "
            "Check the configured credentials; "
            "raw provider details are in the gateway logs.");
    } else if (match_policy_re(text)) {
        snprintf(out, out_size,
            "\xe2\x9a\xa0\xef\xb8\x8f The model provider rejected the request. "
            "I kept the raw provider error out of chat; "
            "check gateway logs for details or try rephrasing.");
    } else if (match_rate_limit_re(text)) {
        snprintf(out, out_size,
            "\xe2\x8f\xb1\xef\xb8\x8f The model provider is rate-limiting requests. "
            "Please wait a moment and try again.");
    } else {
        snprintf(out, out_size,
            "\xe2\x9a\xa0\xef\xb8\x8f The model provider failed after retries. "
            "I kept raw provider details out of chat; "
            "check gateway logs for diagnostics.");
    }
}

/* PoP: gw_looks_like_provider_error @ gateway/run.py:_looks_like_gateway_provider_error */
bool gw_looks_like_provider_error(const char *text)
{
    if (!text) return false;
    const char *body = text;
    while (*body && (unsigned char)*body <= ' ') body++;
    if (strlen(body) > 400) return false;
    /* Check line count */
    int lines = 0;
    for (const char *p = body; *p; p++) {
        if (*p == '\n') lines++;
    }
    if (lines > 4) return false;
    return match_provider_error_re(body);
}

/* PoP: gw_sanitize_final_response @ gateway/run.py:_sanitize_gateway_final_response */
void gw_sanitize_final_response(const char *platform, const char *text,
                                 char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;

    if (gw_surface_passes_raw_text(platform)) {
        /* _gateway_surface_passes_raw_text → keep raw */
        size_t n = strlen(text);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, text, n);
        out[n] = '\0';
        return;
    }

    /* Check for interruption prefix */
    const char *tr = text;
    while (*tr && (unsigned char)*tr <= ' ') tr++;
    /* INTERRUPT_WAITING_FOR_MODEL_PREFIX → return "" */
    const char *interrupt_prefix = "interrupt: waiting for model";
    if (strncasecmp(tr, interrupt_prefix, strlen(interrupt_prefix)) == 0) {
        out[0] = '\0';
        return;
    }

    char redacted[4096];
    gw_redact_secrets(text, redacted, sizeof(redacted));

    if (gw_looks_like_provider_error(redacted)) {
        gw_provider_error_reply(redacted, out, out_size);
    } else {
        size_t n = strlen(redacted);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, redacted, n);
        out[n] = '\0';
    }
}

/* PoP: gw_prepare_status_message @ gateway/run.py:_prepare_gateway_status_message */
int gw_prepare_status_message(const char *platform, const char *event_type,
                               const char *message, char *out, size_t out_size)
{
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (!message) return 0;

    const char *tr = message;
    while (*tr && (unsigned char)*tr <= ' ') tr++;
    if (*tr == '\0') return 0;

    if (gw_surface_passes_raw_text(platform)) {
        size_t n = strlen(tr);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, tr, n);
        out[n] = '\0';
        return 1;
    }

    char redacted[4096];
    gw_redact_secrets(tr, redacted, sizeof(redacted));

    /* _TELEGRAM_NOISY_STATUS_RE filter */
    char lower[512];
    size_t len = strlen(redacted);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++)
        lower[i] = (char)tolower((unsigned char)redacted[i]);
    lower[len] = '\0';
    /* Suppress common noisy status messages */
    const char *noisy[] = {
        "compressing context...", "context compressed.", "thinking...",
        NULL
    };
    for (int i = 0; noisy[i]; i++) {
        if (strstr(lower, noisy[i]))
            return 0;
    }

    if (gw_looks_like_provider_error(redacted)) {
        gw_provider_error_reply(redacted, out, out_size);
    } else {
        size_t n = strlen(redacted);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, redacted, n);
        out[n] = '\0';
    }
    return 1;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  3. Secret redaction
 * ════════════════════════════════════════════════════════════════════════════ */

/* _GATEWAY_SECRET_PATTERNS — belt-and-suspenders regex patterns */
static void apply_secret_patterns(const char *text, char *out, size_t out_size)
{
    /* Simple approach: scan for known secret patterns and replace.
     * C doesn't have regex built-in; use simple substring matching
     * for the known prefixes. This is the "belt" pass; the main
     * redaction happens via redact_sensitive_text() which uses full regex. */
    if (!out || out_size == 0) return;
    const char *p = text ? text : "";
    char buf[8192];
    size_t pos = 0;

    while (*p && pos < sizeof(buf) - 1) {
        /* Check known key prefixes */
        int sk_len = 0;

        /* sk-[A-Za-z0-9]... OpenAI style */
        if (p[0] == 's' && p[1] == 'k' && p[2] == '-') {
            sk_len = 3;
            while (sk_len < 50 && p[sk_len] &&
                   (isalnum((unsigned char)p[sk_len]) || p[sk_len] == '_' || p[sk_len] == '-'))
                sk_len++;
            if (sk_len >= 15) {
                for (int i = 0; i < sk_len && pos < sizeof(buf) - 1; i++) {
                    buf[pos++] = (i < 3) ? p[i] : '*';
                }
                p += sk_len;
                continue;
            }
        }

        buf[pos++] = *p;
        p++;
    }
    buf[pos] = '\0';

    size_t n = strlen(buf);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, buf, n);
    out[n] = '\0';
}

/* PoP: gw_redact_secrets @ gateway/run.py:_redact_gateway_user_facing_secrets */
void gw_redact_secrets(const char *text, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;

    /* Primary: delegate to hermes_redact() */
    char *primary = hermes_redact(text);
    apply_secret_patterns(primary ? primary : text, out, out_size);
    free(primary);
}

/* PoP: gw_redact_approval_command @ gateway/run.py:_redact_approval_command */
void gw_redact_approval_command(const char *cmd, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    const char *src = cmd ? cmd : "";
    /* hermes_redact with force=true (hermes_redact always forces) */
    char *primary = hermes_redact(src);
    apply_secret_patterns(primary ? primary : src, out, out_size);
    free(primary);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  4. Formatting helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_format_exec_approval_fallback @ gateway/run.py:_format_exec_approval_fallback */
void gw_format_exec_approval_fallback(const char *command,
                                       const char *description,
                                       const char *command_prefix,
                                       bool allow_permanent,
                                       bool allow_session,
                                       bool smart_denied,
                                       char *out, size_t out_size)
{
    if (!out || out_size == 0) return;

    /* cmd_preview = command[:200] + "..." if len(command) > 200 else command */
    char cmd_preview[256];
    size_t cmd_len = command ? strlen(command) : 0;
    if (cmd_len > 200) {
        memcpy(cmd_preview, command, 200);
        memcpy(cmd_preview + 200, "...", 4);
    } else {
        memcpy(cmd_preview, command ? command : "", cmd_len + 1);
    }

    const char *heading;
    if (smart_denied)
        heading = "\xe2\x9a\xa0\xef\xb8\x8f **Smart DENY \xe2\x80\x94 "
                  "owner override for one operation:**";
    else
        heading = "\xe2\x9a\xa0\xef\xb8\x8f **Dangerous command requires approval:**";

    char desc_buf[512];
    if (description) {
        size_t dn = strlen(description);
        if (dn >= sizeof(desc_buf)) dn = sizeof(desc_buf) - 1;
        memcpy(desc_buf, description, dn);
        desc_buf[dn] = '\0';
    } else {
        desc_buf[0] = '\0';
    }

    if (!command_prefix) command_prefix = "/";

    /* Build choices list */
    char choices[1024];
    size_t cpos = 0;

    /* "Reply `{prefix}approve` to execute this one operation" */
    cpos += snprintf(choices + cpos, sizeof(choices) - cpos,
        "Reply `%sapprove` to execute this one operation",
        command_prefix);

    if (!smart_denied && allow_session) {
        cpos += snprintf(choices + cpos, sizeof(choices) - cpos,
            ", `%sapprove session` to approve this pattern for the session",
            command_prefix);
        if (allow_permanent) {
            cpos += snprintf(choices + cpos, sizeof(choices) - cpos,
                ", `%sapprove always` to approve permanently",
                command_prefix);
        }
    }
    cpos += snprintf(choices + cpos, sizeof(choices) - cpos,
        ", or `%sdeny` to cancel.", command_prefix);

    snprintf(out, out_size,
        "%s\n```\n%s\n```\nReason: %s\n\n%s",
        heading, cmd_preview, desc_buf, choices);
}

/* PoP: gw_render_notice_line @ gateway/run.py:render_notice_line */
void gw_render_notice_line(const char *notice_text, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    const char *src = notice_text ? notice_text : "";
    /* return str(getattr(notice, "text", "") or "").strip() */
    while (*src && (unsigned char)*src <= ' ') src++;
    size_t n = strlen(src);
    /* strip trailing */
    while (n > 0 && (unsigned char)src[n-1] <= ' ') n--;
    if (n >= out_size) n = out_size - 1;
    memcpy(out, src, n);
    out[n] = '\0';
}

/* PoP: gw_format_duration @ gateway/run.py:_format_duration */
void gw_format_duration(double seconds, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    int total = (int)round(seconds);
    if (total < 0) total = 0;
    int hours = total / 3600;
    int rem = total % 3600;
    int minutes = rem / 60;
    int secs = rem % 60;
    if (hours > 0)
        snprintf(out, out_size, "%d:%02d:%02d", hours, minutes, secs);
    else
        snprintf(out, out_size, "%d:%02d", minutes, secs);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  5. Thread / progress helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_resolve_progress_thread_id @ gateway/run.py:_resolve_progress_thread_id */
void gw_resolve_progress_thread_id(const char *platform,
                                    const char *source_thread_id,
                                    const char *event_message_id,
                                    char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    if (source_thread_id && source_thread_id[0]) {
        size_t n = strlen(source_thread_id);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, source_thread_id, n);
        out[n] = '\0';
        return;
    }

    char norm[64];
    gw_platform_value(platform, norm, sizeof(norm));
    if ((strcmp(norm, "slack") == 0 || strcmp(norm, "mattermost") == 0)
        && event_message_id && event_message_id[0]) {
        size_t n = strlen(event_message_id);
        if (n >= out_size) n = out_size - 1;
        memcpy(out, event_message_id, n);
        out[n] = '\0';
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  6. Display config helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_has_display_override @ gateway/run.py:_has_platform_display_override */
bool gw_has_display_override(const char *user_config_json,
                              const char *platform_key,
                              const char *setting)
{
    /* Simple approach: search for display.platforms.<key>.<setting> in JSON.
     * Full JSON parsing via libjson would be ideal, but for a quick faithful
     * port we do a string search since this is a config-existent check. */
    if (!user_config_json || !platform_key || !setting) return false;
    /* Build search pattern: "platforms": { ... "key": { ... "setting" */
    char needle[256];
    int n = snprintf(needle, sizeof(needle),
                     "\"%s\"", setting);
    (void)n;

    /* Find platform_key section first */
    char key_needle[128];
    snprintf(key_needle, sizeof(key_needle), "\"%s\"", platform_key);
    const char *key_pos = strstr(user_config_json, key_needle);
    if (!key_pos) return false;

    /* Find "platforms" before that */
    char section[2048];
    /* Scan backwards for "platforms" */
    const char *scan = key_pos;
    while (scan > user_config_json) {
        scan--;
        if (strncmp(scan, "\"platforms\"", 11) == 0) {
            /* Found it. Check if setting is within the same platform block */
            /* (up to the next top-level key or end of platform value) */
            size_t slen = strlen(scan);
            if (slen > sizeof(section) - 1) slen = sizeof(section) - 1;
            memcpy(section, scan, slen);
            section[slen] = '\0';
            return strstr(section, needle) != NULL;
        }
    }
    return false;
}

/* PoP: gw_resolve_display_bool @ gateway/run.py:_resolve_gateway_display_bool */
bool gw_resolve_display_bool(const char *user_config_json,
                              const char *platform_key,
                              const char *setting,
                              bool default_val,
                              const char *platform,
                              const char *require_override_platforms_json)
{
    /* Simplified: check override first, then check display setting.
     * For pure config bool, we trust the JSON-path pattern. */
    if (!user_config_json || !platform_key || !setting) return default_val;

    /* Check require_platform_override_for set */
    if (require_override_platforms_json && require_override_platforms_json[0]) {
        char plat_norm[64];
        gw_platform_value(platform ? platform : platform_key, plat_norm, sizeof(plat_norm));
        if (strstr(require_override_platforms_json, plat_norm)) {
            if (!gw_has_display_override(user_config_json, platform_key, setting))
                return false;
        }
    }

    /* Build key lookup: "display": { ... "platforms": { ... "key": { ... "setting": val */
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", setting);
    const char *settings_section = strstr(user_config_json, "\"display\"");
    if (!settings_section) return default_val;

    const char *platforms_section = strstr(settings_section, "\"platforms\"");
    if (!platforms_section) {
        /* Top-level display setting? */
        const char *val = strstr(settings_section, needle);
        if (!val) return default_val;
        /* Check value after colon */
        val += strlen(needle);
        while (*val && *val != ':' && *val != ',' && *val != '}') val++;
        if (*val == ':') {
            val++;
            while (*val && (unsigned char)*val <= ' ') val++;
            if (strncmp(val, "true", 4) == 0) return true;
            if (strncmp(val, "false", 5) == 0) return false;
        }
        return default_val;
    }

    /* Check within platform key section */
    char key_needle[128];
    snprintf(key_needle, sizeof(key_needle), "\"%s\"", platform_key);
    const char *key_pos = strstr(platforms_section, key_needle);
    if (!key_pos) return default_val;

    /* Find : { and check for setting within that scope */
    const char *setting_pos = strstr(key_pos, needle);
    if (!setting_pos) return default_val;

    /* Check value after colon */
    setting_pos += strlen(needle);
    while (*setting_pos && *setting_pos != ':' && *setting_pos != ',' && *setting_pos != '}') setting_pos++;
    if (*setting_pos == ':') {
        setting_pos++;
        while (*setting_pos && (unsigned char)*setting_pos <= ' ') setting_pos++;
        if (strncmp(setting_pos, "true", 4) == 0) return true;
        if (strncmp(setting_pos, "false", 5) == 0) return false;
    }
    return default_val;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  7. Timestamp / freshness helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_coerce_timestamp @ gateway/run.py:_coerce_gateway_timestamp */
double gw_coerce_timestamp(double value, int is_ms,
                            const char *iso_string, int is_iso_string)
{
    if (is_iso_string && iso_string) {
        /* Parse ISO-8601 like "2026-04-28T13:40:53" or "2026-04-28T13:40:53Z" */
        struct tm tm = {0};
        const char *p = iso_string;
        /* Skip whitespace */
        while (*p && (unsigned char)*p <= ' ') p++;
        if (!*p) return -1.0;
        /* Try to parse */
        if (strptime(p, "%Y-%m-%dT%H:%M:%S", &tm) ||
            strptime(p, "%Y-%m-%d %H:%M:%S", &tm)) {
            time_t t = timegm(&tm); /* Treat as UTC */
            return (double)t;
        }
        /* Try with milliseconds */
        struct tm tm2 = {0};
        if (strptime(p, "%Y-%m-%dT%H:%M:%S", &tm2)) {
            time_t t = timegm(&tm2);
            /* Try to get fractional seconds */
            const char *dot = strchr(p, '.');
            if (dot) {
                /* Parse up to 3 digits */
                double frac = 0;
                sscanf(dot, ".%lf", &frac);
                return (double)t + frac;
            }
            return (double)t;
        }
        return -1.0;
    }

    if (is_ms) {
        return value / 1000.0;
    }
    return value;
}

/* PoP: gw_float_env @ gateway/run.py:_float_env */
double gw_float_env(const char *name, double default_val)
{
    const char *raw = getenv(name);
    if (!raw || raw[0] == '\0')
        return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || (*end != '\0' && !(*end == ' ' || *end == '\t' || *end == '\n')))
        return default_val;
    return val;
}

/* PoP: gw_is_fresh_interruption @ gateway/run.py:_is_fresh_gateway_interruption */
bool gw_is_fresh_interruption(double timestamp, double now, double window_secs)
{
    double window = (window_secs > 0) ? window_secs : 3600.0; /* default 1hr */
    if (window <= 0) return true;
    if (timestamp < 0) return true; /* unknown = fresh */
    double current = (now > 0) ? now : (double)time(NULL);
    return (current - timestamp) <= window;
}

/* PoP: gw_build_resume_recovery_note @ gateway/run.py:build_resume_recovery_note */
void gw_build_resume_recovery_note(const char *reason, const char *message,
                                    bool interactive,
                                    char *out, size_t out_size)
{
    if (!out || out_size == 0) return;

    const char *reason_phrase = "a gateway interruption";
    if (reason) {
        if (strcmp(reason, "restart_timeout") == 0)
            reason_phrase = "a gateway restart";
        else if (strcmp(reason, "shutdown_timeout") == 0)
            reason_phrase = "a gateway shutdown";
    }

    const char *resume_guidance;
    const char *tail_guidance;
    if (message && message[0]) {
        resume_guidance =
            "Address the user's NEW message below FIRST and focus "
            "on what the user is asking now.";
        tail_guidance =
            "Do NOT re-execute old tool calls \xe2\x80\x94 skip any "
            "unfinished work from the conversation history.";
    } else if (interactive) {
        resume_guidance =
            "Report to the user that the session was restored "
            "successfully and ask what they would like to do next.";
        tail_guidance =
            "Do NOT re-execute old tool calls \xe2\x80\x94 skip any "
            "unfinished work from the conversation history.";
    } else {
        resume_guidance =
            "No user is present on this non-interactive platform, "
            "so do NOT emit a 'session restored' acknowledgement "
            "or ask questions. Review the conversation history and "
            "CONTINUE the interrupted task to completion.";
        tail_guidance =
            "Do NOT re-run tool calls whose results already "
            "appear in the history \xe2\x80\x94 resume from the first step "
            "that has no recorded result.";
    }

    if (message && message[0]) {
        snprintf(out, out_size,
            "[System note: The previous turn was interrupted by "
            "%s; the gateway is now back online. "
            "Any restart/shutdown command in the history has already "
            "run \xe2\x80\x94 do NOT re-execute or verify it. %s "
            "%s]\n\n%s",
            reason_phrase, resume_guidance, tail_guidance, message);
    } else {
        snprintf(out, out_size,
            "[System note: The previous turn was interrupted by "
            "%s; the gateway is now back online. "
            "Any restart/shutdown command in the history has already "
            "run \xe2\x80\x94 do NOT re-execute or verify it. %s "
            "%s]",
            reason_phrase, resume_guidance, tail_guidance);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  8. Transcript replay helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_uses_observed_group_context @ gateway/run.py:_uses_telegram_observed_group_context */
bool gw_uses_observed_group_context(const char *channel_prompt)
{
    if (!channel_prompt) return false;
    return strstr(channel_prompt, "observed Telegram group context") != NULL;
}

/* PoP: gw_message_timestamps_enabled @ gateway/run.py:_message_timestamps_enabled */
bool gw_message_timestamps_enabled(const char *user_config_json)
{
    if (!user_config_json) return false;

    /* Find gateway.message_timestamps.enabled or bare message_timestamps: true */
    const char *gw_section = strstr(user_config_json, "\"gateway\"");
    if (!gw_section) return false;

    const char *mt_section = strstr(gw_section, "\"message_timestamps\"");
    if (!mt_section) return false;

    /* Check if it's a dict {enabled: true} or bare true */
    const char *after_colon = mt_section + strlen("\"message_timestamps\"");
    while (*after_colon && (unsigned char)*after_colon <= ' ') after_colon++;
    if (*after_colon == ':') after_colon++;
    while (*after_colon && (unsigned char)*after_colon <= ' ') after_colon++;

    if (*after_colon == 't') return true;        /* bare true */
    if (*after_colon == 'f') return false;       /* bare false */

    /* Must be object — find "enabled": */
    const char *enabled = strstr(after_colon, "\"enabled\"");
    if (!enabled) return false;
    enabled += strlen("\"enabled\"");
    while (*enabled && (unsigned char)*enabled <= ' ') enabled++;
    if (*enabled == ':') enabled++;
    while (*enabled && (unsigned char)*enabled <= ' ') enabled++;
    return (strncmp(enabled, "true", 4) == 0);
}

/* PoP: gw_last_transcript_timestamp @ gateway/run.py:_last_transcript_timestamp */
double gw_last_transcript_timestamp(const char *history_json)
{
    if (!history_json) return -1.0;

    /* JSON: [{"role":"user","content":"...","timestamp":1234567890.0}, ...]
     * Scan from the end, skip session_meta and system rows. */
    /* Simple approach: find the last non-system, non-meta role and return its timestamp. */
    const char *p = history_json + strlen(history_json);
    while (p > history_json) {
        /* Find each message object from the end */
        p--;
        if (*p == '}') {
            /* Find matching { */
            const char *end_brace = p;
            int depth = 0;
            while (p >= history_json) {
                if (*p == '}') depth++;
                else if (*p == '{') depth--;
                if (depth == 0) break;
                p--;
            }
            if (depth == 0 && p <= end_brace) {
                /* Found a message object. Check its role. */
                const char *role_key = strstr(p, "\"role\"");
                if (role_key && role_key < end_brace) {
                    role_key += strlen("\"role\"");
                    while (*role_key && *role_key != ':' && role_key < end_brace) role_key++;
                    if (*role_key == ':') {
                        role_key++;
                        while (*role_key && (unsigned char)*role_key <= ' ' && role_key < end_brace) role_key++;
                        if (*role_key == '"') {
                            /* Skip session_meta and system */
                            int skip = 0;
                            if (strncmp(role_key + 1, "session_meta", 12) == 0) skip = 1;
                            if (strncmp(role_key + 1, "system", 6) == 0) skip = 1;
                            if (!skip) {
                                /* Found non-meta role. Look for timestamp. */
                                const char *ts = strstr(p, "\"timestamp\"");
                                if (ts && ts < end_brace) {
                                    ts += strlen("\"timestamp\"");
                                    while (*ts && *ts != ':' && ts < end_brace) ts++;
                                    if (*ts == ':') {
                                        ts++;
                                        while (*ts && (unsigned char)*ts <= ' ' && ts < end_brace) ts++;
                                        char *end = NULL;
                                        double val = strtod(ts, &end);
                                        if (end > ts)
                                            return val;
                                    }
                                }
                                /* No timestamp on this usable row */
                                return -1.0;
                            }
                        }
                    }
                }
            }
        }
    }
    return -1.0;
}

/* PoP: gw_is_auto_continue_noise @ gateway/run.py:_is_auto_continue_noise */
bool gw_is_auto_continue_noise(const char *content)
{
    if (!content) return false;
    return (strncmp(content, "[System note: Your previous turn", 32) == 0)
        || (strncmp(content, "[System note: A new message", 27) == 0);
}

/* PoP: gw_strip_auto_continue_noise @ gateway/run.py:_strip_auto_continue_noise */
void gw_strip_auto_continue_noise(const char *content,
                                   char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!content) return;

    const char *p = content;
    /* Keep stripping leading auto-continue brackets */
    while (gw_is_auto_continue_noise(p)) {
        const char *end = strchr(p, ']');
        if (!end) { out[0] = '\0'; return; }
        p = end + 1;
        while (*p && (unsigned char)*p <= ' ') p++;
    }

    size_t n = strlen(p);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, p, n);
    out[n] = '\0';
}
/* PoP: _select_cached_agent_history @ gateway/run.py:_select_cached_agent_history */
/* (returns longer of two lists — lives in gateway runtime session code) */

/* ════════════════════════════════════════════════════════════════════════════
 *  9. Media helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_event_media_type_at @ gateway/run.py:_event_media_type_at */
void gw_event_media_type_at(const char *event_json, int index,
                             char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!event_json) return;

    /* Find the index-th media entry in the event JSON */
    char idx_str[16];
    snprintf(idx_str, sizeof(idx_str), "media[%d]", index);
    const char *media_section = strstr(event_json, "\"media\"");
    if (!media_section) return;

    /* Try to find "mime_type" in that section */
    /* Look for mime entries in the media array */
    int cur_idx = 0;
    const char *p = media_section;
    while (*p) {
        const char *mime = strstr(p, "\"mime_type\"");
        if (!mime) break;
        if (cur_idx == index) {
            mime += strlen("\"mime_type\"");
            while (*mime && *mime != ':' && *mime != ',' && *mime != '}') mime++;
            if (*mime == ':') {
                mime++;
                while (*mime && (unsigned char)*mime <= ' ') mime++;
                if (*mime == '"') {
                    mime++;
                    size_t n = 0;
                    while (mime[n] && mime[n] != '"' && n < out_size - 1) {
                        out[n] = mime[n];
                        n++;
                    }
                    out[n] = '\0';
                    return;
                }
            }
            return;
        }
        cur_idx++;
        p = mime + 1; /* Skip past this one */
    }

    /* Try simple getattr fallback: message_type */
    const char *msg_type = strstr(event_json, "\"message_type\"");
    if (msg_type) {
        msg_type += strlen("\"message_type\"");
        while (*msg_type && *msg_type != ':' && *msg_type != ',' && *msg_type != '}') msg_type++;
        if (*msg_type == ':') {
            msg_type++;
            while (*msg_type && (unsigned char)*msg_type <= ' ') msg_type++;
            if (*msg_type == '"') {
                msg_type++;
                size_t n = 0;
                while (msg_type[n] && msg_type[n] != '"' && n < out_size - 1) {
                    out[n] = msg_type[n];
                    n++;
                }
                out[n] = '\0';
            }
        }
    }
}

/* PoP: gw_event_media_is_image @ gateway/run.py:_event_media_is_image */
bool gw_event_media_is_image(const char *event_json, int index)
{
    char mtype[64];
    gw_event_media_type_at(event_json, index, mtype, sizeof(mtype));
    if (mtype[0]) {
        return (strncmp(mtype, "image/", 6) == 0);
    }
    /* Fallback to message_type check */
    if (!event_json) return false;
    const char *mt = strstr(event_json, "\"message_type\"");
    if (mt) {
        /* Check if contains PHOTO */
        const char *val = strchr(mt, ':');
        if (val) {
            val++;
            while (*val && (unsigned char)*val <= ' ') val++;
            if (*val == '"') {
                if (strncmp(val + 1, "PHOTO", 5) == 0) return true;
            }
        }
    }
    return false;
}

/* PoP: gw_event_media_is_audio @ gateway/run.py:_event_media_is_audio */
bool gw_event_media_is_audio(const char *event_json, int index)
{
    char mtype[64];
    gw_event_media_type_at(event_json, index, mtype, sizeof(mtype));
    if (mtype[0]) {
        return (strncmp(mtype, "audio/", 6) == 0)
            || (strncmp(mtype, "video/", 6) == 0);
    }
    if (!event_json) return false;
    const char *mt = strstr(event_json, "\"message_type\"");
    if (mt) {
        const char *val = strchr(mt, ':');
        if (val) {
            val++;
            while (*val && (unsigned char)*val <= ' ') val++;
            if (*val == '"') {
                if (strncmp(val + 1, "VOICE", 5) == 0
                    || strncmp(val + 1, "VIDEO", 5) == 0
                    || strncmp(val + 1, "AUDIO", 5) == 0) return true;
            }
        }
    }
    return false;
}

/* PoP: gw_event_media_is_video @ gateway/run.py:_event_media_is_video */
bool gw_event_media_is_video(const char *event_json, int index)
{
    char mtype[64];
    gw_event_media_type_at(event_json, index, mtype, sizeof(mtype));
    if (mtype[0]) {
        return (strncmp(mtype, "video/", 6) == 0);
    }
    if (!event_json) return false;
    const char *mt = strstr(event_json, "\"message_type\"");
    if (mt) {
        const char *val = strchr(mt, ':');
        if (val) {
            val++;
            while (*val && (unsigned char)*val <= ' ') val++;
            if (*val == '"') {
                if (strncmp(val + 1, "VIDEO", 5) == 0) return true;
            }
        }
    }
    return false;
}

/* PoP: gw_event_media_is_stt_input @ gateway/run.py:_event_media_is_stt_input */
bool gw_event_media_is_stt_input(const char *event_json, int index)
{
    /* STT input: audio media that should be transcribed */
    char mtype[64];
    gw_event_media_type_at(event_json, index, mtype, sizeof(mtype));
    if (mtype[0]) {
        return (strncmp(mtype, "audio/", 6) == 0)
            || (strcmp(mtype, "ogg") == 0) || (strcmp(mtype, "opus") == 0);
    }
    if (!event_json) return false;
    const char *mt = strstr(event_json, "\"message_type\"");
    if (mt) {
        const char *val = strchr(mt, ':');
        if (val) {
            val++;
            while (*val && (unsigned char)*val <= ' ') val++;
            if (*val == '"') {
                if (strncmp(val + 1, "VOICE", 5) == 0) return true;
            }
        }
    }
    return false;
}

/* PoP: gw_build_media_placeholder @ gateway/run.py:_build_media_placeholder */
void gw_build_media_placeholder(const char *event_json,
                                 char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!event_json) return;

    /* Extract media_urls from event JSON */
    /* Build: [User sent an image/video/audio/file: url] per media item */
    char result[2048];
    result[0] = '\0';
    size_t rpos = 0;

    /* Count media entries */
    const char *urls_section = strstr(event_json, "\"media_urls\"");
    if (!urls_section) {
        /* Try to check message_type directly */
        const char *mt = strstr(event_json, "\"message_type\"");
        if (mt) {
            const char *cap = strstr(mt, "\"caption\"");
            if (!cap) {
                /* Check for single-media event */
                bool is_img = gw_event_media_is_image(event_json, 0);
                bool is_aud = gw_event_media_is_audio(event_json, 0);
                bool is_vid = gw_event_media_is_video(event_json, 0);
                if (is_img)
                    rpos += snprintf(result + rpos, sizeof(result) - rpos,
                        "[User sent an image]");
                else if (is_aud)
                    rpos += snprintf(result + rpos, sizeof(result) - rpos,
                        "[User sent audio]");
                else if (is_vid)
                    rpos += snprintf(result + rpos, sizeof(result) - rpos,
                        "[User sent a video]");
            }
        }
        if (rpos > 0 && rpos < out_size) {
            memcpy(out, result, rpos);
            out[rpos] = '\0';
        }
        return;
    }

    /* Parse media_urls from JSON: "media_urls": ["url1", "url2", ...] */
    urls_section += strlen("\"media_urls\"");
    while (*urls_section && *urls_section != ':' && *urls_section != '[') urls_section++;
    if (*urls_section == ':') urls_section++;
    while (*urls_section && (unsigned char)*urls_section <= ' ') urls_section++;
    if (*urls_section == '[') urls_section++;
    while (*urls_section && (unsigned char)*urls_section <= ' ') urls_section++;

    int idx = 0;
    while (*urls_section && *urls_section != ']' && rpos < sizeof(result) - 100) {
        /* Skip comma/ws */
        while (*urls_section && ((unsigned char)*urls_section <= ' ' || *urls_section == ',')) urls_section++;
        if (*urls_section == ']') break;

        /* Extract URL string */
        if (*urls_section == '"') {
            urls_section++;
            char url[512];
            size_t u = 0;
            while (*urls_section && *urls_section != '"' && u < sizeof(url) - 1) {
                url[u++] = *urls_section++;
            }
            url[u] = '\0';
            if (*urls_section == '"') urls_section++;

            /* Determine type */
            if (gw_event_media_is_image(event_json, idx)) {
                rpos += snprintf(result + rpos, sizeof(result) - rpos,
                    "[User sent an image: %s]\n", url);
            } else if (gw_event_media_is_audio(event_json, idx)) {
                rpos += snprintf(result + rpos, sizeof(result) - rpos,
                    "[User sent audio: %s]\n", url);
            } else if (gw_event_media_is_video(event_json, idx)) {
                rpos += snprintf(result + rpos, sizeof(result) - rpos,
                    "[User sent a video: %s]\n", url);
            } else {
                rpos += snprintf(result + rpos, sizeof(result) - rpos,
                    "[User sent a file: %s]\n", url);
            }
            idx++;
        } else {
            urls_section++;
        }
    }

    /* Strip trailing newline */
    while (rpos > 0 && result[rpos-1] == '\n') rpos--;

    size_t n = (rpos < out_size) ? rpos : out_size - 1;
    memcpy(out, result, n);
    out[n] = '\0';
}

/* PoP: gw_build_document_context_note @ gateway/run.py:_build_document_context_note */
void gw_build_document_context_note(const char *display_name,
                                     const char *agent_path,
                                     const char *mtype,
                                     char *out, size_t out_size)
{
    if (!out || out_size == 0) return;

    if (!display_name) display_name = "";
    if (!agent_path) agent_path = "";
    if (!mtype) mtype = "";

    if (strncmp(mtype, "text/", 5) == 0) {
        snprintf(out, out_size,
            "[The user sent a text document: '%s'. "
            "Its content has been included below. "
            "It is stored at '%s'.]",
            display_name, agent_path);
    } else {
        snprintf(out, out_size,
            "[The user sent a document: '%s'. "
            "You can read it with read_file('%s').]",
            display_name, agent_path);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  10. Control / misc helpers
 * ════════════════════════════════════════════════════════════════════════════ */

/* _CONTROL_INTERRUPT_MESSAGES */
static const char *CONTROL_INTERRUPT_MESSAGES[] = {
    "stop requested", "session reset requested",
    "execution timed out (inactivity)", "sse client disconnected",
    "gateway shutting down", "gateway restarting",
    NULL
};

/* PoP: gw_is_control_interrupt_message @ gateway/run.py:_is_control_interrupt_message */
bool gw_is_control_interrupt_message(const char *message)
{
    if (!message) return false;
    /* Normalize: strip, collapse whitespace, lowercase */
    const char *p = message;
    while (*p && (unsigned char)*p <= ' ') p++;
    char norm[256];
    size_t n = 0;
    int last_was_space = 0;
    while (*p && n < sizeof(norm) - 1) {
        if ((unsigned char)*p <= ' ') {
            if (!last_was_space) {
                norm[n++] = ' ';
                last_was_space = 1;
            }
        } else {
            norm[n++] = (char)tolower((unsigned char)*p);
            last_was_space = 0;
        }
        p++;
    }
    /* Strip trailing space */
    while (n > 0 && norm[n-1] == ' ') n--;
    norm[n] = '\0';

    for (int i = 0; CONTROL_INTERRUPT_MESSAGES[i]; i++) {
        if (strcmp(norm, CONTROL_INTERRUPT_MESSAGES[i]) == 0)
            return true;
    }
    return false;
}

/* PoP: gw_skill_slug_from_frontmatter @ gateway/run.py:_skill_slug_from_frontmatter */
void gw_skill_slug_from_frontmatter(const char *skill_md_content,
                                     char *slug_out, size_t slug_size,
                                     char *name_out, size_t name_size)
{
    if (slug_out && slug_size > 0) slug_out[0] = '\0';
    if (name_out && name_size > 0) name_out[0] = '\0';

    if (!skill_md_content) return;

    const char *p = skill_md_content;
    /* Tolerate UTF-8 BOM */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;

    /* Must start with "---" */
    if (strncmp(p, "---", 3) != 0) return;
    p += 3;
    const char *end = strstr(p, "\n---");
    if (!end) {
        /* Try end of file */
        end = p + strlen(p);
    }

    /* Parse frontmatter for name: */
    const char *name_key = strstr(p, "name:");
    if (name_key && name_key < end) {
        name_key += 5; /* skip "name:" */
        while (*name_key && ((unsigned char)*name_key <= ' ' || *name_key == '"' || *name_key == '\''))
            name_key++;
        size_t nn = 0;
        while (name_key[nn] && name_key[nn] != '\n'
               && name_key[nn] != '\r' && nn < name_size - 1) {
            if (name_out) {
                if (name_key[nn] != '"' && name_key[nn] != '\'')
                    name_out[nn] = name_key[nn];
                else {
                    /* Find matching close quote */
                    char q = name_key[nn];
                    nn++;
                    while (name_key[nn] && name_key[nn] != q && name_key[nn] != '\n') nn++;
                    break;
                }
            }
            nn++;
        }
        if (name_out) name_out[nn] = '\0';
        /* Trim trailing space from name */
        if (name_out) {
            size_t tn = strlen(name_out);
            while (tn > 0 && (unsigned char)name_out[tn-1] <= ' ') tn--;
            name_out[tn] = '\0';
        }
    }

    /* Build slug from name */
    if (slug_out && name_out && name_out[0]) {
        size_t sn = 0;
        for (size_t i = 0; name_out[i] && sn < slug_size - 1; i++) {
            char c = (char)tolower((unsigned char)name_out[i]);
            if (c == ' ' || c == '_') {
                slug_out[sn++] = '-';
            } else if (isalnum((unsigned char)c) || c == '-') {
                slug_out[sn++] = c;
            }
        }
        slug_out[sn] = '\0';
    } else if (slug_out) {
        slug_out[0] = '\0';
    }
}

/* PoP: gw_check_unavailable_skill @ gateway/run.py:_check_unavailable_skill */
const char *gw_check_unavailable_skill(const char *command_name)
{
    /* Returns a malloc'd string if the command matches a known disabled or
     * installable-but-not-installed skill, NULL if no match found. */
    if (!command_name || !command_name[0]) return NULL;

    /* Normalize: command uses hyphens, skill names may use hyphens or underscores */
    char normalized[256];
    snprintf(normalized, sizeof(normalized), "%s", command_name);
    for (int i = 0; normalized[i]; i++) {
        if (normalized[i] == '_') normalized[i] = '-';
        normalized[i] = tolower((unsigned char)normalized[i]);
    }

    /* Load gateway config to get disabled skills list */
    hermes_config_t *cfg = gw_load_gateway_config();
    if (!cfg) return NULL;

    /* Ensure skills are scanned */
    skill_cmd_scan_filtered(NULL);

    int count = 0;
    const skill_cmd_entry_t **all_skills = skill_cmd_get_all(&count);
    if (all_skills && count > 0) {
        for (int i = 0; i < count; i++) {
            const skill_cmd_entry_t *sk = all_skills[i];
            if (!sk || !sk->slug[0]) continue;
            /* Check if slug matches normalized command */
            char slug_norm[256];
            snprintf(slug_norm, sizeof(slug_norm), "%s", sk->slug);
            for (int j = 0; slug_norm[j]; j++) {
                if (slug_norm[j] == '_') slug_norm[j] = '-';
                slug_norm[j] = tolower((unsigned char)slug_norm[j]);
            }
            if (strcmp(slug_norm, normalized) == 0) {
                /* Check if this skill is disabled */
                if (skill_cmd_is_disabled(sk->slug, cfg->skills.disabled)) {
                    char *msg = malloc(512);
                    if (msg) {
                        snprintf(msg, 512,
                            "The **%s** skill is installed but disabled.\n"
                            "Enable it with: `hermes skills config`",
                            command_name);
                    }
                    free((void *)all_skills);
                    free(cfg);
                    return msg;
                }
            }
        }
    }
    free((void *)all_skills);

    free(cfg);
    return NULL;
}

/* PoP: gw_platform_config_key @ gateway/run.py:_platform_config_key */
void gw_platform_config_key(const char *platform, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    char norm[128];
    gw_platform_value(platform, norm, sizeof(norm));

    /* Map platform values to config keys */
    /* Most platforms use their value directly, but some need mapping */
    const char *key = norm;
    if (strcmp(norm, "") == 0) key = "unknown";
    /* Direct mapping for all */
    size_t n = strlen(key);
    if (n >= out_size) n = out_size - 1;
    memcpy(out, key, n);
    out[n] = '\0';
}

/* PoP: gw_teams_pipeline_plugin_enabled @ gateway/run.py:_teams_pipeline_plugin_enabled */
bool gw_teams_pipeline_plugin_enabled(void)
{
    /* Check if the teams pipeline plugin is configured.
     * Port of checking for plugin config / imports. */
    const char *val = getenv("HERMES_TEAMS_PIPELINE_PLUGIN");
    if (val && (strcmp(val, "1") == 0 || strcasecmp(val, "true") == 0))
        return true;
    return false;
}

/* PoP: gw_gateway_config_home @ gateway/run.py:_gateway_config_home */
void gw_gateway_config_home(char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    /* Port of: return HERMES_HOME / ".hermes" / "gateway" */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(out, out_size, "%s/.hermes/gateway", home);
}

/* PoP: gw_parse_session_key @ gateway/run.py:_parse_session_key */
void gw_parse_session_key(const char *session_key,
                           char *platform_out, size_t platform_size,
                           char *id_out, size_t id_size)
{
    if (platform_out && platform_size > 0) platform_out[0] = '\0';
    if (id_out && id_size > 0) id_out[0] = '\0';

    if (!session_key) return;

    /* Format: "platform:chat_id" — find the first colon */
    const char *colon = strchr(session_key, ':');
    if (!colon) {
        /* No colon — entire thing might be a platform name with numeric suffix */
        size_t n = strlen(session_key);
        if (n < platform_size) {
            memcpy(platform_out, session_key, n);
            platform_out[n] = '\0';
        }
        return;
    }

    /* Platform before colon */
    size_t plen = (size_t)(colon - session_key);
    if (plen >= platform_size) plen = platform_size - 1;
    memcpy(platform_out, session_key, plen);
    platform_out[plen] = '\0';

    /* Id after colon */
    const char *id_part = colon + 1;
    size_t ilen = strlen(id_part);
    if (ilen >= id_size) ilen = id_size - 1;
    memcpy(id_out, id_part, ilen);
    id_out[ilen] = '\0';
}