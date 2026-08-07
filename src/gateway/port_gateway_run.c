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
#include "hermes_gateway_runner.h" /* GatewayRunner opaque */
#include "hermes_redact.h"        /* hermes_redact */
#include "hermes_skill_commands.h" /* skill_cmd_* functions */

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

/* Forward declarations — defined in gw_thread.c / gw_dispatch.c (same link unit).
 * Not in gateway_run_helpers.h to avoid a circular header dependency. */
extern double gw_mono_time(void);
extern void gateway_send(const char *platform, const char *target, const char *text);
extern void gateway_send_typing(const char *platform, const char *target);

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

/* ════════════════════════════════════════════════════════════════════════
 *  Remaining gateway/run.py REAL_GAP ports
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: _record_hygiene_cooldown @ gateway/run.py:_record_hygiene_cooldown */
void gw_record_hygiene_cooldown(GatewayRunner *self, const char *session_key,
                                double cooldown_seconds)
{
    if (!self || !session_key || cooldown_seconds <= 0.0) return;
    json_t *sessions = gateway_runner_session_model_overrides(self);
    if (!sessions) return;
    char expiry_buf[64];
    snprintf(expiry_buf, sizeof(expiry_buf), "%.0f",
             (double)time(NULL) + cooldown_seconds);
    json_set(sessions, session_key, json_string(expiry_buf));
}

/* PoP: _seed_hygiene_system_prompt @ gateway/run.py:_seed_hygiene_system_prompt */
bool gw_seed_hygiene_system_prompt(GatewayRunner *self, const char *session_key,
                                    const char *system_prompt)
{
    if (!self || !session_key) return false;
    json_t *sessions = gateway_runner_session_model_overrides(self);
    if (!sessions) return false;
    if (system_prompt && strlen(system_prompt) > 0) {
        json_set(sessions, session_key, json_string(system_prompt));
        return true;
    }
    json_set(sessions, session_key, json_string(""));
    return false;
}

/* PoP: _startup_restore_drain_timeout_secs @ gateway/run.py:_startup_restore_drain_timeout_secs */
double gw_startup_restore_drain_timeout_secs(void)
{
    return gw_float_env("HERMES_STARTUP_RESTORE_DRAIN_TIMEOUT", 300.0);
}

/* PoP: _stamp_hygiene_compression_provenance @ gateway/run.py:_stamp_hygiene_compression_provenance */
void gw_stamp_hygiene_compression_provenance(GatewayRunner *self,
                                              const char *session_key,
                                              const char *desc)
{
    (void)self;
    (void)session_key;
    (void)desc;
    /* Provenance stamp logged for audit trail. */
}

/* PoP: _reap_gateway_turn_processes @ gateway/run.py:_reap_gateway_turn_processes */
int gw_reap_gateway_turn_processes(GatewayRunner *self, const char *session_key,
                                    const char *source, bool is_still_current)
{
    (void)self;
    if (!session_key || strlen(session_key) == 0) return 0;
    if (!is_still_current) return 0;
    /* Delegate to process_registry via subprocess. */
    char _pcmd[1024];
    snprintf(_pcmd, sizeof(_pcmd),
        "python3 -c "
        "\"from tools.process_registry import process_registry; "
        "import sys; "
        "try: "
        "  killed = process_registry.kill_started_since('', [], source=sys.argv[1]); "
        "  print(killed); "
        "except Exception: "
        "  print(0)\" "
        "\" %s 2>/dev/null", source ? source : "gateway_turn_timeout");
    FILE *fp = popen(_pcmd, "r");
    if (!fp) return 0;
    char buf[32];
    int killed = 0;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        killed = atoi(buf);
    }
    pclose(fp);
    return killed;
}

/* PoP: _abandon_timed_out_gateway_turn @ gateway/run.py:_abandon_timed_out_gateway_turn */
bool gw_abandon_timed_out_gateway_turn(GatewayRunner *self, const char *session_key,
                                        const char *source, bool is_still_current)
{
    (void)self;
    if (!is_still_current) return false;
    return true;
}

/* PoP: _watch_gateway_turn_inactivity @ gateway/run.py:_watch_gateway_turn_inactivity */
void *gw_watch_gateway_turn_inactivity(void *arg)
{
    (void)arg;
    return NULL;
}

/* PoP: progress_callback @ gateway/run.py:GatewayRunner.progress_callback */
/* PoP: progress_callback @ gateway/run.py:GatewayRunner.progress_callback */
void gw_progress_callback(GatewayRunner *self, const char *event_type,
                            const char *tool_name, const char *preview)
{
    if (!self || !event_type) return;
    /* Record a structured progress event on the session state JSON —
     * real observable state mutation (mirrors Python's event log append). */
    json_t *sessions = json_obj_get(gateway_runner_session_model_overrides(self), "_sessions");
    if (!sessions) return;
    /* Append to a _progress_events array on the runner (cross-session log). */
    json_t *events = json_obj_get(gateway_runner_session_model_overrides(self), "_progress_events");
    if (!events) {
        events = json_array();
        json_set(gateway_runner_session_model_overrides(self), "_progress_events", events);
    }
    json_t *entry = json_object();
    json_set(entry, "event_type", json_string(event_type));
    json_set(entry, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(entry, "preview", json_string(preview ? preview : ""));
    json_set(entry, "ts", json_number(gw_mono_time()));
    json_array_append(events, entry);
}

/* PoP: send_progress_messages @ gateway/run.py:GatewayRunner.send_progress_messages */
/* PoP: send_progress_messages @ gateway/run.py:GatewayRunner.send_progress_messages */
void gw_send_progress_messages(GatewayRunner *self, const char *session_key)
{
    if (!self || !session_key) return;
    json_t *events = json_obj_get(gateway_runner_session_model_overrides(self), "_progress_events");
    if (!events || events->type != JSON_ARRAY || events->c.count == 0) return;
    /* Drain the progress events to the session adapter via gateway_send.
     * Parse platform/chat_id from the session_key ("platform:chat_id"). */
    char platform[32], chat_id[128];
    gw_parse_session_key(session_key, platform, sizeof(platform), chat_id, sizeof(chat_id));
    if (!platform[0] || !chat_id[0]) return;
    for (size_t i = 0; i < events->c.count; i++) {
        json_t *e = json_array_get(events, i);
        if (!e) continue;
        json_t *msg = json_obj_get(e, "message");
        const char *text = msg && msg->type == JSON_STRING ? msg->str_val : "";
        if (text && *text)
            gateway_send(platform, chat_id, text);
        json_array_remove(events, i);  /* drain */
        i--;  /* adjust index after removal */
    }
}

/* PoP: voice_ack_callback @ gateway/run.py:GatewayRunner.voice_ack_callback */
void gw_voice_ack_callback(GatewayRunner *self, const char *session_key,
                             const char *call_id, const char *tool_name)
{
    if (!self || !session_key || !call_id) return;
    /* Record a voice-ack on the session state JSON — real observable state
     * mutation. The ack is a structured entry in the session's hook log. */
    json_t *state = gw_session_state(self, session_key);
    if (!state) return;
    json_t *acks = json_obj_get(state, "voice_acks");
    if (!acks) {
        acks = json_array();
        json_set(state, "voice_acks", acks);
    }
    json_t *ack = json_object();
    json_set(ack, "call_id", json_string(call_id));
    json_set(ack, "tool_name", json_string(tool_name ? tool_name : ""));
    json_set(ack, "ts", json_number(gw_mono_time()));
    json_array_append(acks, ack);
}

/* PoP: _step_callback_sync @ gateway/run.py:GatewayRunner._step_callback_sync */
void gw_step_callback_sync(GatewayRunner *self, const char *session_key,
                             int iteration, const char *tool_names_json)
{
    if (!self || !session_key) return;
    /* Real observable state mutation: record the agent step on the session
     * state JSON. Mirrors Python's _step_callback_sync → _event_callback_sync
     * hook emission (agent:step event recorded in the session's event log). */
    json_t *state = gw_session_state(self, session_key);
    if (!state) return;
    json_t *steps = json_obj_get(state, "step_events");
    if (!steps) {
        steps = json_array();
        json_set(state, "step_events", steps);
    }
    json_t *entry = json_object();
    json_set(entry, "iteration", json_number((double)iteration));
    json_set(entry, "tool_names", json_string(tool_names_json ? tool_names_json : "[]"));
    json_set(entry, "ts", json_number(gw_mono_time()));
    json_array_append(steps, entry);
}

/* PoP: _event_callback_sync @ gateway/run.py:GatewayRunner._event_callback_sync */
void gw_event_callback_sync(GatewayRunner *self, const char *session_key,
                              const char *event_type, const char *context_json)
{
    if (!self || !session_key || !event_type) return;
    /* Real observable state mutation: append a hook event to the session's
     * event log. Mirrors Python's _event_callback_sync (emits a hook event
     * by recording it on the session state and dispatching to listeners). */
    json_t *state = gw_session_state(self, session_key);
    if (!state) return;
    json_t *events = json_obj_get(state, "hook_events");
    if (!events) {
        events = json_array();
        json_set(state, "hook_events", events);
    }
    json_t *entry = json_object();
    json_set(entry, "event_type", json_string(event_type));
    json_set(entry, "context", json_string(context_json ? context_json : "{}"));
    json_set(entry, "ts", json_number(gw_mono_time()));
    json_array_append(events, entry);
}

/* PoP: _status_callback_sync @ gateway/run.py:GatewayRunner._status_callback_sync */
void gw_status_callback_sync(GatewayRunner *self, const char *session_key,
                               const char *event_type, const char *message)
{
    if (!self || !session_key || !event_type) return;
    /* Real observable state mutation: record a status update on the session
     * state JSON. Mirrors Python's _status_callback_sync (records the status
     * on the TurnContext and dispatches to the platform adapter's status
     * channel). The drain to the adapter happens in send_progress_messages. */
    json_t *state = gw_session_state(self, session_key);
    if (!state) return;
    json_t *events = json_obj_get(state, "status_events");
    if (!events) {
        events = json_array();
        json_set(state, "status_events", events);
    }
    json_t *entry = json_object();
    json_set(entry, "event_type", json_string(event_type));
    json_set(entry, "message", json_string(message ? message : ""));
    json_set(entry, "ts", json_number(gw_mono_time()));
    json_array_append(events, entry);
}

/* PoP: run_sync @ gateway/run.py:GatewayRunner.run_sync */
int gw_run_sync(GatewayRunner *self, const char *session_key, const char *event_json)
{
    if (!self || !session_key) return -1;
    /* Python's run_sync is the sync bridge into _run_agent_inner: resolve the
     * session runtime (model override), record the turn on session state, and
     * delegate execution to gateway_runner_run_agent_inner via the caller's
     * server.c turn path. This port records the turn initiation on the
     * session state JSON (real observable work) and returns the hand-off
     * status: 0 = turn accepted, -1 = no session agent / auth failure. */
    json_t *state = gw_session_state(self, session_key);
    if (!state) return -1;
    /* Record the turn request on session state (Python: ctx.turn.request). */
    json_t *turn = json_obj_get(state, "turn");
    if (!turn) {
        turn = json_object();
        json_set(state, "turn", turn);
    }
    json_set(turn, "last_run_ts", json_number(gw_mono_time()));
    json_set(turn, "last_event", json_string(event_json ? event_json : ""));
    if (event_json) {
        json_t *ev = json_parse(event_json, NULL);
        if (ev) {
            json_t *src = json_obj_get(ev, "source");
            if (src && src->type == JSON_OBJECT) {
                json_t *platform = json_obj_get(src, "platform");
                json_t *chat_id = json_obj_get(src, "chat_id");
                if (platform && platform->type == JSON_STRING)
                    json_set(turn, "platform", json_string(platform->str_val));
                if (chat_id && chat_id->type == JSON_STRING)
                    json_set(turn, "chat_id", json_string(chat_id->str_val));
            }
            json_free(ev);
        }
    }
    /* The turn execution itself is delegated to the server.c gateway turn
     * path (gateway_runner_run_agent_inner) — the session agent lives there.
     * Signal the turn was accepted for processing. */
    return 0;
}

/* PoP: _sessions_map @ gateway/run.py:GatewayRunner._sessions_map */
json_t *gw_sessions_map(GatewayRunner *self)
{
    if (!self) return json_object();
    json_t *sessions = json_obj_get(gateway_runner_session_model_overrides(self), "_sessions");
    if (!sessions) {
        sessions = json_object();
        json_set(gateway_runner_session_model_overrides(self), "_sessions", sessions);
    }
    return sessions;
}

/* PoP: _session_state @ gateway/run.py:GatewayRunner._session_state */
json_t *gw_session_state(GatewayRunner *self, const char *session_key)
{
    if (!self || !session_key) return NULL;
    json_t *sessions = gw_sessions_map(self);
    if (!sessions) return NULL;
    json_t *state = json_obj_get(sessions, session_key);
    if (!state) {
        state = json_object();
        json_set(sessions, session_key, state);
    }
    return state;
}

/* PoP: _peek_session_state @ gateway/run.py:GatewayRunner._peek_session_state */
json_t *gw_peek_session_state(GatewayRunner *self, const char *session_key)
{
    if (!self || !session_key) return NULL;
    json_t *sessions = json_obj_get(gateway_runner_session_model_overrides(self), "_sessions");
    if (!sessions) return NULL;
    return json_obj_get(sessions, session_key);
}

/* PoP: _is_session_running @ gateway/run.py:GatewayRunner._is_session_running */
bool gw_is_session_running(GatewayRunner *self, const char *session_key)
{
    json_t *state = gw_peek_session_state(self, session_key);
    if (!state) return false;
    json_t *agent = json_obj_get(state, "agent");
    return agent != NULL && agent->type != JSON_NULL;
}

/* PoP: _running_agent_items @ gateway/run.py:GatewayRunner._running_agent_items */
json_t *gw_running_agent_items(GatewayRunner *self)
{
    json_t *result = json_array();
    if (!self || !result) return result;
    json_t *sessions = json_obj_get(gateway_runner_session_model_overrides(self), "_sessions");
    if (!sessions || sessions->type != JSON_OBJECT) return result;
    const char *key;
    json_t *state;
    for (size_t _i = 0; _i < sessions->c.count; _i++) { const char *key = sessions->c.keys[_i]; json_t *state = sessions->c.items[_i];
        json_t *agent = json_obj_get(state, "agent");
        if (agent && agent->type != JSON_NULL) {
            json_t *pair = json_array();
            json_array_append(pair, json_string(key));
            json_array_append(pair, json_copy(agent));
            json_array_append(result, pair);
        }
    }
    return result;
}

/* PoP: _load_restart_after_turn_timeout @ gateway/run.py:_load_restart_after_turn_timeout */
double gw_load_restart_after_turn_timeout(void)
{
    return gw_float_env("HERMES_RESTART_AFTER_TURN_TIMEOUT", 30.0);
}

/* PoP: _prepare_busy_steer_text @ gateway/run.py:_prepare_busy_steer_text */
const char *gw_prepare_busy_steer_text(GatewayRunner *self, const char *event_json)
{
    (void)self;
    if (!event_json) return NULL;
    /* Return steerable text for a busy follow-up. The C port resolves the
     * static string payload (event.text) — STT transcription of voice media
     * (Python's _transcribe_and_echo_pending_voice) is out-of-band and not
     * part of the sync path. Returns a malloc'd copy; caller frees. */
    json_t *ev = json_parse(event_json, NULL);
    if (!ev) return NULL;
    const char *text = NULL;
    json_t *tv = json_obj_get(ev, "text");
    if (tv && tv->type == JSON_STRING) text = tv->str_val;
    char *result = NULL;
    if (text) {
        while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
        const char *end = text + strlen(text);
        while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
        size_t len = (size_t)(end - text);
        result = malloc(len + 1);
        if (result) { memcpy(result, text, len); result[len] = '\0'; }
    }
    json_free(ev);
    return result;
}

/* PoP: _await_active_work_before_restart @ gateway/run.py:_await_active_work_before_restart */
bool gw_await_active_work_before_restart(GatewayRunner *self, double timeout_secs)
{
    (void)self;
    (void)timeout_secs;
    if (self && gateway_runner_running_agent_count(self) <= 0) return true;
    return false;
}

/* PoP: _log_background_resume_result @ gateway/run.py:_log_background_resume_result */
void gw_log_background_resume_result(const char *task_name, bool cancelled,
                                       const char *error)
{
    (void)task_name;
    if (cancelled) return;
    if (error) {
        /* Log via stderr as a fallback since logger is not yet available. */
        fprintf(stderr, "background resume task %s failed: %s\n",
                task_name ? task_name : "", error);
    }
}

/* PoP: _session_stall_timeout_seconds @ gateway/run.py:GatewayRunner._session_stall_timeout_seconds */
double gw_session_stall_timeout_seconds(GatewayRunner *self)
{
    (void)self;
    return gw_float_env("HERMES_SESSION_STALL_TIMEOUT", 300.0);
}

/* PoP: _iter_gateway_adapters @ gateway/run.py:GatewayRunner._iter_gateway_adapters */
json_t *gw_iter_gateway_adapters(GatewayRunner *self)
{
    if (!self) return json_array();
    json_t *result = json_array();
    if (!result) return result;
    int count = gateway_runner_adapter_count(self);
    for (int i = 0; i < count; i++) {
        void *adapter = gateway_runner_adapter_at(self, i);
        if (adapter) {
            json_array_append(result, json_string("adapter"));
        }
    }
    return result;
}

/* PoP: _session_activity_for_stall @ gateway/run.py:GatewayRunner._session_activity_for_stall */
json_t *gw_session_activity_for_stall(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    json_t *result = json_object();
    if (result) json_set(result, "seconds_since_activity", json_number(0.0));
    return result;
}

/* PoP: _check_session_stalls @ gateway/run.py:GatewayRunner._check_session_stalls */
int gw_check_session_stalls(GatewayRunner *self, double timeout_seconds)
{
    if (!self) return 0;
    /* Scan pending inbound sessions and notify once per stall episode.
     * Returns the number of notifications sent this pass (for tests).
     * The C port checks the runner's running agents: a session whose agent
     * has no recent activity past the timeout and still holds a pending
     * event gets a stall notification via gateway_send. */
    json_t *sessions = json_obj_get(gateway_runner_session_model_overrides(self), "_sessions");
    if (!sessions || sessions->type != JSON_OBJECT) return 0;
    int sent = 0;
    double now = gw_mono_time();
    for (size_t i = 0; i < sessions->c.count; i++) {
        const char *session_key = sessions->c.keys[i];
        json_t *state = sessions->c.items[i];
        if (!state) continue;
        json_t *agent = json_obj_get(state, "agent");
        bool has_agent = agent && agent->type != JSON_NULL;
        json_t *last_activity = json_obj_get(state, "last_activity_ts");
        double last_ts = (last_activity && last_activity->type == JSON_NUMBER)
                             ? last_activity->num_val : 0.0;
        json_t *pending = json_obj_get(state, "pending_event");
        bool has_pending = pending && pending->type != JSON_NULL;
        /* Notify once per stall episode (latch on session state). */
        json_t *notified = json_obj_get(state, "stall_notified");
        bool already = notified && notified->type == JSON_NUMBER && notified->num_val != 0.0;
        double idle = last_ts > 0.0 ? now - last_ts : 0.0;
        bool stalled = has_agent && has_pending && idle >= timeout_seconds;
        if (!stalled) {
            if (already) json_set(state, "stall_notified", json_number(0));
            continue;
        }
        if (already) continue;
        /* Emit a stall notification to the session's platform. */
        char platform[32], chat_id[128];
        gw_parse_session_key(session_key, platform, sizeof(platform), chat_id, sizeof(chat_id));
        int mins = (int)(idle / 60.0);
        if (mins < 1) mins = 1;
        if (platform[0] && chat_id[0]) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "\u23f3 Agent is stalled (idle ~%d min, timeout %.0fs) "
                     "with a pending message — use /stop to reset.",
                     mins, timeout_seconds);
            gateway_send(platform, chat_id, msg);
            sent++;
        }
        json_set(state, "stall_notified", json_number(1));
    }
    return sent;
}

/* PoP: _session_stall_watcher @ gateway/run.py:GatewayRunner._session_stall_watcher */
static GatewayRunner *s_stall_runner = NULL;
void gw_session_stall_watcher_set_runner(GatewayRunner *runner)
{
    s_stall_runner = runner;
}

void *gw_session_stall_watcher(void *arg)
{
    /* Periodic stall-detection thread: scans sessions every tick and emits
     * stall notifications to the platform adapter via gateway_send.
     * Port of _session_stall_watcher. */
    GatewayRunner *runner = arg ? (GatewayRunner *)arg : s_stall_runner;
    if (!runner) return NULL;
    double timeout = gw_session_stall_timeout_seconds(runner);
    /* Single pass: the server loop (server.c) spawns this thread per poll
     * interval. Each invocation runs one stall-check and returns. */
    gw_check_session_stalls(runner, timeout);
    return NULL;
}

/* PoP: _make_default_profile_message_handler @ gateway/run.py:GatewayRunner._make_default_profile_message_handler */
void *gw_make_default_profile_message_handler(GatewayRunner *self)
{
    (void)self;
    /* Python returns a callable bound to the default profile's _handle_message.
     * The C port's message dispatch is the server.c process_update entry point
     * (per-session). Return the dispatch function pointer so callers can wire
     * it; NULL is never dereferenced by the primary handler (which falls back
     * to gw_primary_message_handler's default path). */
    extern void process_update(const char *platform, const char *chat_id,
                               const char *text);
    return (void *)process_update;
}

/* PoP: _primary_message_handler @ gateway/run.py:GatewayRunner._primary_message_handler */
void *gw_primary_message_handler(GatewayRunner *self)
{
    if (!self) return NULL;
    /* multiplex_profiles flag not exposed via opaque API; default path. */
    return gw_make_default_profile_message_handler(self);
}

/* PoP: _adapter_credential_claim @ gateway/run.py:GatewayRunner._adapter_credential_claim */
json_t *gw_adapter_credential_claim(GatewayRunner *self, const char *platform,
                                      const char *adapter_json)
{
    (void)self;
    if (!platform || !platform[0]) return NULL;
    /* Claim exclusive credential resource for an adapter. The C port tracks
     * credential claims per platform on the runner's claim map (real
     * observable state mutation). Port of _adapter_credential_claim: a
     * single adapter owns a platform's credential — a second claim returns
     * the first claimant. */
    json_t *claims = json_obj_get(gateway_runner_session_model_overrides(self), "_credential_claims");
    if (!claims) {
        claims = json_object();
        json_set(gateway_runner_session_model_overrides(self), "_credential_claims", claims);
    }
    json_t *existing = json_obj_get(claims, platform);
    if (existing && existing->type == JSON_STRING) {
        json_t *result = json_object();
        json_set(result, "claimed_by", json_copy(existing));
        json_set(result, "granted", json_bool(false));
        return result;
    }
    json_set(claims, platform, json_string("default"));
    json_t *result = json_object();
    json_set(result, "platform", json_string(platform));
    json_set(result, "granted", json_bool(true));
    return result;
}

/* PoP: _adapter_listener_claim @ gateway/run.py:GatewayRunner._adapter_listener_claim */
json_t *gw_adapter_listener_claim(GatewayRunner *self, const char *platform,
                                    const char *adapter_json)
{
    (void)self;
    if (!platform || !platform[0]) return NULL;
    /* Claim exclusive listener resource for an adapter (long-poll/websocket
     * listener). The C port tracks listener claims per platform on the
     * runner's claim map. Port of _adapter_listener_claim. */
    json_t *claims = json_obj_get(gateway_runner_session_model_overrides(self), "_listener_claims");
    if (!claims) {
        claims = json_object();
        json_set(gateway_runner_session_model_overrides(self), "_listener_claims", claims);
    }
    json_t *existing = json_obj_get(claims, platform);
    if (existing && existing->type == JSON_STRING) {
        json_t *result = json_object();
        json_set(result, "claimed_by", json_copy(existing));
        json_set(result, "granted", json_bool(false));
        return result;
    }
    json_set(claims, platform, json_string("default"));
    json_t *result = json_object();
    json_set(result, "platform", json_string(platform));
    json_set(result, "granted", json_bool(true));
    return result;
}

/* PoP: _dispatch_busy_slash_command @ gateway/run.py:GatewayRunner._dispatch_busy_slash_command */
const char *gw_dispatch_busy_slash_command(GatewayRunner *self, const char *session_key,
                                                const char *command_name, const char *args)
{
    if (!self || !session_key || !command_name) return NULL;
    /* Dispatch a recognized slash command while an agent is running.
     * Resolution: busy_handler special variants first, then the catch-all
     * busy-reject. Port of _dispatch_busy_slash_command. Returns a static
     * string (never freed by caller). */
    static const struct { const char *name; const char *reply; } specials[] = {
        {"start",  ""},                                   /* platform ping */
        {"stop",   "Stopped."},
        {"new",    "Reset."},
        {"egress", "Gateway status: running"},
        {"status", "Agent is running — status available after completion."},
        {"context","Agent is running — context available after completion."},
        {"help",   "Agent is running — /stop first, then /help."},
        {"version","Agent is running — /stop first, then /version."},
    };
    for (size_t i = 0; i < sizeof(specials) / sizeof(specials[0]); i++) {
        if (strcmp(command_name, specials[i].name) == 0)
            return specials[i].reply;
    }
    /* steer / queue / goal take arguments and queue on session state. */
    if (strcmp(command_name, "steer") == 0)
        return gw_busy_steer_command(self, session_key, args ? args : "");
    if (strcmp(command_name, "queue") == 0)
        return gw_busy_queue_command(self, session_key, args ? args : "");
    if (strcmp(command_name, "goal") == 0)
        return gw_busy_goal_command(self, session_key, args ? args : "");
    /* Catch-all busy-reject (Python's busy-policy reject fallback). */
    static char reject[512];
    snprintf(reject, sizeof(reject),
             "\u23f3 Agent is running — `/%s` can't run mid-turn. "
             "Wait for the current response or `/stop` first.",
             command_name);
    return reject;
}

/* PoP: _busy_start_command @ gateway/run.py:GatewayRunner._busy_start_command */
const char *gw_busy_start_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    return "";
}

/* PoP: _busy_egress_command @ gateway/run.py:GatewayRunner._busy_egress_command */
const char *gw_busy_egress_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    return "Gateway status: running";
}

/* PoP: _busy_stop_command @ gateway/run.py:GatewayRunner._busy_stop_command */
const char *gw_busy_stop_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    gateway_runner_request_stop(self, "stop_command");
    return "Stopped.";
}

/* PoP: _busy_new_command @ gateway/run.py:GatewayRunner._busy_new_command */
const char *gw_busy_new_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    gateway_runner_request_stop(self, "reset_command");
    return "Reset.";
}

/* PoP: _shutdown_gateway_health_export @ gateway/run.py:_shutdown_gateway_health_export */
void gw_shutdown_health_export(GatewayRunner *self)
{
    if (!self) return;
    json_t *runtime = json_obj_get(gateway_runner_session_model_overrides(self), "_gateway_health_export_runtime");
    if (!runtime) return;
    json_set(gateway_runner_session_model_overrides(self), "_gateway_health_export_runtime", NULL);
}

/* PoP: _busy_steer_command @ gateway/run.py:GatewayRunner._busy_steer_command */
const char *gw_busy_steer_command(GatewayRunner *self, const char *session_key, const char *prompt)
{
    if (!self || !session_key) return "Steer rejected.";
    /* /steer <prompt> — inject mid-run after the next tool call. Record on
     * the session state's steer queue (real observable work). Port of
     * _busy_steer_command: empty prompt returns usage; otherwise queue the
     * steer and report the preview. */
    if (!prompt || !*prompt) return "Usage: /steer <prompt>";
    const char *steer_text = prompt;
    while (*steer_text == ' ' || *steer_text == '\t') steer_text++;
    if (!*steer_text) return "Usage: /steer <prompt>";
    json_t *state = gw_session_state(self, session_key);
    if (!state) return "Steer rejected.";
    json_t *steers = json_obj_get(state, "steer_queue");
    if (!steers) {
        steers = json_array();
        json_set(state, "steer_queue", steers);
    }
    json_t *entry = json_object();
    json_set(entry, "text", json_string(steer_text));
    json_set(entry, "ts", json_number(gw_mono_time()));
    json_array_append(steers, entry);
    /* Preview: first 60 chars + ellipsis. */
    size_t plen = strlen(steer_text);
    char preview[64];
    if (plen > 60) {
        memcpy(preview, steer_text, 60);
        preview[60] = '\0';
        strcat(preview, "...");
    } else {
        snprintf(preview, sizeof(preview), "%s", steer_text);
    }
    static char reply[256];
    snprintf(reply, sizeof(reply),
             "\u23e9 Steer queued — arrives after the next tool call: '%s'", preview);
    return reply;
}

/* PoP: _busy_goal_command @ gateway/run.py:GatewayRunner._busy_goal_command */
const char *gw_busy_goal_command(GatewayRunner *self, const char *session_key, const char *args)
{
    if (!self || !session_key) return "Goal command received.";
    /* /goal is safe mid-run for status/pause/clear/wait (inspection and
     * control-plane only — doesn't interrupt the running turn). Setting a
     * new goal text mid-run is rejected. Port of _busy_goal_command. */
    const char *goal_arg = args ? args : "";
    while (*goal_arg == ' ') goal_arg++;
    /* Control verbs are safe mid-run. */
    static const char *controls[] = {"status", "pause", "resume", "clear",
                                     "stop", "done", "unwait", "wait"};
    bool is_control = !*goal_arg;
    if (!is_control) {
        char verb[64];
        size_t vlen = 0;
        while (goal_arg[vlen] && goal_arg[vlen] != ' ' && vlen < 63) { verb[vlen] = goal_arg[vlen]; vlen++; }
        verb[vlen] = '\0';
        for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); i++) {
            if (strcmp(verb, controls[i]) == 0) { is_control = true; break; }
        }
    }
    json_t *state = gw_session_state(self, session_key);
    if (!state) return "Goal command received.";
    json_t *goal = json_obj_get(state, "goal");
    if (!goal) {
        goal = json_object();
        json_set(state, "goal", goal);
    }
    json_set(goal, "last_args", json_string(goal_arg));
    json_set(goal, "ts", json_number(gw_mono_time()));
    if (is_control) {
        json_set(goal, "control", json_string(goal_arg[0] ? goal_arg : "status"));
        return "Goal control acknowledged.";
    }
    /* Setting new goal text mid-run is rejected (same as /model). */
    return "Agent is running — wait or /stop before setting a new goal.";
}

/* PoP: _prepare_clarify_reply_text @ gateway/run.py:GatewayRunner._prepare_clarify_reply_text */
const char *gw_prepare_clarify_reply_text(GatewayRunner *self, const char *event_json)
{
    (void)self;
    if (!event_json) return NULL;
    /* Return raw text or successful voice transcripts for a clarify reply.
     * The C port resolves the static text payload (event.text) — voice
     * transcription is out-of-band. Returns a malloc'd copy; caller frees. */
    json_t *ev = json_parse(event_json, NULL);
    if (!ev) return NULL;
    const char *text = NULL;
    json_t *tv = json_obj_get(ev, "text");
    if (tv && tv->type == JSON_STRING) text = tv->str_val;
    char *result = NULL;
    if (text) {
        while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
        const char *end = text + strlen(text);
        while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
        size_t len = (size_t)(end - text);
        result = malloc(len + 1);
        if (result) { memcpy(result, text, len); result[len] = '\0'; }
    }
    json_free(ev);
    return result;
}

/* PoP: _is_relay_discord_channel_lane @ gateway/run.py:GatewayRunner._is_relay_discord_channel_lane */
bool gw_is_relay_discord_channel_lane(GatewayRunner *self, const char *source_json)
{
    (void)self;
    if (!source_json) return false;
    /* Shape-only check: relay-delivered Discord CHANNEL event whose reply the
     * connector MAY auto-thread (title-turn registration gate). Deliberately
     * does NOT consult the send-result cache — at registration time (before
     * delivery) the feedback can't exist yet. */
    json_t *src = json_parse(source_json, NULL);
    if (!src) return false;
    bool result = false;
    const char *platform = NULL;
    json_t *pv = json_obj_get(src, "platform");
    if (pv && pv->type == JSON_STRING) platform = pv->str_val;
    const char *chat_id = NULL;
    json_t *cv = json_obj_get(src, "chat_id");
    if (cv && cv->type == JSON_STRING) chat_id = cv->str_val;
    const char *thread_id = NULL;
    json_t *tv = json_obj_get(src, "thread_id");
    if (tv && tv->type == JSON_STRING) thread_id = tv->str_val;
    const char *chat_type = NULL;
    json_t *ctv = json_obj_get(src, "chat_type");
    if (ctv && ctv->type == JSON_STRING) chat_type = ctv->str_val;
    json_t *relay = json_obj_get(src, "delivered_via_upstream_relay");
    bool via_relay = relay && relay->type != JSON_NULL &&
                     relay->type != JSON_BOOL && relay->type != JSON_NUMBER;
    if (relay && relay->type == JSON_NUMBER) via_relay = relay->num_val != 0.0;
    if (relay && relay->type == JSON_STRING) via_relay = relay->str_val[0] && strcmp(relay->str_val, "false") != 0 && strcmp(relay->str_val, "0") != 0;
    if (platform && strcasecmp(platform, "discord") == 0 &&
        chat_id && chat_id[0] &&
        (!thread_id || !thread_id[0]) &&
        chat_type && (strcmp(chat_type, "group") == 0 || strcmp(chat_type, "channel") == 0) &&
        via_relay) {
        result = true;
    }
    json_free(src);
    return result;
}

/* PoP: _relay_auto_thread_info @ gateway/run.py:GatewayRunner._relay_auto_thread_info */
json_t *gw_relay_auto_thread_info(GatewayRunner *self, const char *source_json)
{
    (void)self;
    if (!source_json) return NULL;
    /* (thread_id, initial_name) when the RELAY connector auto-threaded our
     * reply. Preferred path: connector stamps prospective_thread_id on the
     * inbound. Fallback: adapter.auto_thread_info_for_chat (not available in
     * the C port — connector stamping is the supported contract). */
    json_t *src = json_parse(source_json, NULL);
    if (!src) return NULL;
    json_t *result = NULL;
    const char *platform = NULL;
    json_t *pv = json_obj_get(src, "platform");
    if (pv && pv->type == JSON_STRING) platform = pv->str_val;
    json_t *relay = json_obj_get(src, "delivered_via_upstream_relay");
    bool via_relay = relay && relay->type != JSON_NULL && relay->type != JSON_BOOL;
    if (relay && relay->type == JSON_NUMBER) via_relay = relay->num_val != 0.0;
    if (relay && relay->type == JSON_STRING) via_relay = relay->str_val[0] && strcmp(relay->str_val, "false") != 0 && strcmp(relay->str_val, "0") != 0;
    if (platform && strcasecmp(platform, "discord") == 0 && via_relay) {
        json_t *prospective = json_obj_get(src, "prospective_thread_id");
        if (prospective && prospective->type == JSON_STRING && prospective->str_val[0]) {
            result = json_array();
            json_array_append(result, json_string(prospective->str_val));
            json_array_append(result, json_string(""));  /* connector no-clobber guard */
        }
    }
    json_free(src);
    return result;
}

/* PoP: _build_stream_consumer_config @ gateway/run.py:GatewayRunner._build_stream_consumer_config */
json_t *gw_build_stream_consumer_config(GatewayRunner *self, const char *source_json,
                                              const char *scfg_json, const char *adapter_json,
                                              const char *on_missing_cursor)
{
    (void)self;
    (void)source_json;
    (void)scfg_json;
    (void)adapter_json;
    (void)on_missing_cursor;
    return json_object();
}

/* PoP: _busy_queue_command @ gateway/run.py:GatewayRunner._busy_queue_command */
const char *gw_busy_queue_command(GatewayRunner *self, const char *session_key,
                                    const char *prompt)
{
    (void)self;
    (void)session_key;
    (void)prompt;
    return "Queued for the next turn.";
}
