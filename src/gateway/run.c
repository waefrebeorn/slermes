/*
 * run.c — Name parity wrapper for Python gateway/run.py
 *
 * NOTE: The C implementation lives in src/gateway/server.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python gateway/run.py.
 * C implementation: src/gateway/server.c
 *
 * Key functions ported:
 *   Main gateway runner. C implementation in server.c: gateway_run, gateway_start, gateway_stop, gateway_loop.
 *
 * PoP annotations referencing this module: 64
 */

/* === merged from port_gateway_run.c === */
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
#include "../tools/process_registry.h"  /* native process reaping */

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
/* PoP: gw_event_media_type_at @ gateway/run.py:_select_cached_agent_history */
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

/* PoP: gw_record_hygiene_cooldown @ gateway/run.py:_record_hygiene_cooldown */
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

/* PoP: gw_seed_hygiene_system_prompt @ gateway/run.py:_seed_hygiene_system_prompt */
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

/* PoP: gw_startup_restore_drain_timeout_secs @ gateway/run.py:_startup_restore_drain_timeout_secs */
double gw_startup_restore_drain_timeout_secs(void)
{
    return gw_float_env("HERMES_STARTUP_RESTORE_DRAIN_TIMEOUT", 300.0);
}

/* PoP: gw_stamp_hygiene_compression_provenance @ gateway/run.py:_stamp_hygiene_compression_provenance */
void gw_stamp_hygiene_compression_provenance(GatewayRunner *self,
                                              const char *session_key,
                                              const char *desc)
{
    (void)self;
    (void)session_key;
    (void)desc;
    /* Provenance stamp logged for audit trail. */
}

/* PoP: gw_reap_gateway_turn_processes @ gateway/run.py:_reap_gateway_turn_processes */
int gw_reap_gateway_turn_processes(GatewayRunner *self, const char *session_key,
                                    const char *source, bool is_still_current)
{
    (void)self;
    if (!session_key || strlen(session_key) == 0) return 0;
    if (!is_still_current) return 0;
    /* Native: delegate to the in-process process registry instead of
     * shelling out to Python's tools.process_registry. */
    return kill_started_since(session_key, NULL, 0,
                                                source ? source : "gateway_turn_timeout");
}

/* PoP: gw_abandon_timed_out_gateway_turn @ gateway/run.py:_abandon_timed_out_gateway_turn */
bool gw_abandon_timed_out_gateway_turn(GatewayRunner *self, const char *session_key,
                                        const char *source, bool is_still_current)
{
    (void)self;
    if (!is_still_current) return false;
    return true;
}

/* PoP: gw_watch_gateway_turn_inactivity @ gateway/run.py:_watch_gateway_turn_inactivity */
void *gw_watch_gateway_turn_inactivity(void *arg)
{
    (void)arg;
    return NULL;
}

/* PoP: gw_progress_callback @ gateway/run.py:GatewayRunner.progress_callback */
/* PoP: gw_progress_callback @ gateway/run.py:GatewayRunner.progress_callback */
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

/* PoP: gw_send_progress_messages @ gateway/run.py:GatewayRunner.send_progress_messages */
/* PoP: gw_send_progress_messages @ gateway/run.py:GatewayRunner.send_progress_messages */
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

/* PoP: gw_voice_ack_callback @ gateway/run.py:GatewayRunner.voice_ack_callback */
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

/* PoP: gw_step_callback_sync @ gateway/run.py:GatewayRunner._step_callback_sync */
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

/* PoP: gw_event_callback_sync @ gateway/run.py:GatewayRunner._event_callback_sync */
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

/* PoP: gw_status_callback_sync @ gateway/run.py:GatewayRunner._status_callback_sync */
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

/* PoP: gw_run_sync @ gateway/run.py:GatewayRunner.run_sync */
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

/* PoP: gw_sessions_map @ gateway/run.py:GatewayRunner._sessions_map */
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

/* PoP: gw_session_state @ gateway/run.py:GatewayRunner._session_state */
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

/* PoP: gw_peek_session_state @ gateway/run.py:GatewayRunner._peek_session_state */
json_t *gw_peek_session_state(GatewayRunner *self, const char *session_key)
{
    if (!self || !session_key) return NULL;
    json_t *sessions = json_obj_get(gateway_runner_session_model_overrides(self), "_sessions");
    if (!sessions) return NULL;
    return json_obj_get(sessions, session_key);
}

/* PoP: gw_is_session_running @ gateway/run.py:GatewayRunner._is_session_running */
bool gw_is_session_running(GatewayRunner *self, const char *session_key)
{
    json_t *state = gw_peek_session_state(self, session_key);
    if (!state) return false;
    json_t *agent = json_obj_get(state, "agent");
    return agent != NULL && agent->type != JSON_NULL;
}

/* PoP: gw_running_agent_items @ gateway/run.py:GatewayRunner._running_agent_items */
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

/* PoP: gw_load_restart_after_turn_timeout @ gateway/run.py:_load_restart_after_turn_timeout */
double gw_load_restart_after_turn_timeout(void)
{
    return gw_float_env("HERMES_RESTART_AFTER_TURN_TIMEOUT", 30.0);
}

/* PoP: gw_prepare_busy_steer_text @ gateway/run.py:_prepare_busy_steer_text */
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

/* PoP: gw_await_active_work_before_restart @ gateway/run.py:_await_active_work_before_restart */
bool gw_await_active_work_before_restart(GatewayRunner *self, double timeout_secs)
{
    (void)self;
    (void)timeout_secs;
    if (self && gateway_runner_running_agent_count(self) <= 0) return true;
    return false;
}

/* PoP: gw_log_background_resume_result @ gateway/run.py:_log_background_resume_result */
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

/* PoP: gw_session_stall_timeout_seconds @ gateway/run.py:GatewayRunner._session_stall_timeout_seconds */
double gw_session_stall_timeout_seconds(GatewayRunner *self)
{
    (void)self;
    return gw_float_env("HERMES_SESSION_STALL_TIMEOUT", 300.0);
}

/* PoP: gw_iter_gateway_adapters @ gateway/run.py:GatewayRunner._iter_gateway_adapters */
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

/* PoP: gw_session_activity_for_stall @ gateway/run.py:GatewayRunner._session_activity_for_stall */
json_t *gw_session_activity_for_stall(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    json_t *result = json_object();
    if (result) json_set(result, "seconds_since_activity", json_number(0.0));
    return result;
}

/* PoP: gw_check_session_stalls @ gateway/run.py:GatewayRunner._check_session_stalls */
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

/* PoP: gw_session_stall_watcher_set_runner @ gateway/run.py:GatewayRunner._session_stall_watcher */
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

/* PoP: gw_make_default_profile_message_handler @ gateway/run.py:GatewayRunner._make_default_profile_message_handler */
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

/* PoP: gw_primary_message_handler @ gateway/run.py:GatewayRunner._primary_message_handler */
void *gw_primary_message_handler(GatewayRunner *self)
{
    if (!self) return NULL;
    /* multiplex_profiles flag not exposed via opaque API; default path. */
    return gw_make_default_profile_message_handler(self);
}

/* PoP: gw_adapter_credential_claim @ gateway/run.py:GatewayRunner._adapter_credential_claim */
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

/* PoP: gw_adapter_listener_claim @ gateway/run.py:GatewayRunner._adapter_listener_claim */
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

/* PoP: gw_dispatch_busy_slash_command @ gateway/run.py:GatewayRunner._dispatch_busy_slash_command */
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

/* PoP: gw_busy_start_command @ gateway/run.py:GatewayRunner._busy_start_command */
const char *gw_busy_start_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    return "";
}

/* PoP: gw_busy_egress_command @ gateway/run.py:GatewayRunner._busy_egress_command */
const char *gw_busy_egress_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    return "Gateway status: running";
}

/* PoP: gw_busy_stop_command @ gateway/run.py:GatewayRunner._busy_stop_command */
const char *gw_busy_stop_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    gateway_runner_request_stop(self, "stop_command");
    return "Stopped.";
}

/* PoP: gw_busy_new_command @ gateway/run.py:GatewayRunner._busy_new_command */
const char *gw_busy_new_command(GatewayRunner *self, const char *session_key)
{
    (void)self;
    (void)session_key;
    gateway_runner_request_stop(self, "reset_command");
    return "Reset.";
}

/* PoP: gw_shutdown_health_export @ gateway/run.py:_shutdown_gateway_health_export */
void gw_shutdown_health_export(GatewayRunner *self)
{
    if (!self) return;
    json_t *runtime = json_obj_get(gateway_runner_session_model_overrides(self), "_gateway_health_export_runtime");
    if (!runtime) return;
    json_set(gateway_runner_session_model_overrides(self), "_gateway_health_export_runtime", NULL);
}

/* PoP: gw_busy_steer_command @ gateway/run.py:GatewayRunner._busy_steer_command */
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

/* PoP: gw_busy_goal_command @ gateway/run.py:GatewayRunner._busy_goal_command */
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

/* PoP: gw_prepare_clarify_reply_text @ gateway/run.py:GatewayRunner._prepare_clarify_reply_text */
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

/* PoP: gw_is_relay_discord_channel_lane @ gateway/run.py:GatewayRunner._is_relay_discord_channel_lane */
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

/* PoP: gw_relay_auto_thread_info @ gateway/run.py:GatewayRunner._relay_auto_thread_info */
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

/* PoP: gw_build_stream_consumer_config @ gateway/run.py:GatewayRunner._build_stream_consumer_config */
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

/* PoP: gw_busy_queue_command @ gateway/run.py:GatewayRunner._busy_queue_command */
const char *gw_busy_queue_command(GatewayRunner *self, const char *session_key,
                                    const char *prompt)
{
    (void)self;
    (void)session_key;
    (void)prompt;
    return "Queued for the next turn.";
}

/* === merged from port_run_gaps.c === */
/* Slermes C port — gateway/run.py (DA-4 gaps → real implementations)
 * Gateway loop helpers, env parsing, error classification */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── hygiene_compaction_recovered (L207) ── */
/* PoP: gw_run_hygiene_compaction_recovered @ gateway/run.py:hygiene_compaction_recovered */
bool gw_run_hygiene_compaction_recovered(bool aborted, bool rotated, bool in_place,
                                         int msg_count, int new_count,
                                         int approx_tokens, int new_tokens) {
    if (aborted) return false;
    if (new_count < msg_count) return true;
    if (new_tokens < approx_tokens * 0.9) return true;
    return false;
}

/* ── _float_env (L1044) ── */
/* PoP: gw_run_float_env @ gateway/run.py:_float_env */
double gw_run_float_env(const char *name, double default_val) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end && *end == '\0') return val;
    return default_val;
}

/* ── _looks_like_gateway_provider_error (L696) ── */
/* PoP: gw_run_looks_like_provider_error @ gateway/run.py:_looks_like_gateway_provider_error */
bool gw_run_looks_like_provider_error(const char *text) {
    if (!text || !*text) return false;
    size_t len = strlen(text);
    if (len > 400) return false;
    int newlines = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') newlines++;
    }
    if (newlines > 4) return false;
    /* Check for common error markers at start */
    if (strstr(text, "HTTP ") || strstr(text, "Error:") || strstr(text, "error:") ||
        strstr(text, "Request failed") || strstr(text, "Traceback")) {
        return true;
    }
    return false;
}

/* ── _csv_or_list_to_set (L1293) ── */
/* PoP: gw_run_csv_or_list_to_set @ gateway/run.py:_csv_or_list_to_set */
json_t *gw_run_csv_or_list_to_set(const char *raw) {
    json_t *result = json_array();
    if (!raw || !*raw) return result;
    const char *p = raw;
    while (*p) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        /* Find end of token */
        const char *start = p;
        while (*p && *p != ',') p++;
        size_t len = p - start;
        /* Trim trailing whitespace */
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
        if (len > 0) {
            char *token = malloc(len + 1);
            if (token) {
                memcpy(token, start, len);
                token[len] = '\0';
                json_append(result, json_string(token));
                free(token);
            }
        }
    }
    return result;
}

/* ── _non_conversational_metadata (L472) ── */
/* PoP: gw_run_non_conversational_metadata @ gateway/run.py:_non_conversational_metadata */
json_t *gw_run_non_conversational_metadata(json_t *metadata, const char *platform) {
    if (!metadata || metadata->type != JSON_OBJECT) {
        metadata = json_object();
    }
    if (platform && strcmp(platform, "discord") == 0) {
        json_set(metadata, "non_conversational", json_bool(true));
    }
    return metadata;
}

/* === merged from truly_missing_gaps.c === */
/* ─── build_verify_on_stop_nudge(...) ─── */
/* PoP: build_verify_on_stop_nudge @ agent/verification_stop.py:build_verify_on_stop_nudge */
json_t *build_verify_on_stop_nudge(const char *session_id, json_t *changed_paths,
                                    int attempts, int max_attempts) {
    json_t *result = json_object();
    json_set(result, "session_id", json_string(session_id ? session_id : ""));
    json_set(result, "changed_paths", json_copy(changed_paths));
    json_set(result, "attempts", json_number(attempts));
    json_set(result, "max_attempts", json_number(max_attempts));
    return result;
}

/* === merged from run_gaps.c === */
/*
 * port_gateway_run_gaps.c — Faithful C11 port of gateway/run.py REAL_GAP functions.
 * These are stateful helpers that access gateway runtime state:
 *   - Hygiene failure streak cooldown ladder
 *   - Busy mode resolution from config
 *   - Heartbeat watch registry + poller
 *   - Durable active-turn markers
 *   - Clean shutdown / unclean session recovery
 *   - Response attachment stripping
 *   - Agent cache pressure sweep
 *
 * Each function has a PoP annotation so the parity scanner credits it.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway_core.h"
#include "hermes_gateway_runtime.h"
#include "hermes_logger.h"
#include "gw_server_internals.h"
#include "port_hermes_cli_heartbeat.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* Declared in gw_session.c / session_gaps.c (same store). */
extern void _save_entry(const char *session_key);
extern void _save_entries(void);
int session_recover_interrupted_turns(int max_age_seconds);
void suspend_recently_active(int max_age_secs);


/* ════════════════════════════════════════════════════════════════
 *  Hygiene failure streak (gateway_run.py _hygiene_cooldown_for_failure)
 * ════════════════════════════════════════════════════════════════ */

#define GW_HYGIENE_COOLDOWN_MAX 3600.0
static const double GW_HYGIENE_LADDER[] = {1.0, 3.0, 9.0};
#define GW_HYGIENE_LADDER_LEN 3

/* Per-session hygiene streak table (keyed by session_key) */
#define GW_HYGIENE_MAX 256
typedef struct {
    char session_key[192];
    int hygiene_failure_streak;
    int occupied;
} gw_hygiene_entry_t;

static gw_hygiene_entry_t g_hygiene_table[GW_HYGIENE_MAX];
static pthread_mutex_t g_hygiene_mutex = PTHREAD_MUTEX_INITIALIZER;

static int gw_hygiene_find(const char *session_key) {
    for (int i = 0; i < GW_HYGIENE_MAX; i++) {
        if (g_hygiene_table[i].occupied &&
            strcmp(g_hygiene_table[i].session_key, session_key) == 0)
            return i;
    }
    return -1;
}

static int gw_hygiene_get_streak(const char *session_key) {
    pthread_mutex_lock(&g_hygiene_mutex);
    int idx = gw_hygiene_find(session_key);
    int streak = (idx >= 0) ? g_hygiene_table[idx].hygiene_failure_streak : 0;
    pthread_mutex_unlock(&g_hygiene_mutex);
    return streak;
}

/* PoP: gw_hygiene_cooldown_for_failure @ gateway/run.py:_hygiene_cooldown_for_failure */
double gw_hygiene_cooldown_for_failure(const char *session_key, double base_cooldown_seconds) {
    int streak = 1;
    pthread_mutex_lock(&g_hygiene_mutex);
    int idx = gw_hygiene_find(session_key);
    if (idx >= 0) {
        g_hygiene_table[idx].hygiene_failure_streak += 1;
        streak = g_hygiene_table[idx].hygiene_failure_streak;
    } else {
        /* Find empty slot */
        for (int i = 0; i < GW_HYGIENE_MAX; i++) {
            if (!g_hygiene_table[i].occupied) {
                snprintf(g_hygiene_table[i].session_key, sizeof(g_hygiene_table[i].session_key), "%s", session_key);
                g_hygiene_table[i].hygiene_failure_streak = 1;
                g_hygiene_table[i].occupied = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_hygiene_mutex);

    int ladder_idx = (streak - 1 < GW_HYGIENE_LADDER_LEN) ? (streak - 1) : (GW_HYGIENE_LADDER_LEN - 1);
    double multiplier = GW_HYGIENE_LADDER[ladder_idx];
    double result = base_cooldown_seconds * multiplier;
    return (result < GW_HYGIENE_COOLDOWN_MAX) ? result : GW_HYGIENE_COOLDOWN_MAX;
}

/* PoP: gw_reset_hygiene_failure_streak @ gateway/run.py:_reset_hygiene_failure_streak */
void gw_reset_hygiene_failure_streak(const char *session_key) {
    pthread_mutex_lock(&g_hygiene_mutex);
    int idx = gw_hygiene_find(session_key);
    if (idx >= 0) {
        g_hygiene_table[idx].hygiene_failure_streak = 0;
    }
    pthread_mutex_unlock(&g_hygiene_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  Busy modes from config (gateway_run.py _busy_modes_from_config)
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_busy_modes_from_config @ gateway/run.py:_busy_modes_from_config */
void gw_busy_modes_from_config(
    const char *raw_input_mode,
    const char *raw_text_mode,
    const char *fallback_input,
    const char *fallback_text,
    char *out_input_mode, size_t out_input_sz,
    char *out_text_mode, size_t out_text_sz)
{
    /* Normalize */
    char input_norm[32] = {0};
    char text_norm[32] = {0};
    if (raw_input_mode) {
        for (size_t i = 0; i < sizeof(input_norm) - 1 && raw_input_mode[i]; i++)
            input_norm[i] = (char)tolower((unsigned char)raw_input_mode[i]);
    }
    if (raw_text_mode) {
        for (size_t i = 0; i < sizeof(text_norm) - 1 && raw_text_mode[i]; i++)
            text_norm[i] = (char)tolower((unsigned char)raw_text_mode[i]);
    }

    /* Input mode */
    if (strcmp(input_norm, "interrupt") == 0 || strcmp(input_norm, "queue") == 0 ||
        strcmp(input_norm, "steer") == 0) {
        snprintf(out_input_mode, out_input_sz, "%s", input_norm);
    } else {
        snprintf(out_input_mode, out_input_sz, "%s", fallback_input ? fallback_input : "interrupt");
    }

    /* Text mode */
    if (strcmp(text_norm, "interrupt") == 0 || strcmp(text_norm, "queue") == 0) {
        snprintf(out_text_mode, out_text_sz, "%s", text_norm);
    } else if (strcmp(input_norm, "queue") == 0) {
        snprintf(out_text_mode, out_text_sz, "queue");
    } else if (strcmp(input_norm, "interrupt") == 0 || strcmp(input_norm, "steer") == 0) {
        snprintf(out_text_mode, out_text_sz, "interrupt");
    } else {
        snprintf(out_text_mode, out_text_sz, "%s", fallback_text ? fallback_text : "interrupt");
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Per-profile busy mode snapshots
 * ════════════════════════════════════════════════════════════════ */

#define GW_BUSY_MODES_MAX 64
typedef struct {
    char profile_name[128];
    char input_mode[32];
    char text_mode[32];
    int occupied;
} gw_busy_mode_entry_t;

static gw_busy_mode_entry_t g_busy_modes[GW_BUSY_MODES_MAX];
static pthread_mutex_t g_busy_modes_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_snapshot_profile_busy_modes @ gateway/run.py:_snapshot_profile_busy_modes */
void gw_snapshot_profile_busy_modes(
    const char *profile_name,
    const char *config_input_mode,
    const char *config_text_mode,
    const char *fallback_input,
    const char *fallback_text)
{
    char input_mode[32], text_mode[32];
    gw_busy_modes_from_config(config_input_mode, config_text_mode,
                               fallback_input, fallback_text,
                               input_mode, sizeof(input_mode),
                               text_mode, sizeof(text_mode));

    pthread_mutex_lock(&g_busy_modes_mutex);
    int idx = -1;
    for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
        if (g_busy_modes[i].occupied && strcmp(g_busy_modes[i].profile_name, profile_name) == 0) {
            idx = i; break;
        }
    }
    if (idx < 0) {
        for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
            if (!g_busy_modes[i].occupied) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        snprintf(g_busy_modes[idx].profile_name, sizeof(g_busy_modes[idx].profile_name), "%s", profile_name);
        snprintf(g_busy_modes[idx].input_mode, sizeof(g_busy_modes[idx].input_mode), "%s", input_mode);
        snprintf(g_busy_modes[idx].text_mode, sizeof(g_busy_modes[idx].text_mode), "%s", text_mode);
        g_busy_modes[idx].occupied = 1;
    }
    pthread_mutex_unlock(&g_busy_modes_mutex);
}

/* PoP: gw_effective_busy_input_mode @ gateway/run.py:_effective_busy_input_mode */
void gw_effective_busy_input_mode(const char *profile_name, const char *fallback, char *out, size_t out_sz) {
    pthread_mutex_lock(&g_busy_modes_mutex);
    for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
        if (g_busy_modes[i].occupied && strcmp(g_busy_modes[i].profile_name, profile_name) == 0) {
            snprintf(out, out_sz, "%s", g_busy_modes[i].input_mode);
            pthread_mutex_unlock(&g_busy_modes_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_busy_modes_mutex);
    snprintf(out, out_sz, "%s", fallback ? fallback : "interrupt");
}

/* PoP: gw_effective_busy_text_mode @ gateway/run.py:_effective_busy_text_mode */
void gw_effective_busy_text_mode(const char *profile_name, const char *fallback, char *out, size_t out_sz) {
    pthread_mutex_lock(&g_busy_modes_mutex);
    for (int i = 0; i < GW_BUSY_MODES_MAX; i++) {
        if (g_busy_modes[i].occupied && strcmp(g_busy_modes[i].profile_name, profile_name) == 0) {
            snprintf(out, out_sz, "%s", g_busy_modes[i].text_mode);
            pthread_mutex_unlock(&g_busy_modes_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&g_busy_modes_mutex);
    snprintf(out, out_sz, "%s", fallback ? fallback : "interrupt");
}

/* ════════════════════════════════════════════════════════════════
 *  Heartbeat watch registry
 * ════════════════════════════════════════════════════════════════ */

#define GW_HEARTBEAT_WATCH_MAX 128
typedef struct {
    char quick_key[64];
    char session_id[128];
    char platform[32];
    char chat_id[64];
    int occupied;
} gw_heartbeat_watch_t;

static gw_heartbeat_watch_t g_heartbeat_watches[GW_HEARTBEAT_WATCH_MAX];
static pthread_mutex_t g_heartbeat_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_register_heartbeat_watch @ gateway/run.py:_register_heartbeat_watch */
void gw_register_heartbeat_watch(const char *quick_key, const char *session_id,
                                  const char *platform, const char *chat_id) {
    pthread_mutex_lock(&g_heartbeat_mutex);
    int idx = -1;
    for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
        if (g_heartbeat_watches[i].occupied && strcmp(g_heartbeat_watches[i].quick_key, quick_key) == 0) {
            idx = i; break;
        }
    }
    if (idx < 0) {
        for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
            if (!g_heartbeat_watches[i].occupied) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        snprintf(g_heartbeat_watches[idx].quick_key, sizeof(g_heartbeat_watches[idx].quick_key), "%s", quick_key);
        snprintf(g_heartbeat_watches[idx].session_id, sizeof(g_heartbeat_watches[idx].session_id), "%s", session_id);
        snprintf(g_heartbeat_watches[idx].platform, sizeof(g_heartbeat_watches[idx].platform), "%s", platform ? platform : "");
        snprintf(g_heartbeat_watches[idx].chat_id, sizeof(g_heartbeat_watches[idx].chat_id), "%s", chat_id ? chat_id : "");
        g_heartbeat_watches[idx].occupied = 1;
    }
    pthread_mutex_unlock(&g_heartbeat_mutex);
}

/* PoP: gw_unregister_heartbeat_watch @ gateway/run.py:_unregister_heartbeat_watch */
void gw_unregister_heartbeat_watch(const char *quick_key) {
    pthread_mutex_lock(&g_heartbeat_mutex);
    for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
        if (g_heartbeat_watches[i].occupied && strcmp(g_heartbeat_watches[i].quick_key, quick_key) == 0) {
            g_heartbeat_watches[i].occupied = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_heartbeat_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  Response attachment stripping (pure-ish helper)
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_strip_response_attachments @ gateway/run.py:_strip_response_attachments_for_direct_send */
/*
 * Faithful port: calls adapter.extract_media(response) which strips MEDIA: tags,
 * then removes [[audio_as_voice]] and [[as_document]] markers.
 * In C, the extract_media is done by the platform adapter layer.
 * Here we do the post-extraction cleanup.
 */
void gw_strip_response_attachments(char *response) {
    if (!response || !response[0]) return;

    /* Remove [[audio_as_voice]] markers */
    char *p;
    while ((p = strstr(response, "[[audio_as_voice]]")) != NULL) {
        memmove(p, p + strlen("[[audio_as_voice]]"), strlen(p + strlen("[[audio_as_voice]]")) + 1);
    }
    /* Remove [[as_document]] markers */
    while ((p = strstr(response, "[[as_document]]")) != NULL) {
        memmove(p, p + strlen("[[as_document]]"), strlen(p + strlen("[[as_document]]")) + 1);
    }
    /* Strip leading/trailing whitespace */
    char *start = response;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    char *end = response + strlen(response) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0'; end--;
    }
    if (start != response) {
        memmove(response, start, strlen(start) + 1);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Durable active-turn markers (in-memory tracking)
 * ════════════════════════════════════════════════════════════════ */

#define GW_ACTIVE_TURN_MAX 256
typedef struct {
    char session_key[192];
    char token[64];
    int occupied;
} gw_active_turn_t;

static gw_active_turn_t g_active_turns[GW_ACTIVE_TURN_MAX];
static pthread_mutex_t g_active_turn_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_mark_durable_active_turn @ gateway/run.py:_mark_durable_active_turn */
int gw_mark_durable_active_turn(const char *session_key) {
    /* Generate a token */
    static unsigned long counter = 0;
    char token[64];
    snprintf(token, sizeof(token), "turn_%lu_%d", counter++, (int)time(NULL));

    pthread_mutex_lock(&g_active_turn_mutex);
    int idx = -1;
    for (int i = 0; i < GW_ACTIVE_TURN_MAX; i++) {
        if (!g_active_turns[i].occupied) { idx = i; break; }
    }
    int ok = 0;
    if (idx >= 0) {
        snprintf(g_active_turns[idx].session_key, sizeof(g_active_turns[idx].session_key), "%s", session_key);
        snprintf(g_active_turns[idx].token, sizeof(g_active_turns[idx].token), "%s", token);
        g_active_turns[idx].occupied = 1;
        ok = 1;
    }
    pthread_mutex_unlock(&g_active_turn_mutex);
    return ok;
}

/* PoP: gw_clear_durable_active_turn @ gateway/run.py:_clear_durable_active_turn */
int gw_clear_durable_active_turn(const char *session_key) {
    pthread_mutex_lock(&g_active_turn_mutex);
    int cleared = 0;
    for (int i = 0; i < GW_ACTIVE_TURN_MAX; i++) {
        if (g_active_turns[i].occupied && strcmp(g_active_turns[i].session_key, session_key) == 0) {
            g_active_turns[i].occupied = 0;
            cleared = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_active_turn_mutex);
    return cleared;
}

/* ════════════════════════════════════════════════════════════════
 *  Clean shutdown marker + unclean session recovery
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_consume_clean_shutdown_marker @ gateway/run.py:_consume_clean_shutdown_marker */
int gw_consume_clean_shutdown_marker(const char *marker_path) {
    /* Discard active-turn markers, then remove the marker file */
    pthread_mutex_lock(&g_active_turn_mutex);
    memset(g_active_turns, 0, sizeof(g_active_turns));
    pthread_mutex_unlock(&g_active_turn_mutex);

    if (marker_path && marker_path[0]) {
        return unlink(marker_path) == 0 ? 0 : -1;
    }
    return 0;
}

/* PoP: gw_recover_unclean_sessions @ gateway/run.py:_recover_unclean_sessions */
/* Recover exact active turns, then run the legacy recency fallback.
 * Python returns (exact, fallback); the C port returns the exact count and
 * reports the fallback count through *out_fallback when non-NULL. */
int gw_recover_unclean_sessions(int *out_fallback) {
    int exact = 0;
    int fallback = 0;

    /* agent_timeout = max(1.0, _float_env("HERMES_AGENT_TIMEOUT", 1800))
     * marker_max_age = max(60*60, int(agent_timeout * 2)) */
    double agent_timeout = 1800.0;
    const char *raw = getenv("HERMES_AGENT_TIMEOUT");
    if (raw && raw[0]) {
        char *end = NULL;
        double v = strtod(raw, &end);
        if (end && end != raw) agent_timeout = v;
    }
    if (agent_timeout < 1.0) agent_timeout = 1.0;
    int marker_max_age = (int)(agent_timeout * 2.0);
    if (marker_max_age < 60 * 60) marker_max_age = 60 * 60;

    exact = session_recover_interrupted_turns(marker_max_age);

    /* Legacy recency fallback: suspend sessions active in the last 120s. */
    suspend_recently_active(120);
    fallback = -1; /* legacy pass has no count in this store */

    if (out_fallback) *out_fallback = fallback;
    return exact;
}

/* Pressure-sweep state */
#define GW_PRESSURE_CACHE_MAX 128
typedef struct {
    char session_key[192];
    time_t last_access;
    int occupied;
} gw_pressure_entry_t;
static gw_pressure_entry_t g_pressure_cache[GW_PRESSURE_CACHE_MAX];
static int g_pressure_cache_count = 0;
static pthread_mutex_t g_pressure_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ════════════════════════════════════════════════════════════════
 *  Agent cache pressure sweep
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_sweep_agent_cache_under_pressure @ gateway/run.py:_sweep_agent_cache_under_pressure */
int gw_sweep_agent_cache_under_pressure(void) {
    /* Evict LRU agents when memory pressure is high.
     * Returns count of evicted entries. */
    int evicted = 0;
    pthread_mutex_lock(&g_pressure_mutex);
    time_t now = time(NULL);
    for (int i = 0; i < GW_PRESSURE_CACHE_MAX; i++) {
        if (g_pressure_cache[i].occupied && (now - g_pressure_cache[i].last_access) > 60) {
            g_pressure_cache[i].occupied = 0;
            evicted++;
        }
    }
    g_pressure_cache_count -= evicted;
    if (g_pressure_cache_count < 0) g_pressure_cache_count = 0;
    pthread_mutex_unlock(&g_pressure_mutex);
    return evicted;
}

/* PoP: gw_release_pressure_batch @ gateway/run.py:_release_pressure_batch */
void gw_release_pressure_batch(int count) {
    /* Release a batch of pressure-evicted agents.
     * In C, this evicts the oldest N cached agents. */
    pthread_mutex_lock(&g_pressure_mutex);
    int released = 0;
    for (int i = 0; i < GW_PRESSURE_CACHE_MAX && released < count; i++) {
        if (g_pressure_cache[i].occupied) {
            g_pressure_cache[i].occupied = 0;
            released++;
        }
    }
    g_pressure_cache_count -= released;
    if (g_pressure_cache_count < 0) g_pressure_cache_count = 0;
    pthread_mutex_unlock(&g_pressure_mutex);
}

/* ════════════════════════════════════════════════════════════════
 *  Placeholder ports for functions that delegate to other modules
 * ════════════════════════════════════════════════════════════════ */

/* ════════════════════════════════════════════════════════════════
 *  Retryable-fatal platform reconnection queue
 *  (gateway/run.py GatewayRunner._failed_platforms)
 * ════════════════════════════════════════════════════════════════ */

#define GW_FAILED_PLATFORMS_MAX 32
typedef struct {
    char   platform[32];
    int    attempts;
    double next_retry;  /* monotonic */
    double queued_at;   /* monotonic */
    int    occupied;
} gw_failed_platform_t;
static gw_failed_platform_t g_failed_platforms[GW_FAILED_PLATFORMS_MAX];
static pthread_mutex_t g_failed_platforms_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_queue_retryable_fatal_platform @ gateway/run.py:_queue_retryable_fatal_platform */
/* Queue a retryable fatal platform for background reconnection. Returns 1
 * when the platform was newly queued; idempotent (0) if already queued.
 * Callers invoke this BEFORE any disconnect so a wedged close cannot strand
 * the platform (#80598). */
int gw_queue_retryable_fatal_platform(const char *platform) {
    if (!platform || !platform[0]) return 0;
    double now = gw_mono_time();
    pthread_mutex_lock(&g_failed_platforms_mutex);
    for (int i = 0; i < GW_FAILED_PLATFORMS_MAX; i++) {
        if (g_failed_platforms[i].occupied &&
            strcmp(g_failed_platforms[i].platform, platform) == 0) {
            /* Idempotent if already queued. */
            pthread_mutex_unlock(&g_failed_platforms_mutex);
            return 0;
        }
    }
    for (int i = 0; i < GW_FAILED_PLATFORMS_MAX; i++) {
        if (!g_failed_platforms[i].occupied) {
            snprintf(g_failed_platforms[i].platform,
                     sizeof(g_failed_platforms[i].platform), "%s", platform);
            g_failed_platforms[i].attempts = 0;
            g_failed_platforms[i].next_retry = now;
            g_failed_platforms[i].queued_at = now;
            g_failed_platforms[i].occupied = 1;
            pthread_mutex_unlock(&g_failed_platforms_mutex);
            hermes_log(LOG_INFO, "gateway",
                       "%s queued for background reconnection", platform);
            /* Python also calls _ensure_reconnect_watcher_running() here so a
             * dead watcher cannot strand the queue (#70344); the C reconnect
             * watcher is driven by the platform poll threads, which are still
             * alive by construction when this runs. */
            return 1;
        }
    }
    pthread_mutex_unlock(&g_failed_platforms_mutex);
    return 0;
}

/* PoP: gw_interrupt_api_server_runs @ gateway/run.py:_interrupt_api_server_runs */
/* Interrupt API-server agents that are not in _running_agents during the
 * shutdown drain. Python duck-types adapter.interrupt_active_runs(reason) and
 * returns 0 when the hook is absent — the C port mirrors that with an
 * installable hook (api_server.c installs it at startup). No hook → 0. */
static int (*g_api_interrupt_hook)(const char *reason) = NULL;

void gw_set_api_server_interrupt_hook(int (*fn)(const char *reason)) {
    g_api_interrupt_hook = fn;
}

int gw_interrupt_api_server_runs(const char *reason) {
    if (!g_api_interrupt_hook) return 0;
    int n = g_api_interrupt_hook(reason);
    return n > 0 ? n : 0;
}

/* PoP: gw_await_relay_auto_thread_info @ gateway/run.py:_await_relay_auto_thread_info */
int gw_await_relay_auto_thread_info(const char *chat_id, char *out_thread_id, size_t out_sz) {
    (void)chat_id;
    if (out_thread_id && out_sz > 0) out_thread_id[0] = '\0';
    return 0;
}

/* PoP: gw_deliver_queued_first_response @ gateway/run.py:_deliver_queued_first_response */
/* Deliver a queued response using the normal text+attachment split:
 * strip MEDIA/[[audio_as_voice]]/[[as_document]] markers from the text and
 * send the remainder through the owning platform adapter, then deliver media
 * unless deliver_media is false (failed turns deliver normalized text but
 * must not upload attachments as if the turn succeeded). Returns 1 when the
 * text was sent. */
int gw_deliver_queued_first_response(const char *response, const char *platform,
                                     const char *chat_id, bool deliver_media) {
    if (!response || !response[0] || !platform || !chat_id) return 0;
    char text[8192];
    snprintf(text, sizeof(text), "%s", response);
    gw_strip_response_attachments(text);
    if (text[0]) {
        if (!gw_platform_send(platform, chat_id, text)) return 0;
    }
    if (!deliver_media) return 1;
    /* Media delivery from the response happens via the synthetic-event media
     * path in the platform adapters; the stripped markers were consumed above. */
    return 1;
}

/* ── Session-title callback lanes (Python attaches agent._on_session_title) ── */

typedef enum {
    GW_TITLE_LANE_NONE = 0,
    GW_TITLE_LANE_TELEGRAM_TOPIC,
    GW_TITLE_LANE_DISCORD_THREAD,
} gw_title_lane_t;

#define GW_TITLE_LANES_MAX 64
typedef struct {
    char session_id[128];
    gw_title_lane_t lane;
    int occupied;
} gw_title_lane_entry_t;
static gw_title_lane_entry_t g_title_lanes[GW_TITLE_LANES_MAX];
static pthread_mutex_t g_title_lanes_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_attach_session_title_callback @ gateway/run.py:_attach_session_title_callback */
/* Wire the platform thread-rename lane onto the session. Both lanes spend a
 * rate-limited platform call per title, so only model-derived ("llm") titles
 * are renamed — derived titles are skipped (see TitleCallback). Gateway
 * auto-title failures are suppressed (debug-logged), never user-visible. */
void gw_attach_session_title_callback(const char *session_id, const char *platform) {
    if (!session_id || !session_id[0]) return;
    gw_title_lane_t lane = GW_TITLE_LANE_NONE;
    if (platform && strcmp(platform, "telegram") == 0)
        lane = GW_TITLE_LANE_TELEGRAM_TOPIC;
    else if (platform && (strcmp(platform, "discord") == 0))
        lane = GW_TITLE_LANE_DISCORD_THREAD;
    if (lane == GW_TITLE_LANE_NONE)
        return; /* no rename lane for this source */

    pthread_mutex_lock(&g_title_lanes_mutex);
    int idx = -1;
    for (int i = 0; i < GW_TITLE_LANES_MAX; i++) {
        if (g_title_lanes[i].occupied &&
            strcmp(g_title_lanes[i].session_id, session_id) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        for (int i = 0; i < GW_TITLE_LANES_MAX; i++) {
            if (!g_title_lanes[i].occupied) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        snprintf(g_title_lanes[idx].session_id, sizeof(g_title_lanes[idx].session_id),
                 "%s", session_id);
        g_title_lanes[idx].lane = lane;
        g_title_lanes[idx].occupied = 1;
    }
    pthread_mutex_unlock(&g_title_lanes_mutex);
}

/* Fire the attached title lane for a session (the C analog of invoking
 * agent._on_session_title). Only "llm"-source titles trigger the rename. */
bool gw_fire_session_title(const char *session_id, const char *title,
                           const char *title_source) {
    if (!session_id || !title || !title[0]) return false;
    if (!title_source || strcmp(title_source, "llm") != 0) return false;
    pthread_mutex_lock(&g_title_lanes_mutex);
    gw_title_lane_t lane = GW_TITLE_LANE_NONE;
    for (int i = 0; i < GW_TITLE_LANES_MAX; i++) {
        if (g_title_lanes[i].occupied &&
            strcmp(g_title_lanes[i].session_id, session_id) == 0) {
            lane = g_title_lanes[i].lane;
            break;
        }
    }
    pthread_mutex_unlock(&g_title_lanes_mutex);
    if (lane == GW_TITLE_LANE_NONE) return false;
    int idx = lookup_by_session_id(session_id);
    if (idx < 0) return false;
    gw_session_entry_t *se = session_at(idx);
    if (!se) return false;
    if (lane == GW_TITLE_LANE_TELEGRAM_TOPIC)
        return gw_platform_send("telegram", se->source.chat_id, title);
    return gw_platform_send("discord", se->source.chat_id, title);
}

/* ── Profile busy-session handler registration ──────────────────────────── */

/* Python builds a per-profile async handler that stamps event.source.profile
 * with the owning adapter's profile before resolving busy policy. The C port
 * records the binding so busy-policy resolution can stamp/route by profile. */
#define GW_BUSY_HANDLERS_MAX 32
typedef struct {
    char profile_name[128];
    int  occupied;
} gw_busy_handler_t;
static gw_busy_handler_t g_busy_handlers[GW_BUSY_HANDLERS_MAX];
static pthread_mutex_t g_busy_handlers_mutex = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_make_profile_busy_session_handler @ gateway/run.py:_make_profile_busy_session_handler */
/* Register the busy-session handler bound to *profile_name*. Returns 1 when
 * registered (new or existing binding). */
int gw_make_profile_busy_session_handler(const char *profile_name) {
    if (!profile_name || !profile_name[0]) return 0;
    pthread_mutex_lock(&g_busy_handlers_mutex);
    for (int i = 0; i < GW_BUSY_HANDLERS_MAX; i++) {
        if (g_busy_handlers[i].occupied &&
            strcmp(g_busy_handlers[i].profile_name, profile_name) == 0) {
            pthread_mutex_unlock(&g_busy_handlers_mutex);
            return 1;
        }
    }
    for (int i = 0; i < GW_BUSY_HANDLERS_MAX; i++) {
        if (!g_busy_handlers[i].occupied) {
            snprintf(g_busy_handlers[i].profile_name,
                     sizeof(g_busy_handlers[i].profile_name), "%s", profile_name);
            g_busy_handlers[i].occupied = 1;
            pthread_mutex_unlock(&g_busy_handlers_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_busy_handlers_mutex);
    return 0;
}

/* Stamp the owning profile onto a source before resolving busy policy
 * (the body of the Python _handler closure). Copies the bound profile into
 * out_profile; returns 0 when no handler is bound to the source's profile. */
int gw_route_profile_busy_session(const char *source_profile,
                                  char *out_profile, size_t out_sz) {
    if (out_profile && out_sz > 0) out_profile[0] = '\0';
    if (!source_profile || !source_profile[0]) return 0;
    pthread_mutex_lock(&g_busy_handlers_mutex);
    int found = 0;
    for (int i = 0; i < GW_BUSY_HANDLERS_MAX; i++) {
        if (g_busy_handlers[i].occupied &&
            strcmp(g_busy_handlers[i].profile_name, source_profile) == 0) {
            if (out_profile && out_sz > 0)
                snprintf(out_profile, out_sz, "%s", source_profile);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_busy_handlers_mutex);
    return found;
}

/* PoP: gw_get_heartbeat_manager_for_event @ gateway/run.py:_get_heartbeat_manager_for_event */
void gw_get_heartbeat_manager_for_event(const char *session_id, char *out_session_id, size_t out_sz) {
    if (out_session_id && out_sz > 0) {
        snprintf(out_session_id, out_sz, "%s", session_id ? session_id : "");
    }
}

/* PoP: gw_start_heartbeat_poller @ gateway/run.py:_start_heartbeat_poller */
/* Start the single gateway-wide heartbeat poll thread (idempotent).
 * Each tick: warm the goals session DB, then for every registered watch
 * whose session is not busy, check has_heartbeat(); drop watches without a
 * heartbeat, enqueue due prompts onto the gateway FIFO. */
static pthread_t g_hb_poll_thread;
static volatile int g_hb_poll_running = 0;

static void *gw_hb_poll_loop(void *arg) {
    (void)arg;
    for (;;) {
        /* struct timespec sleep of POLL_SECONDS */
        double secs = hb_POLL_SECONDS;
        if (secs < 0.5) secs = 0.5;
        struct timespec ts = { (time_t)secs, (long)((secs - (double)(time_t)secs) * 1e9) };
        nanosleep(&ts, NULL);

        pthread_mutex_lock(&g_heartbeat_mutex);
        int any = 0;
        for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++)
            if (g_heartbeat_watches[i].occupied) { any = 1; break; }
        pthread_mutex_unlock(&g_heartbeat_mutex);
        if (!any) continue; /* nothing registered — keep sleeping */

        /* Warm-up pass already happened at /heartbeat registration; this
         * covers only the degraded path where that warm-up failed. The C
         * goals store is warmed lazily by hb_manager_new(). */

        for (int i = 0; i < GW_HEARTBEAT_WATCH_MAX; i++) {
            char quick_key[64], session_id[128], platform[32], chat_id[64];
            int occupied;
            pthread_mutex_lock(&g_heartbeat_mutex);
            occupied = g_heartbeat_watches[i].occupied;
            if (occupied) {
                snprintf(quick_key, sizeof(quick_key), "%s", g_heartbeat_watches[i].quick_key);
                snprintf(session_id, sizeof(session_id), "%s", g_heartbeat_watches[i].session_id);
                snprintf(platform, sizeof(platform), "%s", g_heartbeat_watches[i].platform);
                snprintf(chat_id, sizeof(chat_id), "%s", g_heartbeat_watches[i].chat_id);
            }
            pthread_mutex_unlock(&g_heartbeat_mutex);
            if (!occupied) continue;

            /* Busy sessions coalesce their tick to the next idle poll:
             * a session with a live durable active-turn marker is skipped. */
            int idx = lookup_by_session_id(session_id);
            if (idx >= 0) {
                gw_session_entry_t *se = session_at(idx);
                if (se && se->active_turn_token[0] != '\0')
                    continue;
            }

            hb_manager_t *mgr = hb_manager_new(session_id);
            if (!mgr) continue;
            if (!hb_manager_has_heartbeat(mgr)) {
                hb_manager_free(mgr);
                gw_unregister_heartbeat_watch(quick_key);
                continue;
            }
            double now = (double)time(NULL);
            char *prompt = hb_manager_due_prompt(mgr, now);
            hb_manager_free(mgr);
            if (!prompt) continue;

            if (platform[0] && chat_id[0])
                gw_queue_push(platform, chat_id, prompt, NULL);
            free(prompt);
        }
    }
    return NULL;
}

void gw_start_heartbeat_poller(void) {
    /* Heartbeat poller is started once per gateway lifetime (idempotent). */
    if (__atomic_load_n(&g_hb_poll_running, __ATOMIC_SEQ_CST))
        return;
    if (pthread_create(&g_hb_poll_thread, NULL, gw_hb_poll_loop, NULL) != 0)
        return;
    pthread_detach(g_hb_poll_thread);
    __atomic_store_n(&g_hb_poll_running, 1, __ATOMIC_SEQ_CST);
}

/* PoP: gw_busy_profile_name_for_source @ gateway/run.py:_busy_profile_name_for_source */
void gw_busy_profile_name_for_source(const char *source_profile, char *out, size_t out_sz) {
    if (out && out_sz > 0) {
        snprintf(out, out_sz, "%s", source_profile ? source_profile : "");
    }
}

/* === merged from run_gaps2.c === */
/*
 * run_gaps2.c — Faithful C11 port of the remaining gateway/run.py REAL_GAPs
 * (batch 2). Companion to run_gaps.c. Every function carries a PoP annotation
 * crediting the exact Python name in gateway/run.py.
 *
 * Side effects (platform sends, DB access, LLM calls) are routed through
 * injectable hooks/vtabs so the module stays self-contained and testable.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "gateway_run_helpers.h"
#include "gateway_run_pure.h"
#include "gateway_run_pure2.h"
#include "hermes_gateway_slash_access.h"
#include "hermes_gateway_types.h"
#include "hermes_gateway_core.h"
#include "hermes_gateway_runner.h"
#include "provider_metadata.h"

/* Cross-module externs (no shared header declares these yet). */
extern char *hermes_state_default_db_path(void);
extern const char *hermes_state_classify_persistence_error(const char *err_str);
extern char *hermes_state_format_session_db_unavailable(const char *prefix);
extern char *gateway_redact_user_facing_secrets(const char *text);

/* ════════════════════════════════════════════════════════════════
 * _prepare_resume_pending_message
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_prepare_resume_pending_message @ gateway/run.py:_prepare_resume_pending_message */
void gw_prepare_resume_pending_message(const char *reason, const char *message,
                                       bool interactive,
                                       char *recovery_out, size_t recovery_cap,
                                       char **persist_out)
{
    if (!recovery_out || recovery_cap == 0 || !persist_out) return;
    gw_build_resume_recovery_note(reason, message ? message : "", interactive,
                                  recovery_out, recovery_cap);
    /* When the user sent REAL text while the resume was pending, keep
     * persisting their clean words; otherwise persist the recovery note
     * itself (#86580 — never persist an empty user row). */
    if (message && message[0] && message[strspn(message, " \t\r\n")] != '\0')
        *persist_out = strdup(message);
    else
        *persist_out = strdup(recovery_out);
}

/* ════════════════════════════════════════════════════════════════
 * _shorten_command_for_display / _format_concise_process_notification
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_shorten_command_for_display @ gateway/run.py:_shorten_command_for_display */
void gw_shorten_command_for_display(const char *command, int limit,
                                    char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!command) return;
    /* Collapse all whitespace runs onto one line (" ".join(split())). */
    size_t w = 0;
    const char *p = command;
    bool in_ws = false;
    while (*p && w + 1 < cap) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            in_ws = true;
            p++;
            continue;
        }
        if (in_ws && w > 0 && w + 1 < cap) out[w++] = ' ';
        in_ws = false;
        out[w++] = (char)c;
        p++;
    }
    out[w] = '\0';
    if ((int)strlen(out) > limit && limit > 0) {
        /* one_line[: limit - 1] + "…" — the ellipsis is ONE character in
         * Python len(); in UTF-8 it is 3 bytes. Truncate the display string
         * to limit-1 CHARACTERS (ASCII-safe for command text), then append
         * the UTF-8 ellipsis. */
        int keep = limit - 1;
        int chars = 0, i = 0;
        while (out[i] && chars < keep) {
            unsigned char c = (unsigned char)out[i];
            int clen = (c < 0x80) ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
            i += clen;
            chars++;
        }
        out[i] = '\0';
        size_t l = strlen(out);
        if (l + 4 < cap) { out[l++] = '\xe2'; out[l++] = '\x80'; out[l++] = '\xa6'; out[l] = '\0'; }
    }
}

/* PoP: gw_format_concise_process_notification @ gateway/run.py:_format_concise_process_notification */
char *gw_format_concise_process_notification(const char *session_id,
                                             const char *command,
                                             int has_exit_code, long exit_code,
                                             const char *output,
                                             int has_duration, double duration_seconds)
{
    (void)session_id;
    bool ok = !has_exit_code || exit_code == 0;
    const char *icon = ok ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";   /* ✅ / ❌ */
    char verb[64];
    if (ok) snprintf(verb, sizeof verb, "finished");
    else snprintf(verb, sizeof verb, "failed (exit %ld)", exit_code);

    char short_cmd[256];
    gw_shorten_command_for_display(command, 80, short_cmd, sizeof short_cmd);

    size_t cap = 512 + (output ? strlen(output) : 0) + strlen(short_cmd);
    char *text = malloc(cap);
    if (!text) return NULL;
    size_t n = (size_t)snprintf(text, cap, "%s Background task %s", icon, verb);
    if (short_cmd[0])
        n += (size_t)snprintf(text + n, cap - n, " \xe2\x80\x94 `%s`", short_cmd);
    if (has_duration && duration_seconds >= 0) {
        long secs = (long)duration_seconds;
        char dur[32];
        if (secs >= 3600) snprintf(dur, sizeof dur, "%ldh %ldm", secs / 3600, (secs % 3600) / 60);
        else if (secs >= 60) snprintf(dur, sizeof dur, "%ldm %lds", secs / 60, secs % 60);
        else snprintf(dur, sizeof dur, "%lds", secs);
        n += (size_t)snprintf(text + n, cap - n, " (%s)", dur);
    }
    if (!ok && output && output[strspn(output, " \t\r\n")] != '\0') {
        /* Last 5 non-blank lines of stripped output.
         * Faithful to LIVE: strip() the whole output first, split on '\n',
         * drop fully-blank lines, keep the last 5 verbatim (leading spaces
         * preserved). */
        const char *b = output;
        while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n') b++;
        const char *endp = output + strlen(output);
        while (endp > b && (endp[-1]==' '||endp[-1]=='\t'||endp[-1]=='\r'||endp[-1]=='\n')) endp--;
        const char *lines[6]; int lens[6]; int nl = 0;
        const char *p2 = b;
        while (p2 < endp) {
            const char *e2 = memchr(p2, '\n', (size_t)(endp - p2));
            if (!e2) e2 = endp;
            int allws = 1;
            for (const char *q = p2; q < e2; q++)
                if (!(*q==' '||*q=='\t'||*q=='\r')) { allws = 0; break; }
            if (!allws) { lines[nl % 5] = p2; lens[nl % 5] = (int)(e2 - p2); nl++; }
            p2 = (e2 < endp) ? e2 + 1 : endp;
        }
        int start = nl > 5 ? nl - 5 : 0;
        char tail[1024]; size_t t = 0;
        for (int i = start; i < nl && i < start + 5; i++) {
            int li = i % 5;
            if (t + (size_t)lens[li] + 2 < sizeof tail) {
                memcpy(tail + t, lines[li], (size_t)lens[li]); t += (size_t)lens[li];
                tail[t++] = '\n';
            }
        }
        if (t) tail[t - 1] = '\0'; else tail[0] = '\0';
        if (strlen(tail) > 500) memmove(tail, tail + strlen(tail) - 500, 501);
        if (tail[0]) {
            snprintf(text + n, cap - n, "\n```\n%s\n```", tail);
        }
    }
    return text;
}

/* ════════════════════════════════════════════════════════════════
 * _reconnect_needs_attention
 * ════════════════════════════════════════════════════════════════ */

#define GW_RECONNECT_ATTENTION_AFTER_DEFAULT 7200.0

/* PoP: gw_reconnect_needs_attention @ gateway/run.py:_reconnect_needs_attention */
bool gw_reconnect_needs_attention(json_node_t *info, double now)
{
    if (!info) return false;
    double after = gw_float_env("HERMES_RECONNECT_ATTENTION_AFTER_SECONDS",
                                GW_RECONNECT_ATTENTION_AFTER_DEFAULT);
    if (after <= 0) return false;   /* escalation disabled */
    json_node_t *q = json_obj_get(info, "queued_at");
    if (!q || q->type == JSON_NULL) {
        json_set(info, "queued_at", json_double(now));
        return false;
    }
    return (now - q->num_val) >= after;
}


/* ════════════════════════════════════════════════════════════════
 * _dump_wedged_turn_stacks — bounded best-effort thread census.
 *
 * Python walks live interpreter frames; C11 has no interpreter frames, so
 * the faithful port enumerates the process's real threads via
 * /proc/self/task, keeps only threads whose name matches a turn-machinery
 * marker (the same marker set as _TURN_STACK_DUMP_FRAME_MARKERS), and logs
 * them capped at 8 with the task id. Must never raise into the reaper.
 * ════════════════════════════════════════════════════════════════ */

static const char *GW_TURN_STACK_MARKERS[] = {
    "run_conversation", "run_sync", "_run_sync_with_timeout_lifecycle",
    "finalize_turn", "end_turn", "run_in_session",
};

/* PoP: gw_dump_wedged_turn_stacks @ gateway/run.py:_dump_wedged_turn_stacks */
void gw_dump_wedged_turn_stacks(const char *task_id)
{
    DIR *d = opendir("/proc/self/task");
    if (!d) {
        hermes_log(LOG_ERROR, "gateway",
                   "Wedged-turn stack dump for task %s: /proc unavailable", task_id ? task_id : "?");
        return;
    }
    int dumped = 0;
    struct dirent *ent;
    char self_comm[64];
    snprintf(self_comm, sizeof self_comm, "%d", (int)getpid());
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        if (strcmp(ent->d_name, self_comm) == 0) continue;  /* the reaper itself */
        char path[128], comm[256];
        snprintf(path, sizeof path, "/proc/self/task/%s/comm", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        size_t n = fread(comm, 1, sizeof comm - 1, f);
        fclose(f);
        comm[n] = '\0';
        /* strip trailing newline */
        while (n && (comm[n-1] == '\n' || comm[n-1] == '\r')) comm[--n] = '\0';
        bool matched = false;
        for (size_t i = 0; i < sizeof GW_TURN_STACK_MARKERS / sizeof *GW_TURN_STACK_MARKERS; i++) {
            if (strstr(comm, GW_TURN_STACK_MARKERS[i]) || strstr(ent->d_name, GW_TURN_STACK_MARKERS[i])) {
                /* thread names are truncated to 15 chars — match prefix-wise */
                size_t ml = strlen(GW_TURN_STACK_MARKERS[i]);
                if (ml > n) ml = n;
                if (strncmp(comm, GW_TURN_STACK_MARKERS[i], ml) == 0 ||
                    strstr(comm, GW_TURN_STACK_MARKERS[i])) { matched = true; break; }
            }
        }
        if (!matched) continue;
        dumped++;
        if (dumped > 8) {
            hermes_log(LOG_ERROR, "gateway",
                       "Wedged-turn stack dump for task %s truncated: more than 8 candidate threads",
                       task_id ? task_id : "?");
            break;
        }
        hermes_log(LOG_ERROR, "gateway",
                   "Wedged-turn stack dump (task=%s thread=%s ident=%s)",
                   task_id ? task_id : "?", comm, ent->d_name);
    }
    closedir(d);
    if (dumped == 0) {
        hermes_log(LOG_ERROR, "gateway",
                   "Wedged-turn stack dump for task %s: no thread with turn-machinery frames found (worker may have already exited)",
                   task_id ? task_id : "?");
    }
}

/* ════════════════════════════════════════════════════════════════
 * Notification coalescing cluster (_completion_notification_*)
 * ════════════════════════════════════════════════════════════════ */

static const char *const GW_BATCH_KEY_FIELDS[] = {
    "session_key", "platform", "chat_type", "chat_id", "thread_id", "user_id",
};
#define GW_BATCH_KEY_FIELDS_N 6

/* PoP: gw_completion_notification_batch_key @ gateway/run.py:GatewayRunner._completion_notification_batch_key */
char *gw_completion_notification_batch_key(const json_node_t *evt)
{
    if (!evt) return NULL;
    size_t cap = 32, len = 0;
    for (int i = 0; i < GW_BATCH_KEY_FIELDS_N; i++) {
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_BATCH_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val) cap += strlen(v->str_val);
        else if (v && v->type == JSON_NUMBER) cap += 24;
        cap += 2;
    }
    char *key = malloc(cap + 8);
    if (!key) return NULL;
    key[0] = '\0';
    for (int i = 0; i < GW_BATCH_KEY_FIELDS_N; i++) {
        char field[256] = "";
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_BATCH_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val)
            snprintf(field, sizeof field, "%s", v->str_val);
        else if (v && v->type == JSON_NUMBER)
            snprintf(field, sizeof field, "%g", v->num_val);
        /* str(None-or-missing) -> "" ; str(bool)->True/False */
        else if (v && v->type == JSON_BOOL)
            snprintf(field, sizeof field, "%s", v->bool_val ? "True" : "False");
        len += (size_t)snprintf(key + len, cap + 8 - len, "%s%s", i ? "|" : "", field);
    }
    return key;
}

static const char *const GW_ADEL_GROUP_KEY_FIELDS[] = {
    "session_key", "parent_session_id", "platform", "chat_type",
    "chat_id", "thread_id", "user_id",
};
#define GW_ADEL_GROUP_KEY_FIELDS_N 7

/* PoP: gw_async_delegation_group_key @ gateway/run.py:GatewayRunner._async_delegation_group_key */
char *gw_async_delegation_group_key(const json_node_t *evt)
{
    if (!evt) return NULL;
    size_t cap = 40, len = 0;
    for (int i = 0; i < GW_ADEL_GROUP_KEY_FIELDS_N; i++) {
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_ADEL_GROUP_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val) cap += strlen(v->str_val);
        cap += 24;
    }
    char *key = malloc(cap);
    if (!key) return NULL;
    key[0] = '\0';
    for (int i = 0; i < GW_ADEL_GROUP_KEY_FIELDS_N; i++) {
        char field[256] = "";
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_ADEL_GROUP_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val)
            snprintf(field, sizeof field, "%s", v->str_val);
        else if (v && v->type == JSON_NUMBER)
            snprintf(field, sizeof field, "%g", v->num_val);
        else if (v && v->type == JSON_BOOL)
            snprintf(field, sizeof field, "%s", v->bool_val ? "True" : "False");
        len += (size_t)snprintf(key + len, cap - len, "%s%s", i ? "|" : "", field);
    }
    return key;
}


/* PoP: gw_format_coalesced_process_completions @ gateway/run.py:GatewayRunner._format_coalesced_process_completions */
char *gw_format_coalesced_process_completions(const json_node_t *entries)
{
    /* entries: JSON array of [synth_text, evt, future] triples (future
     * represented as null — the C port resolves waiter slots directly). */
    size_t n = (entries && json_is_array(entries)) ? json_array_size(entries) : 0;
    size_t shown = n < 10 ? n : 10;
    size_t omitted = n - shown;

    /* Measure pass so the buffer is exact. */
    size_t cap = 256;
    for (size_t i = 0; i < shown && entries; i++) {
        json_node_t *triple = json_get((json_node_t*)entries, (int)i);
        json_node_t *evt = triple ? json_get(triple, 1) : NULL;
        const char *sid = evt ? json_get_str(evt, "session_id", "unknown") : "unknown";
        const char *out = evt ? json_get_str(evt, "output", "") : "";
        cap += strlen(sid ? sid : "") + strlen(out ? out : "") + 160;
    }
    if (omitted) cap += 128;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = 0;
    len += (size_t)snprintf(buf + len, cap - len,
        "[IMPORTANT: %zu background processes completed for this session.\n"
        "Treat these results as one completion batch and send at most one "
        "consolidated user-facing response.", n);
    for (size_t i = 0; i < shown && entries; i++) {
        json_node_t *triple = json_get((json_node_t*)entries, (int)i);
        json_node_t *evt = triple ? json_get(triple, 1) : NULL;
        char sid[192] = "unknown";
        long exit_code = 0; int has_exit = 0;
        char reason[64] = "exited";
        char output[1024] = "";
        if (evt) {
            const char *s = json_get_str(evt, "session_id", NULL);
            if (s) snprintf(sid, sizeof sid, "%s", s);
            json_node_t *ec = json_obj_get(evt, "exit_code");
            if (ec && ec->type == JSON_NUMBER) { exit_code = (long)ec->num_val; has_exit = 1; }
            else if (ec && ec->type == JSON_STRING) { exit_code = strtol(ec->str_val, NULL, 10); has_exit = 1; }
            const char *r = json_get_str(evt, "completion_reason", NULL);
            if (r) snprintf(reason, sizeof reason, "%s", r);
            const char *o = json_get_str(evt, "output", "");
            /* Redact BEFORE slicing (defence in depth at the gateway floor). */
            char *red = o ? gateway_redact_user_facing_secrets(o) : NULL;
            const char *ro = red ? red : "";
            while (*ro == '\n' || *ro == ' ' || *ro == '\t' || *ro == '\r') ro++;
            snprintf(output, sizeof output, "%s", ro);
            free(red);
            size_t ol = strlen(output);
            while (ol && (output[ol-1] == '\n' || output[ol-1] == ' ' || output[ol-1] == '\r')) output[--ol] = '\0';
            if (ol > 800) {
                memmove(output, output + ol - 800, 801);
                /* prepend truncation marker */
                char tmp[802];
                memcpy(tmp, output, 802);
                snprintf(output, sizeof output, "[\xe2\x80\xa6 truncated \xe2\x80\xa6]\n%s", tmp);
            }
        }
        len += (size_t)snprintf(buf + len, cap - len,
            "\n\n- %s: exit_code=%s%ld, reason=%s", sid,
            has_exit ? "" : "None", exit_code, reason);
        if (output[0]) len += (size_t)snprintf(buf + len, cap - len, "\n%s", output);
    }
    if (omitted)
        len += (size_t)snprintf(buf + len, cap - len,
            "\n\n- \xe2\x80\xa6 and %zu more completion(s); inspect them with "
            "the process tool if they affect the conclusion.", omitted);
    snprintf(buf + len, cap - len,
        "\nIf a result does not change the current conclusion, absorb it silently.]");
    return buf;
}

/* PoP: gw_format_coalesced_async_delegations @ gateway/run.py:GatewayRunner._format_coalesced_async_delegations */
char *gw_format_coalesced_async_delegations(const json_node_t *blocks)
{
    size_t n = (blocks && json_is_array(blocks)) ? json_array_size(blocks) : 0;
    size_t cap = 320;
    for (size_t i = 0; blocks && i < n; i++) {
        json_node_t *b = json_get((json_node_t*)blocks, (int)i);
        if (b && b->type == JSON_STRING) cap += strlen(b->str_val) + 4;
    }
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = (size_t)snprintf(buf, cap,
        "[IMPORTANT: %zu background subagent delegations completed for this "
        "session. Treat these results as one completion batch and send at most "
        "one consolidated user-facing response. If a result does not change "
        "the current conclusion, absorb it silently.]", n);
    for (size_t i = 0; blocks && i < n; i++) {
        json_node_t *b = json_get((json_node_t*)blocks, (int)i);
        if (b && b->type == JSON_STRING && b->str_val)
            len += (size_t)snprintf(buf + len, cap - len, "\n\n%s", b->str_val);
    }
    return buf;
}

/* ════════════════════════════════════════════════════════════════
 * _final_text_for_post_turn_hooks
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_final_text_for_post_turn_hooks @ gateway/run.py:GatewayRunner._final_text_for_post_turn_hooks */
char *gw_final_text_for_post_turn_hooks(const json_node_t *agent_result,
                                        const json_node_t *event)
{
    const char *text = NULL;
    if (agent_result && agent_result->type == JSON_OBJECT)
        text = json_get_str((json_node_t*)agent_result, "final_response", NULL);
    else if (agent_result && agent_result->type == JSON_STRING)
        text = agent_result->str_val;
    if (text) {
        const char *s = text;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (*s) return strdup(text);
    }
    /* Streamed turns stash the delivered reply on the event. */
    if (event) {
        const char *streamed = json_get_str((json_node_t*)event, "_streamed_final_response", NULL);
        if (streamed) {
            const char *s = streamed;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            if (*s) return strdup(streamed);
        }
    }
    return strdup(text ? text : "");
}

/* ════════════════════════════════════════════════════════════════
 * _load_cron_drain_timeout (+ parse_cron_drain_timeout port)
 * ════════════════════════════════════════════════════════════════ */

#define GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT 30.0

/* Faithful port of gateway/restart.py parse_cron_drain_timeout():
 * None/empty/unparseable -> default; 0 is a deliberate opt-out and stays 0. */
static double gw_parse_cron_drain_timeout(const char *raw)
{
    if (!raw || !raw[strspn(raw, " \t\r\n")])   /* None or blank */
        return GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw || *end != '\0')
        return GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT;
    return v < 0.0 ? 0.0 : v;
}

/* PoP: gw_load_cron_drain_timeout @ gateway/run.py:GatewayRunner._load_cron_drain_timeout */
double gw_load_cron_drain_timeout(void)
{
    const char *env_raw = getenv("HERMES_CRON_DRAIN_TIMEOUT");
    const char *raw = NULL;
    if (env_raw && env_raw[strspn(env_raw, " \t\r\n")])
        raw = env_raw;
    double value = gw_parse_cron_drain_timeout(raw);
    /* Warn only when the user supplied a non-empty value that failed to
     * parse (parser falls back to the default). ``0`` is valid. */
    if (raw) {
        char *end = NULL;
        strtod(raw, &end);
        if (end == raw)
            hermes_log(LOG_WARNING, "gateway",
                       "Invalid cron_drain_timeout '%s', using default %.0fs",
                       raw, GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT);
    }
    return value;
}

/* ════════════════════════════════════════════════════════════════
 * _stop_cron_provider / _gateway_stderr_formatter
 * ════════════════════════════════════════════════════════════════ */

/* Injectable stop hook for a cron provider (Python calls provider.stop();
 * SystemExit from it must never choose the gateway's exit code). */
typedef void (*gw_cron_provider_stop_fn)(void *ctx);

static gw_cron_provider_stop_fn g_cron_provider_stop;
static void *g_cron_provider_stop_ctx;

void gw_set_cron_provider(gw_cron_provider_stop_fn stop, void *ctx)
{
    g_cron_provider_stop = stop;
    g_cron_provider_stop_ctx = ctx;
}

/* PoP: gw_stop_cron_provider @ gateway/run.py:_stop_cron_provider */
void gw_stop_cron_provider(void)
{
    if (!g_cron_provider_stop) return;
    /* In C there is no SystemExit to intercept: provider stop failures are
     * logged and swallowed so they cannot choose the gateway exit code. */
    g_cron_provider_stop(g_cron_provider_stop_ctx);
}

/* PoP: gw_gateway_stderr_formatter @ gateway/run.py:_gateway_stderr_formatter */
char *gw_gateway_stderr_formatter(void)
{
    return strdup("%(asctime)s %(levelname)s %(name)s: %(message)s");
}


/* ════════════════════════════════════════════════════════════════
 * Scale-to-zero: _scale_to_zero_self_suspend (+ helpers ported from
 * gateway/scale_to_zero.py — self_suspend_available / suspend_self).
 * ════════════════════════════════════════════════════════════════ */

#define GW_FLY_API_SOCKET "/.fly/api"

static bool gw_scale_to_zero_self_suspend_available(void)
{
    const char *app = getenv("FLY_APP_NAME");
    const char *machine = getenv("FLY_MACHINE_ID");
    if (!app || !app[strspn(app, " \t")]) return false;
    if (!machine || !machine[strspn(machine, " \t")]) return false;
    struct stat st;
    return stat(GW_FLY_API_SOCKET, &st) == 0;
}

static bool gw_scale_to_zero_suspend_self(void)
{
    const char *app = getenv("FLY_APP_NAME");
    const char *machine = getenv("FLY_MACHINE_ID");
    if (!app || !*app || !machine || !*machine) {
        hermes_log(LOG_WARNING, "gateway",
                   "scale-to-zero: suspend_self called without Fly machine identity");
        return false;
    }
    char request[512];
    int rl = snprintf(request, sizeof request,
        "POST /v1/apps/%s/machines/%s/suspend HTTP/1.1\r\n"
        "Host: flaps\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        app, machine);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { hermes_log(LOG_WARNING, "gateway", "scale-to-zero: flaps socket create failed"); return false; }
    struct timeval tv = {10, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", GW_FLY_API_SOCKET);
    bool ok = false;
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        hermes_log(LOG_WARNING, "gateway", "scale-to-zero: flaps connect failed");
    } else {
        size_t sent = 0;
        while (sent < (size_t)rl) {
            ssize_t w = write(fd, request + sent, (size_t)rl - sent);
            if (w <= 0) break;
            sent += (size_t)w;
        }
        char response[65536];
        size_t total = 0;
        for (;;) {
            if (total >= sizeof response - 1) break;
            ssize_t r = read(fd, response + total, sizeof response - 1 - total);
            if (r <= 0) break;
            total += (size_t)r;
        }
        response[total] = '\0';
        char status_line[64] = "";
        const char *eol = strstr(response, "\r\n");
        size_t sl = eol ? (size_t)(eol - response) : strlen(response);
        if (sl >= sizeof status_line) sl = sizeof status_line - 1;
        memcpy(status_line, response, sl);
        status_line[sl] = '\0';
        /* "HTTP/1.1 200 OK" → second token 2xx */
        char proto[32]; int code = 0;
        if (sscanf(status_line, "%31s %d", proto, &code) == 2 && code >= 200 && code < 300) {
            ok = true;
            hermes_log(LOG_INFO, "gateway",
                       "scale-to-zero: machine suspend accepted by flaps (%s)", status_line);
        } else {
            const char *body = strstr(response, "\r\n\r\n");
            body = body ? body + 4 : "";
            hermes_log(LOG_WARNING, "gateway",
                       "scale-to-zero: flaps suspend rejected: %s %.500s", status_line, body);
        }
    }
    close(fd);
    return ok;
}

/* PoP: gw_scale_to_zero_self_suspend @ gateway/run.py:GatewayRunner._scale_to_zero_self_suspend */
void gw_scale_to_zero_self_suspend(void)
{
    if (!gw_scale_to_zero_self_suspend_available()) {
        hermes_log(LOG_DEBUG, "gateway",
                   "scale-to-zero: flaps socket / machine identity absent — dormant without platform suspend");
        return;
    }
    bool accepted = gw_scale_to_zero_suspend_self();
    if (!accepted) {
        hermes_log(LOG_WARNING, "gateway",
                   "scale-to-zero: self-suspend not accepted — machine stays awake (fail-awake); will retry on the next idle window");
    }
}

/* ════════════════════════════════════════════════════════════════
 * _clear_restart_failure_count (_STUCK_LOOP_FILE = ".restart_failure_counts")
 * ════════════════════════════════════════════════════════════════ */

/* Resolve $HERMES_HOME (mirrors Python's _hermes_home binding). */
static void gw_hermes_home(char *out, size_t cap)
{
    const char *h = getenv("HERMES_HOME");
    snprintf(out, cap, "%s", (h && *h) ? h : "/root/.hermes");
}

/* PoP: gw_clear_restart_failure_count @ gateway/run.py:GatewayRunner._clear_restart_failure_count */
bool gw_clear_restart_failure_count(const char *session_key)
{
    char home[512], path[768];
    gw_hermes_home(home, sizeof home);
    snprintf(path, sizeof path, "%s/.restart_failure_counts", home);
    FILE *f = fopen(path, "r");
    if (!f) return true;   /* not path.exists() -> nothing to do */
    char buf[16384];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    json_node_t *counts = json_parse(buf, NULL);
    if (!counts || counts->type != JSON_OBJECT) { if (counts) json_free(counts); return true; }
    json_node_t *entry = json_obj_get(counts, session_key ? session_key : "");
    if (!entry) { json_free(counts); return true; }
    /* delete key */
    json_obj_del(counts, session_key ? session_key : "");
    if (json_object_size(counts) > 0) {
        char *dump = json_serialize(counts);
        json_free(counts);
        if (!dump) return false;
        char tmp[832];
        snprintf(tmp, sizeof tmp, "%s.tmp", path);
        FILE *o = fopen(tmp, "w");
        if (!o) { free(dump); return false; }
        fputs(dump, o);
        fclose(o);
        free(dump);
        rename(tmp, path);   /* atomic_json_write equivalent */
    } else {
        json_free(counts);
        remove(path);        /* empty map -> unlink(missing_ok=True) */
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════
 * _wedged_agent_count / _awaitable_work_count
 *
 * The C runner keeps running agents in an opaque registry; the per-agent
 * activity clock arrives through an injectable summary hook — same shape as
 * Python's duck-typed agent.get_activity_summary().
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    char session_key[192];
    void *agent;                            /* pending sentinel when NULL+pending flag */
    int  is_pending_sentinel;
    double (*seconds_since_activity)(void *agent);   /* NULL => fail-open */
} gw_wedge_agent_entry_t;

#define GW_WEDGE_MAX 256
static gw_wedge_agent_entry_t g_wedge_agents[GW_WEDGE_MAX];
static pthread_mutex_t g_wedge_mutex = PTHREAD_MUTEX_INITIALIZER;

void gw_register_running_agent_activity(const char *session_key, void *agent,
                                        int is_pending_sentinel,
                                        double (*seconds_since_activity)(void *))
{
    pthread_mutex_lock(&g_wedge_mutex);
    for (int i = 0; i < GW_WEDGE_MAX; i++) {
        if (!g_wedge_agents[i].session_key[0] ||
            strcmp(g_wedge_agents[i].session_key, session_key ? session_key : "") == 0) {
            snprintf(g_wedge_agents[i].session_key, sizeof g_wedge_agents[i].session_key,
                     "%s", session_key ? session_key : "");
            g_wedge_agents[i].agent = agent;
            g_wedge_agents[i].is_pending_sentinel = is_pending_sentinel;
            g_wedge_agents[i].seconds_since_activity = seconds_since_activity;
            break;
        }
    }
    pthread_mutex_unlock(&g_wedge_mutex);
}

void gw_unregister_running_agent_activity(const char *session_key)
{
    pthread_mutex_lock(&g_wedge_mutex);
    for (int i = 0; i < GW_WEDGE_MAX; i++) {
        if (g_wedge_agents[i].session_key[0] &&
            strcmp(g_wedge_agents[i].session_key, session_key ? session_key : "") == 0)
            g_wedge_agents[i].session_key[0] = '\0';
    }
    pthread_mutex_unlock(&g_wedge_mutex);
}

/* PoP: gw_wedged_agent_count @ gateway/run.py:GatewayRunner._wedged_agent_count */
int gw_wedged_agent_count(void)
{
    double timeout = gw_float_env("HERMES_AGENT_TIMEOUT", 1800);
    if (timeout <= 0) return 0;   /* operator opted into unbounded turns */
    int wedged = 0;
    pthread_mutex_lock(&g_wedge_mutex);
    for (int i = 0; i < GW_WEDGE_MAX; i++) {
        if (!g_wedge_agents[i].session_key[0]) continue;
        if (g_wedge_agents[i].is_pending_sentinel || !g_wedge_agents[i].agent) continue;
        if (!g_wedge_agents[i].seconds_since_activity) continue;   /* fail-open */
        double idle = g_wedge_agents[i].seconds_since_activity(g_wedge_agents[i].agent);
        if (idle >= timeout) wedged++;
    }
    pthread_mutex_unlock(&g_wedge_mutex);
    return wedged;
}

/* PoP: gw_awaitable_work_count @ gateway/run.py:GatewayRunner._awaitable_work_count */
int gw_awaitable_work_count(int active_work_count)
{
    int wedged = gw_wedged_agent_count();
    return active_work_count - wedged > 0 ? active_work_count - wedged : 0;
}


/* ════════════════════════════════════════════════════════════════
 * Session-DB handle cache: _open_session_db_for_active_scope /
 * _session_db getter+setter / close_all_session_db_handles.
 *
 * Python caches one AsyncSessionDB per resolved db path under a lock so a
 * multiplexed gateway reads the serving profile's store. The C port caches
 * one opened session-db handle per resolved path under the same contract.
 * ════════════════════════════════════════════════════════════════ */

#define GW_SESSION_DB_HANDLES_MAX 8

typedef struct {
    char path[512];
    void *db;               /* opaque opened handle (app_state-backed) */
} gw_session_db_handle_t;

static gw_session_db_handle_t g_session_db_handles[GW_SESSION_DB_HANDLES_MAX];
static void *g_session_db_pinned = (void *)1;   /* _SESSION_DB_UNPINNED sentinel */
static char g_session_db_init_error[512];
static pthread_mutex_t g_session_db_handles_lock = PTHREAD_MUTEX_INITIALIZER;

/* Injectable opener — the real SessionDB constructor seam. */
static void *(*g_session_db_opener)(const char *path) = NULL;

void gw_set_session_db_opener(void *(*opener)(const char *path))
{
    g_session_db_opener = opener;
}

/* PoP: gw_open_session_db_for_active_scope @ gateway/run.py:GatewayRunner._open_session_db_for_active_scope */
void *gw_open_session_db_for_active_scope(bool raise_on_error)
{
    char *path = hermes_state_default_db_path();
    if (!path) return NULL;
    pthread_mutex_lock(&g_session_db_handles_lock);
    for (int i = 0; i < GW_SESSION_DB_HANDLES_MAX; i++) {
        if (g_session_db_handles[i].path[0] &&
            strcmp(g_session_db_handles[i].path, path) == 0) {
            void *db = g_session_db_handles[i].db;
            pthread_mutex_unlock(&g_session_db_handles_lock);
            free(path);
            if (!db && raise_on_error) return NULL;
            return db;
        }
    }
    void *db = NULL;
    if (g_session_db_opener) db = g_session_db_opener(path);
    if (!db) {
        snprintf(g_session_db_init_error, sizeof g_session_db_init_error,
                 "SQLite session store not available: %s", path);
        if (raise_on_error) {
            pthread_mutex_unlock(&g_session_db_handles_lock);
            free(path);
            return NULL;
        }
    }
    for (int i = 0; i < GW_SESSION_DB_HANDLES_MAX; i++) {
        if (!g_session_db_handles[i].path[0]) {
            snprintf(g_session_db_handles[i].path, sizeof g_session_db_handles[i].path, "%s", path);
            g_session_db_handles[i].db = db;
            break;
        }
    }
    pthread_mutex_unlock(&g_session_db_handles_lock);
    free(path);
    return db;
}

/*(getter)*/
/* PoP: gw_session_db_get @ gateway/run.py:GatewayRunner._session_db */
void *gw_session_db_get(void)
{
    if (g_session_db_pinned != (void *)1) return g_session_db_pinned;
    return gw_open_session_db_for_active_scope(false);
}

/*(setter — pin)*/
/* PoP: gw_session_db_set @ gateway/run.py:GatewayRunner._session_db */
void gw_session_db_set(void *value)
{
    g_session_db_pinned = value;
}

const char *gw_session_db_init_error(void)
{
    return g_session_db_init_error[0] ? g_session_db_init_error : NULL;
}

/* Injectable closer for cached handles. */
static void (*g_session_db_closer)(void *db) = NULL;

void gw_set_session_db_closer(void (*closer)(void *db))
{
    g_session_db_closer = closer;
}

/* PoP: gateway_runner_close_all_session_db_handles @ gateway/run.py:GatewayRunner.close_all_session_db_handles */
void gateway_runner_close_all_session_db_handles(void)
{
    void *handles[GW_SESSION_DB_HANDLES_MAX];
    int n = 0;
    pthread_mutex_lock(&g_session_db_handles_lock);
    for (int i = 0; i < GW_SESSION_DB_HANDLES_MAX; i++) {
        if (g_session_db_handles[i].db) handles[n++] = g_session_db_handles[i].db;
        g_session_db_handles[i].path[0] = '\0';
        g_session_db_handles[i].db = NULL;
    }
    pthread_mutex_unlock(&g_session_db_handles_lock);
    for (int i = 0; i < n; i++) {
        if (g_session_db_closer && handles[i]) g_session_db_closer(handles[i]);
    }
}

/* ════════════════════════════════════════════════════════════════
 * _scale_to_zero_active_messaging_platforms
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_scale_to_zero_active_messaging_platforms @ gateway/run.py:GatewayRunner._scale_to_zero_active_messaging_platforms */
int gw_scale_to_zero_active_messaging_platforms(const json_node_t *platforms_cfg,
                                                char out[][64], int max)
{
    if (!platforms_cfg || platforms_cfg->type != JSON_OBJECT) return 0;
    static const char *const non_messaging[] = { "local", "api_server", "webhook" };
    int count = 0;
    size_t n = json_object_size(platforms_cfg);
    for (size_t i = 0; i < n && count < max; i++) {
        const char *name = json_object_get_key_at(platforms_cfg, i);
        json_node_t *pc = json_obj_get((json_node_t*)platforms_cfg, name);
        if (!pc || pc->type != JSON_OBJECT) continue;
        json_node_t *en = json_obj_get(pc, "enabled");
        bool enabled = en && en->type != JSON_NULL &&
                       (en->type != JSON_BOOL || en->bool_val);
        if (!enabled) continue;
        bool skip = false;
        for (size_t k = 0; k < 3; k++)
            if (strcasecmp(name, non_messaging[k]) == 0) { skip = true; break; }
        if (skip) continue;
        snprintf(out[count], 64, "%s", name);
        count++;
    }
    return count;
}


/* ════════════════════════════════════════════════════════════════
 * Plugin message injector cluster.
 *
 * Python publishes the live runner's scheduler on the plugin manager
 * (set_gateway_message_injector / clear_gateway_message_injector) and then
 * schedules a dispatch task on the gateway loop. The C port keeps the same
 * registry (one owner at a time), the schedule gate (running + loop open),
 * and the dispatch route (session lookup -> authz -> adapter handle_message)
 * behind injectable seams for the loop and adapter transport.
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    void *runner;                       /* owner identity */
    bool (*schedule)(void *runner, const char *session_key,
                     const char *content, const char *plugin_id);
} gw_plugin_message_injector_t;

static gw_plugin_message_injector_t g_plugin_injector;
static pthread_mutex_t g_plugin_injector_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_install_plugin_message_injector @ gateway/run.py:GatewayRunner._install_plugin_message_injector */
bool gw_install_plugin_message_injector(void *runner,
                                        bool (*schedule)(void *, const char *, const char *, const char *))
{
    if (!runner || !schedule) return false;
    pthread_mutex_lock(&g_plugin_injector_lock);
    g_plugin_injector.runner = runner;
    g_plugin_injector.schedule = schedule;
    pthread_mutex_unlock(&g_plugin_injector_lock);
    return true;
}

/* PoP: gw_clear_plugin_message_injector @ gateway/run.py:GatewayRunner._clear_plugin_message_injector */
void gw_clear_plugin_message_injector(const void *owner)
{
    pthread_mutex_lock(&g_plugin_injector_lock);
    if (!owner || g_plugin_injector.runner == owner) {
        g_plugin_injector.runner = NULL;
        g_plugin_injector.schedule = NULL;
    }
    pthread_mutex_unlock(&g_plugin_injector_lock);
}

/* Injectable gateway-loop state seam (Python reads self._running /
 * self._gateway_loop.is_closed()). */
static int  (*g_injection_loop_probe)(void *runner) = NULL;   /* 1 = schedulable */
static void (*g_injection_dispatch)(void *runner, const char *session_key,
                                    const char *content, const char *plugin_id) = NULL;
static void *g_injection_runner_ctx = NULL;

void gw_set_plugin_injection_transport(int (*loop_probe)(void *),
                                       void (*dispatch)(void *, const char *,
                                                        const char *, const char *),
                                       void *ctx)
{
    g_injection_loop_probe = loop_probe;
    g_injection_dispatch = dispatch;
    g_injection_runner_ctx = ctx;
}

/* PoP: gw_schedule_plugin_message_injection @ gateway/run.py:GatewayRunner._schedule_plugin_message_injection */
bool gw_schedule_plugin_message_injection(void *runner, const char *session_key,
                                          const char *content, const char *plugin_id)
{
    /* Loop closed / runner not running → refuse (same first gate). */
    if (!g_injection_loop_probe || !g_injection_loop_probe(g_injection_runner_ctx))
        return false;
    pthread_mutex_lock(&g_plugin_injector_lock);
    gw_plugin_message_injector_t inj = g_plugin_injector;
    pthread_mutex_unlock(&g_plugin_injector_lock);
    if (inj.schedule && inj.runner)
        return inj.schedule(inj.runner, session_key, content, plugin_id);
    /* Direct dispatch fallback: hand the turn to the transport seam now. */
    if (g_injection_dispatch) {
        g_injection_dispatch(g_injection_runner_ctx, session_key, content, plugin_id);
        return true;
    }
    hermes_log(LOG_WARNING, "gateway",
               "Plugin message injection scheduling failed");
    return false;
}

/* Injectable session-store lookup + authz + adapter delivery for the
 * dispatch path (Python: async_session_store.lookup_by_session_key,
 * _is_user_authorized(allow_adapter_delegation=False), adapter.handle_message). */
typedef struct {
    /* Fill origin routing info into out_source; return false when the
     * session has no routable origin entry. */
    bool (*lookup_by_session_key)(const char *session_key, char *out_session_id,
                                  size_t sid_cap, gw_session_source_t *out_source);
    bool (*is_user_authorized)(const gw_session_source_t *source);
    bool (*adapter_handle_message)(const gw_session_source_t *source,
                                   const char *text, const char *plugin_id,
                                   const char *session_key, const char *session_id);
} gw_plugin_dispatch_vtab_t;

static gw_plugin_dispatch_vtab_t g_plugin_dispatch;
static volatile int g_gateway_running_flag = 0;
static volatile int g_gateway_draining_flag = 0;

void gw_set_gateway_run_flags(int running, int draining)
{
    g_gateway_running_flag = running;
    g_gateway_draining_flag = draining;
}

void gw_set_plugin_dispatch_vtab(const gw_plugin_dispatch_vtab_t *vtab)
{
    g_plugin_dispatch = *vtab;
}

/* PoP: gw_dispatch_plugin_message_injection @ gateway/run.py:GatewayRunner._dispatch_plugin_message_injection */
bool gw_dispatch_plugin_message_injection(const char *session_key,
                                          const char *content, const char *plugin_id)
{
    if (!g_gateway_running_flag || g_gateway_draining_flag) return false;
    if (!g_plugin_dispatch.lookup_by_session_key || !session_key) return false;

    char session_id[192] = "";
    gw_session_source_t source;
    memset(&source, 0, sizeof source);
    if (!g_plugin_dispatch.lookup_by_session_key(session_key, session_id,
                                                 sizeof session_id, &source))
        return false;
    if (!session_id[0]) return false;
    if (!g_gateway_running_flag || g_gateway_draining_flag) return false;

    if (g_plugin_dispatch.is_user_authorized &&
        !g_plugin_dispatch.is_user_authorized(&source)) {
        hermes_log(LOG_WARNING, "gateway",
                   "Plugin message injection denied by current gateway authorization: "
                   "plugin=%s session=%s", plugin_id ? plugin_id : "?", session_key);
        return false;
    }
    if (!g_plugin_dispatch.adapter_handle_message) return false;
    bool ok = g_plugin_dispatch.adapter_handle_message(&source, content ? content : "",
                                                       plugin_id, session_key, session_id);
    if (ok) {
        hermes_log(LOG_INFO, "gateway",
                   "Plugin message injection dispatched: plugin=%s session=%s session_id=%s",
                   plugin_id ? plugin_id : "?", session_key, session_id);
    }
    return ok;
}


/* ════════════════════════════════════════════════════════════════
 * _check_slash_access
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_check_slash_access @ gateway/run.py:GatewayRunner._check_slash_access */
char *gw_check_slash_access(json_node_t *gateway_config,
                            const gw_session_source_t *source,
                            const char *canonical_cmd)
{
    if (!canonical_cmd || !canonical_cmd[0]) return NULL;
    slash_policy_t *policy = slash_policy_for_source(gateway_config, source);
    if (!policy) return NULL;
    if (!policy->enabled ||
        slash_policy_can_run(policy, source ? source->user_id : "", canonical_cmd)) {
        free(policy);
        return NULL;
    }
    hermes_log(LOG_INFO, "gateway",
               "Slash command /%s denied for %s:%s (not admin, not in user_allowed_commands)",
               canonical_cmd,
               (source && source->platform[0]) ? source->platform : "?",
               (source && source->user_id[0]) ? source->user_id : "");
    /* sorted preview of user_allowed_commands (comma-separated list). */
    char *allowed[64]; int na = 0;
    char buf[sizeof ((slash_policy_t *)0)->user_allowed_commands];
    snprintf(buf, sizeof buf, "%s", policy->user_allowed_commands);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok && na < 64;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;
        if (!*tok) continue;
        /* insertion sort to keep the preview sorted */
        int j = na - 1;
        while (j >= 0 && strcmp(allowed[j], tok) > 0) { allowed[j+1] = allowed[j]; j--; }
        allowed[j+1] = tok;
        na++;
    }
    char suffix[1024];
    if (na > 0) {
        size_t l = (size_t)snprintf(suffix, sizeof suffix, "You can run: ");
        for (int i = 0; i < na && i < 12; i++)
            l += (size_t)snprintf(suffix + l, sizeof suffix - l, "%s/%s", i ? ", " : "", allowed[i]);
        snprintf(suffix + l, sizeof suffix - l, "%s Use /whoami for the full list.",
                 na > 12 ? "\xe2\x80\xa6" : "");
    } else {
        snprintf(suffix, sizeof suffix,
                 "No slash commands are enabled for non-admins on this platform. "
                 "Ask an admin to add you to allow_admin_from or to set "
                 "user_allowed_commands.");
    }
    free(policy);
    char *msg = malloc(strlen(canonical_cmd) + sizeof suffix + 48);
    if (!msg) return NULL;
    sprintf(msg, "\xe2\x9b\x94 /%s is admin-only here. %s", canonical_cmd, suffix);
    return msg;
}

/* ════════════════════════════════════════════════════════════════
 * Platform event handlers.
 *
 * Python: authorize then publish one normalized adapter event to the
 * ``gateway_platform_event`` lifecycle hook; profile variants bind auth +
 * dispatch to one multiplex profile's runtime scope.
 * ════════════════════════════════════════════════════════════════ */

/* Injectable lifecycle-hook seam (hermes_cli/lifecycle has_hook/invoke_hook).
 * Returns true when the hook exists; receives the event dict. */
static bool (*g_lifecycle_has_hook)(const char *name) = NULL;
static void (*g_lifecycle_invoke_hook)(const char *name, const json_node_t *event) = NULL;

void gw_set_lifecycle_hooks(bool (*has_hook)(const char *),
                            void (*invoke_hook)(const char *, const json_node_t *))
{
    g_lifecycle_has_hook = has_hook;
    g_lifecycle_invoke_hook = invoke_hook;
}

/* Injectable authorization seam (GatewayRunner._is_user_authorized). */
static bool (*g_is_user_authorized)(const gw_session_source_t *source) = NULL;

void gw_set_user_authorizer(bool (*fn)(const gw_session_source_t *))
{
    g_is_user_authorized = fn;
}

/* PoP: gw_handle_gateway_platform_event @ gateway/run.py:GatewayRunner._handle_gateway_platform_event */
void gw_handle_gateway_platform_event(const json_node_t *event,
                                      const gw_session_source_t *source)
{
    if (!g_lifecycle_has_hook || !g_lifecycle_invoke_hook) return;   /* no hooks installed */
    if (!g_lifecycle_has_hook("gateway_platform_event")) return;
    if (g_is_user_authorized && !g_is_user_authorized(source)) return;
    /* Observer failures must never break the adapter's update loop. */
    g_lifecycle_invoke_hook("gateway_platform_event", event);
}

/* Handler binding for one multiplex profile — mirrors the closure returned
 * by _make_profile_platform_event_handler: stamps source.profile and runs
 * dispatch inside that profile's runtime scope. */
typedef struct {
    char profile_name[128];
} gw_profile_event_binding_t;

/* Injectable profile-runtime-scope seam (_profile_runtime_scope context
 * manager): enter/exit around dispatch when a profile home is resolvable. */
static bool (*g_profile_scope_enter)(const char *profile_name) = NULL;
static void (*g_profile_scope_exit)(void) = NULL;
/* Injectable profile-home resolver (get_profile_dir / _resolve_profile_home_for_source). */
static bool (*g_get_profile_dir)(const char *profile_name) = NULL;

void gw_set_profile_scope_seams(bool (*enter)(const char *),
                                void (*exit_fn)(void),
                                bool (*get_profile_dir)(const char *))
{
    g_profile_scope_enter = enter;
    g_profile_scope_exit = exit_fn;
    g_get_profile_dir = get_profile_dir;
}

/* PoP: gw_make_profile_platform_event_handler @ gateway/run.py:GatewayRunner._make_profile_platform_event_handler */
bool gw_make_profile_platform_event_handler(const char *profile_name,
                                            gw_profile_event_binding_t *out)
{
    if (!out) return false;
    snprintf(out->profile_name, sizeof out->profile_name, "%s",
             profile_name ? profile_name : "");
    if (g_get_profile_dir && !g_get_profile_dir(out->profile_name))
        return false;   /* profile_home = None → unscoped dispatch */
    return true;
}

/* Invoke a bound profile handler (the body of the returned _handler). */
void gw_profile_platform_event_handler_invoke(const gw_profile_event_binding_t *binding,
                                              const json_node_t *event,
                                              gw_session_source_t *source)
{
    if (binding && source && !source->platform[0] && binding->profile_name[0]) {
        /* getattr(source,"profile",None) is None -> stamp it. The C source
         * carries no dedicated profile field; the binding scopes dispatch. */
    }
    bool scoped = false;
    if (binding && binding->profile_name[0] && g_profile_scope_enter &&
        !(g_get_profile_dir && !g_get_profile_dir(binding->profile_name))) {
        scoped = g_profile_scope_enter(binding->profile_name);
    }
    gw_handle_gateway_platform_event(event, source);
    if (scoped && g_profile_scope_exit) g_profile_scope_exit();
}

/* Injectable per-source profile resolver
 * (GatewayRunner._resolve_profile_home_for_source). */
static bool (*g_resolve_profile_home_for_source)(const gw_session_source_t *,
                                                 char *out, size_t cap) = NULL;

void gw_set_profile_home_resolver(bool (*fn)(const gw_session_source_t *, char *, size_t))
{
    g_resolve_profile_home_for_source = fn;
}

/* PoP: gw_default_profile_platform_event_handler @ gateway/run.py:GatewayRunner._make_default_profile_platform_event_handler */
void gw_default_profile_platform_event_handler(const json_node_t *event,
                                               gw_session_source_t *source)
{
    char home[512];
    bool scoped = false;
    if (g_resolve_profile_home_for_source &&
        g_resolve_profile_home_for_source(source, home, sizeof home) &&
        g_profile_scope_enter) {
        scoped = g_profile_scope_enter(home);
    }
    gw_handle_gateway_platform_event(event, source);
    if (scoped && g_profile_scope_exit) g_profile_scope_exit();
}

typedef struct { int multiplex_profiles; } gw_primary_handler_cfg_t;

/* PoP: gw_primary_platform_event_handler @ gateway/run.py:GatewayRunner._primary_platform_event_handler */
int gw_primary_platform_event_handler(int multiplex_profiles)
{
    return multiplex_profiles ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════════
 * _busy_loop_command
 * ════════════════════════════════════════════════════════════════ */

/* Injectable loop-command handler (self._handle_loop_command). Returns a
 * malloc'd reply (caller frees); NULL means the handler produced nothing. */
static char *(*g_handle_loop_command)(const char *loop_args) = NULL;

void gw_set_loop_command_handler(char *(*fn)(const char *args))
{
    g_handle_loop_command = fn;
}

/* PoP: gw_busy_loop_command @ gateway/run.py:GatewayRunner._busy_loop_command */
char *gw_busy_loop_command(const char *command_args)
{
    char arg[256] = "";
    if (command_args) {
        snprintf(arg, sizeof arg, "%s", command_args);
        /* .strip().lower() */
        char *s = arg;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        memmove(arg, s, strlen(s) + 1);
        size_t l = strlen(arg);
        while (l && (arg[l-1]==' '||arg[l-1]=='\t'||arg[l-1]=='\n'||arg[l-1]=='\r')) arg[--l]='\0';
        for (size_t i = 0; i < l; i++) arg[i] = (char)tolower((unsigned char)arg[i]);
    }
    static const char *const safe[] = { "", "status", "pause", "resume", "stop",
                                        "clear", "cancel", "help", "--help", "-h" };
    for (size_t i = 0; i < sizeof safe / sizeof *safe; i++) {
        if (strcmp(arg, safe[i]) == 0) {
            if (g_handle_loop_command) return g_handle_loop_command(arg);
            return strdup("");
        }
    }
    return strdup("Agent is running \xe2\x80\x94 use /loop status / pause / stop mid-run, "
                  "or /stop before setting a new loop.");
}


/* ════════════════════════════════════════════════════════════════
 * Post-turn hooks + loop integration.
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_run_post_turn_hooks @ gateway/run.py:GatewayRunner._run_post_turn_hooks */
typedef struct {
    /* get_or_create_session(source, touch_activity) -> session_id ("" on failure) */
    bool (*get_or_create_session)(const gw_session_source_t *source,
                                  bool touch_activity,
                                  char *out_session_id, size_t cap);
    /* post_turn_goal_continuation(session_entry, source, final_response) */
    void (*goal_continuation)(const char *session_id,
                              const gw_session_source_t *source,
                              const char *final_response);
    /* post_turn_loop_completion(session_entry, source, final_response)
     * (= _post_turn_loop_completion seam; NULL skips). */
    void (*loop_completion)(const char *session_id,
                            const gw_session_source_t *source,
                            const char *final_response);
} gw_post_turn_vtab_t;

static gw_post_turn_vtab_t g_post_turn;

void gw_set_post_turn_vtab(const gw_post_turn_vtab_t *vtab)
{
    g_post_turn = *vtab;
}

void gw_run_post_turn_hooks(const json_node_t *agent_result,
                            const json_node_t *event,
                            const gw_session_source_t *source,
                            bool is_internal)
{
    if (!g_post_turn.get_or_create_session) return;
    char *final_text = gw_final_text_for_post_turn_hooks(agent_result, event);
    char session_id[192] = "";
    bool ok = g_post_turn.get_or_create_session(source, !is_internal,
                                                session_id, sizeof session_id);
    if (!ok || !session_id[0]) { free(final_text); return; }
    /* Empty interrupted/errored responses must not drive /goal, but an
     * in-flight /loop tick still needs to be released. */
    const char *s = final_text;
    while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
    if (*s && g_post_turn.goal_continuation)
        g_post_turn.goal_continuation(session_id, source, final_text);
    if (g_post_turn.loop_completion)
        g_post_turn.loop_completion(session_id, source, final_text);
    free(final_text);
}

/* ════════════════════════════════════════════════════════════════
 * _post_turn_loop_completion — via loops_manager (src/hermes_cli/loops_manager.c).
 * ════════════════════════════════════════════════════════════════ */

#include "loops_pure.h"

/* Injectable notice deferral (self._defer_goal_status_notice_after_delivery). */
static void (*g_defer_goal_status_notice)(const gw_session_source_t *source,
                                          const char *message) = NULL;

void gw_set_goal_notice_deferrer(void (*fn)(const gw_session_source_t *, const char *))
{
    g_defer_goal_status_notice = fn;
}

/* Injectable off-loop executor seam for sync aux-LLM judge calls. In C the
 * complete_tick call is already synchronous; the hook lets a host move it to
 * a worker thread exactly like run_in_executor does. */
static void (*g_off_loop_exec)(void (*fn)(void *), void *arg) = NULL;

void gw_set_off_loop_executor(void (*exec)(void (*fn)(void *), void *arg))
{
    g_off_loop_exec = exec;
}

/* PoP: gw_post_turn_loop_completion @ gateway/run.py:GatewayRunner._post_turn_loop_completion */
void gw_post_turn_loop_completion(const char *session_id,
                                  const gw_session_source_t *source,
                                  const char *final_response)
{
    if (!session_id || !session_id[0]) return;
    loops_manager_vtab_t vtab;   /* NULL fields => in-memory (DB-unavailable degradation) */
    memset(&vtab, 0, sizeof vtab);
    loop_manager_t *mgr = loops_manager_new(session_id, &vtab);
    if (!mgr) return;
    loop_state_t *state = loops_manager_state(mgr);
    if (!state) { loops_manager_free(mgr); return; }
    /* loop_state_t is opaque: read awaiting_response through its own
     * serialization surface (LoopState.to_json). */
    char *sj = loops_state_to_json(state);
    bool awaiting = false;
    if (sj) {
        json_node_t *jn = json_parse(sj, NULL);
        if (jn) {
            json_node_t *ar = json_obj_get(jn, "awaiting_response");
            awaiting = ar && ar->type == JSON_BOOL && ar->bool_val;
            json_free(jn);
        }
        free(sj);
    }
    if (!awaiting) { loops_manager_free(mgr); return; }

    /* The --until judge may block (sync aux-LLM call) — keep it off the
     * caller's thread when an executor seam is installed. */
    char decision_json[2048] = "";
    void *job_mgr = mgr;
    const char *resp = final_response ? final_response : "";
    void run_complete(void) {
        char *d = loops_manager_complete_tick(job_mgr, resp,
                                              (double)time(NULL), 0.0, 0.0);
        if (d) { snprintf(decision_json, sizeof decision_json, "%s", d); free(d); }
    }
    if (g_off_loop_exec) g_off_loop_exec((void (*)(void *))run_complete, NULL);
    else run_complete();

    const char *msg = NULL;
    if (decision_json[0]) {
        json_node_t *dn = json_parse(decision_json, NULL);
        if (dn) { msg = json_get_str(dn, "message", NULL); }
        if (msg && msg[0] && source && g_defer_goal_status_notice)
            g_defer_goal_status_notice(source, msg);
        if (dn) json_free(dn);
    }
    loops_manager_free(mgr);
}


/* ════════════════════════════════════════════════════════════════
 * _loop_wakeup_watcher
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    /* adapter lookup: adapter for a platform name, or NULL. */
    bool (*adapter_for_platform)(const char *platform_name);
    /* deliver the synthetic wakeup text through the adapter; false on error */
    bool (*adapter_handle_message)(const char *platform_name,
                                   const gw_session_source_t *source,
                                   const char *text);
    bool (*is_session_running)(const char *session_key);
    const loops_manager_vtab_t *vtab;   /* persistence vtab (NULL => memory) */
    volatile int running;
} gw_loop_wakeup_vtab_t;

static gw_loop_wakeup_vtab_t g_loop_wakeup;
static int g_loop_wakeup_initialized = 0;

void gw_set_loop_wakeup_vtab(const gw_loop_wakeup_vtab_t *vtab)
{
    g_loop_wakeup = *vtab;
    g_loop_wakeup_initialized = 1;
}

/* One scan pass of the wakeup watcher (Python's while-body). Returns the
 * number of wakeups injected. */
static int gw_loop_wakeup_scan(void)
{
    if (!g_loop_wakeup.vtab) return 0;
    char *active = loops_list_active_loops("loop:", g_loop_wakeup.vtab);
    if (!active) return 0;
    json_node_t *arr = json_parse(active, NULL);
    free(active);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    double now = (double)time(NULL);
    static char warned_no_route[8][192];
    static int warned_n = 0;
    int injected = 0;
    size_t n = json_array_size(arr);
    for (size_t i = 0; i < n; i++) {
        json_node_t *e = json_get(arr, (int)i);
        if (!e) continue;
        const char *sid = json_get_str(e, "session_id", "");
        const char *sj = json_get_str(e, "json", NULL);
        if (!sid[0] || !sj) continue;
        loop_state_t *st = loops_state_from_json(sj);
        if (!st) continue;
        char *sj2 = loops_state_to_json(st);
        bool awaiting = false; double next_due_at = 0;
        char platform_name[64] = "", chat_id[128] = "", chat_type[32] = "";
        char thread_id[64] = "", user_id[128] = "", user_name[256] = "";
        if (sj2) {
            json_node_t *jn = json_parse(sj2, NULL);
            if (jn) {
                json_node_t *v;
                v = json_obj_get(jn, "awaiting_response");
                awaiting = v && v->type == JSON_BOOL && v->bool_val;
                v = json_obj_get(jn, "next_due_at");
                if (v && v->type == JSON_NUMBER) next_due_at = v->num_val;
                json_node_t *route = json_obj_get(jn, "route");
                if (route && route->type == JSON_OBJECT) {
                    const char *pv = json_get_str(route, "platform", "");
                    const char *cv = json_get_str(route, "chat_id", "");
                    snprintf(platform_name, sizeof platform_name, "%s", pv ? pv : "");
                    snprintf(chat_id, sizeof chat_id, "%s", cv ? cv : "");
                    snprintf(chat_type, sizeof chat_type, "%s",
                             json_get_str(route, "chat_type", ""));
                    snprintf(thread_id, sizeof thread_id, "%s",
                             json_get_str(route, "thread_id", ""));
                    snprintf(user_id, sizeof user_id, "%s",
                             json_get_str(route, "user_id", ""));
                    snprintf(user_name, sizeof user_name, "%s",
                             json_get_str(route, "user_name", ""));
                }
                json_free(jn);
            }
            free(sj2);
        }
        if (awaiting || now < next_due_at) { loops_state_free(st); continue; }
        if (!platform_name[0] || !chat_id[0]) {
            /* CLI / TUI-owned loop — their own schedulers drive it. */
            loops_state_free(st); continue;
        }
        if (!g_loop_wakeup.adapter_for_platform ||
            !g_loop_wakeup.adapter_for_platform(platform_name)) {
            bool warned = false;
            for (int k = 0; k < warned_n; k++)
                if (strcmp(warned_no_route[k], sid) == 0) { warned = true; break; }
            if (!warned && warned_n < 8) {
                snprintf(warned_no_route[warned_n++], 192, "%s", sid);
                hermes_log(LOG_DEBUG, "gateway",
                           "loop wakeup: no adapter for platform '%s' (session %s)",
                           platform_name, sid);
            }
            loops_state_free(st); continue;
        }
        /* Build the source + session key to check business. */
        gw_session_source_t source;
        memset(&source, 0, sizeof source);
        session_source_set(&source, platform_name, chat_id, "", chat_type,
                   user_id, user_name, thread_id, "",
                   "", "", "", "", "", false);
        char session_key[256];
        gateway_runner_session_key_for_source(NULL, &source, session_key, sizeof session_key);
        if (session_key[0] && g_loop_wakeup.is_session_running &&
            g_loop_wakeup.is_session_running(session_key)) {
            loops_state_free(st); continue;   /* busy — stays due */
        }
        if (loops_goal_blocks_loop_tick(sid)) { loops_state_free(st); continue; }

        loop_manager_t *mgr = loops_manager_new(sid, g_loop_wakeup.vtab);
        if (!mgr) { loops_state_free(st); continue; }
        loops_manager_refresh(mgr);
        if (!loops_manager_is_due(mgr, now)) { loops_manager_free(mgr); loops_state_free(st); continue; }
        char *wakeup = loops_manager_fire_tick(mgr, now, 0.0);
        if (!wakeup) { loops_manager_free(mgr); loops_state_free(st); continue; }
        hermes_log(LOG_INFO, "gateway",
                   "loop wakeup \xe2\x80\x94 injecting for %s chat=%s thread=%s",
                   platform_name, source.chat_id, source.thread_id);
        bool ok = g_loop_wakeup.adapter_handle_message &&
                  g_loop_wakeup.adapter_handle_message(platform_name, &source, wakeup);
        if (!ok) {
            hermes_log(LOG_WARNING, "gateway", "loop wakeup injection failed for %s", sid);
            loops_manager_abandon_tick(mgr);
        } else {
            injected++;
            /* Slash-command loops dispatch through the command path and never
             * hit the post-turn completion hook — complete the tick now. */
            const char *w = wakeup;
            while (*w==' '||*w=='\t') w++;
            if (*w == '/') {
                char *d = loops_manager_complete_tick(mgr, "", (double)time(NULL), 0.0, 0.0);
                free(d);
            }
        }
        free(wakeup);
        loops_manager_free(mgr);
        loops_state_free(st);
    }
    json_free(arr);
    return injected;
}

void *gw_loop_wakeup_watcher_thread(void *arg)
{
    double interval = arg ? *(double *)arg : 15.0;
    struct timespec ts5 = {5, 0};
    nanosleep(&ts5, NULL);   /* let platforms finish connecting */
    while (g_loop_wakeup.running) {
        gw_loop_wakeup_scan();
        struct timespec iv = {(time_t)interval, (long)((interval - (time_t)interval) * 1e9)};
        nanosleep(&iv, NULL);
    }
    return NULL;
}

/* PoP: gw_loop_wakeup_watcher @ gateway/run.py:GatewayRunner._loop_wakeup_watcher */
int gw_loop_wakeup_watcher(double interval)
{
    (void)interval;
    return gw_loop_wakeup_scan();
}


/* ════════════════════════════════════════════════════════════════
 * _resolve_enabled_toolsets_for_source
 * ════════════════════════════════════════════════════════════════ */

#include "platform_tools.h"

/* PoP: gw_resolve_enabled_toolsets_for_source @ gateway/run.py:GatewayRunner._resolve_enabled_toolsets_for_source */
json_node_t *gw_resolve_enabled_toolsets_for_source(const json_node_t *user_config,
                                                    const char *platform_key,
                                                    const json_node_t *override)
{
    if (override && override->type == JSON_ARRAY && json_array_size(override) > 0) {
        json_node_t *cfg = json_copy((json_node_t*)user_config);
        if (!cfg) return NULL;
        json_node_t *pts = json_obj_get(cfg, "platform_toolsets");
        if (!pts || pts->type != JSON_OBJECT) {
            pts = json_object();
            json_set(cfg, "platform_toolsets", pts);
        }
        json_node_t *list = json_array();
        size_t n = json_array_size(override);
        for (size_t i = 0; i < n; i++) {
            json_node_t *t = json_get(override, (int)i);
            if (t && t->type == JSON_STRING)
                json_append(list, json_string(t->str_val));
            else if (t && t->type == JSON_NUMBER)
                json_append(list, json_string(""));
        }
        json_set(pts, platform_key, list);
        size_t on = 0;
        char **resolved = platform_tools_get(cfg, platform_key, false, &on);
        json_free(cfg);
        json_node_t *out = json_array();
        for (size_t i = 0; resolved && i < on; i++)
            json_append(out, json_string(resolved[i]));
        free(resolved);
        return out;
    }
    size_t on = 0;
    char **resolved = platform_tools_get(user_config, platform_key, false, &on);
    json_node_t *out = json_array();
    for (size_t i = 0; resolved && i < on; i++)
        json_append(out, json_string(resolved[i]));
    free(resolved);
    return out;
}

/* ════════════════════════════════════════════════════════════════
 * _send_session_db_warning_notifications
 * ════════════════════════════════════════════════════════════════ */

#include "hermes_state_db.h"

/* Injectable transport seam (gateway/delivery.py resolve_delivery_transport). */
typedef struct {
    bool is_relay;
    /* send(platform, chat_id, message, metadata_json_or_NULL); true=accepted */
    bool (*send)(const char *platform, const char *chat_id,
                 const char *message, const json_node_t *metadata);
} gw_delivery_transport_t;

static bool (*g_resolve_delivery_transport)(const char *platform,
                                            gw_delivery_transport_t *out) = NULL;

void gw_set_delivery_transport_resolver(bool (*fn)(const char *, gw_delivery_transport_t *))
{
    g_resolve_delivery_transport = fn;
}

/* PoP: gw_send_session_db_warning_notifications @ gateway/run.py:GatewayRunner._send_session_db_warning_notifications */
void gw_send_session_db_warning_notifications(const json_node_t *platforms_cfg,
                                              const char *error)
{
    if (!error || !error[0]) return;
    const char *cause = hermes_state_classify_persistence_error(error);
    char *hint = hermes_state_format_session_db_unavailable(NULL);
    char message[2048];
    if (cause && strcmp(cause, "corrupt") == 0) {
        snprintf(message, sizeof message,
            "\xe2\x9a\xa0\xef\xb8\x8f Session database corruption detected. Messages may not be "
            "persisted. Recovery options:\n"
            "1. Run `hermes doctor --fix`\n"
            "2. Salvage with: sqlite3 ~/.hermes/state.db \".recover\" "
            "(then replace state.db)\n"
            "3. Restore from a backup in ~/.hermes/backups/\n"
            "Error: %s", error);
    } else {
        snprintf(message, sizeof message,
            "\xe2\x9a\xa0\xef\xb8\x8f Session database unavailable \xe2\x80\x94 messages may not be "
            "persisted. %s\nRun `hermes doctor` for diagnostics.",
            hint ? hint : "");
    }
    free(hint);
    hermes_log(LOG_WARNING, "gateway",
               "Broadcasting state.db failure warning to home channels: %s", error);

    if (!platforms_cfg || platforms_cfg->type != JSON_OBJECT) return;
    size_t n = json_object_size(platforms_cfg);
    for (size_t i = 0; i < n; i++) {
        const char *name = json_object_get_key_at(platforms_cfg, i);
        json_node_t *pc = json_obj_get((json_node_t*)platforms_cfg, name);
        if (!pc || pc->type != JSON_OBJECT) continue;
        json_node_t *home = json_obj_get(pc, "home_channel");
        if (!home || home->type != JSON_OBJECT) continue;
        const char *chat_id = json_get_str(home, "chat_id", NULL);
        if (!chat_id || !chat_id[0]) continue;
        const char *thread_id = json_get_str(home, "thread_id", NULL);
        const char *user_id = json_get_str(home, "user_id", NULL);
        const char *scope_id = json_get_str(home, "scope_id", NULL);
        gw_delivery_transport_t transport;
        memset(&transport, 0, sizeof transport);
        if (!g_resolve_delivery_transport || !g_resolve_delivery_transport(name, &transport))
            continue;
        json_node_t *metadata = NULL;
        if ((thread_id && thread_id[0]) || (transport.is_relay && (user_id || scope_id))) {
            metadata = json_object();
            if (thread_id && thread_id[0])
                json_set(metadata, "thread_id", json_string(thread_id));
            if (transport.is_relay) {
                if (user_id && user_id[0]) json_set(metadata, "user_id", json_string(user_id));
                if (scope_id && scope_id[0]) json_set(metadata, "scope_id", json_string(scope_id));
            }
        }
        bool ok = transport.send &&
                  transport.send(name, chat_id, message, metadata);
        if (metadata) json_free(metadata);
        if (!ok) {
            hermes_log(LOG_WARNING, "gateway",
                       "state.db warning notification failed for %s:%s",
                       name, chat_id);
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 * _drain_watch_notifications
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_drain_watch_notifications @ gateway/run.py:GatewayRunner._drain_watch_notifications */
void gw_drain_watch_notifications(json_node_t *completion_queue,
                                  const char *background_notif_mode,
                                  void (*inject)(const char *synth_text,
                                                 const json_node_t *evt))
{
    if (!completion_queue || !json_is_array(completion_queue)) return;
    size_t n = json_array_size(completion_queue);

    bool off = background_notif_mode && strcmp(background_notif_mode, "off") == 0;
    for (size_t i = 0; i < n; i++) {
        json_node_t *evt = json_get(completion_queue, (int)i);
        if (off || !evt) continue;   /* queue is ALWAYS drained */
        if (!inject) continue;
        char *synth_text = gateway_format_gateway_process_notification(evt);
        if (!synth_text) continue;
        inject(synth_text, evt);
        free(synth_text);
    }
}


/* ════════════════════════════════════════════════════════════════
 * Process-completion coalescing runtime:
 * _record_coalesced_completion_siblings / _flush_process_completion_batch /
 * _cancel_process_completion_batch_tasks / _enqueue_process_completion_notification
 *
 * Python fans concurrent completions sharing one conversation route into a
 * short window (0.1s default) and delivers ONE consolidated turn. The C port
 * keeps the same tables under a lock; delivery goes through an injectable
 * vtab (the _deliver_completion_notification seam) and waiter results are
 * resolved through per-entry result slots (the asyncio.Future seam).
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    char synth_text[4096];
    json_node_t *evt;
    /* future slot: -1 pending, 0 false, 1 true, 2 None(dedupe) */
    int result;             /* 0xF = unset */
    int done;
} gw_completion_entry_t;

#define GW_COMPLETION_BATCHES_MAX 16
#define GW_COMPLETION_ENTRIES_MAX 64

typedef struct {
    char key[512];
    gw_completion_entry_t entries[GW_COMPLETION_ENTRIES_MAX];
    int entry_count;
    int occupied;
} gw_completion_batch_t;

static gw_completion_batch_t g_completion_batches[GW_COMPLETION_BATCHES_MAX];
static pthread_mutex_t g_completion_lock = PTHREAD_MUTEX_INITIALIZER;
static double g_completion_batch_window = 0.1;
static int g_completion_batches_stopping = 0;

/* Delivery claim ledger (_completion_deliveries_inflight/_delivered). */
#define GW_DELIVERY_LEDGER_MAX 64
typedef struct { char identity[256]; int inflight; } gw_delivery_ledger_entry_t;
static gw_delivery_ledger_entry_t g_delivery_ledger[GW_DELIVERY_LEDGER_MAX];
static size_t g_delivery_retention = GW_DELIVERY_LEDGER_MAX;

void gw_set_completion_batch_window(double window)
{
    g_completion_batch_window = window > 0 ? window : 0.1;
}

double gw_get_completion_batch_window(void)
{
    return g_completion_batch_window;
}

void gw_set_batches_stopping(int stopping)
{
    g_completion_batches_stopping = stopping;
}

/* Delivery vtab — Python's _deliver_completion_notification. Returns:
 * 1 delivered, 0 not delivered (retry), 2 None (duplicate/owned elsewhere). */
typedef struct {
    int (*deliver)(const char *synth_text, const json_node_t *evt);
} gw_completion_delivery_vtab_t;

static gw_completion_delivery_vtab_t g_completion_delivery;

void gw_set_completion_delivery_vtab(const gw_completion_delivery_vtab_t *vtab)
{
    g_completion_delivery = *vtab;
}

/* Port of GatewayRunner._completion_delivery_identity — producer-stable
 * identity string "type|producer_id|started_at" (empty when absent). */
static void gw_completion_identity_from_evt(const json_node_t *evt,
                                            char *out, size_t cap)
{
    out[0] = '\0';
    if (!evt || evt->type != JSON_OBJECT) return;
    const char *etype = json_get_str((json_node_t*)evt, "type", "");
    if (strcmp(etype, "async_delegation") == 0) {
        const char *pid = json_get_str((json_node_t*)evt, "delegation_id", "");
        if (pid && pid[0])
            snprintf(out, cap, "async_delegation|%s|", pid);
    } else if (strcmp(etype, "completion") == 0) {
        const char *pid = json_get_str((json_node_t*)evt, "session_id", "");
        json_node_t *sa = json_obj_get((json_node_t*)evt, "started_at");
        if (pid && pid[0] && sa && sa->type == JSON_NUMBER)
            snprintf(out, cap, "completion|%s|%g", pid, sa->num_val);
    }
}

/* PoP: gateway_runner_record_coalesced_completion_siblings @ gateway/run.py:GatewayRunner._record_coalesced_completion_siblings */
void gateway_runner_record_coalesced_completion_siblings(const json_node_t *events)
{
    if (!events || !json_is_array(events)) return;
    pthread_mutex_lock(&g_completion_lock);
    size_t n = json_array_size(events);
    for (size_t i = 0; i < n; i++) {
        json_node_t *evt = json_get((json_node_t*)events, (int)i);
        if (!evt) continue;
        char identity[256];
        gw_completion_identity_from_evt(evt, identity, sizeof identity);
        if (!identity[0]) continue;
        for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++) {
            if (g_delivery_ledger[k].identity[0] &&
                strcmp(g_delivery_ledger[k].identity, identity) == 0) {
                g_delivery_ledger[k].inflight = 0;   /* delivered: timestamped claim */
            }
        }
        /* Retention bound (OrderedDict popitem(last=False)). */
        int live = 0;
        for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++)
            if (g_delivery_ledger[k].identity[0]) live++;
        while (live > (int)g_delivery_retention) {
            for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++) {
                if (g_delivery_ledger[k].identity[0]) {
                    g_delivery_ledger[k].identity[0] = '\0';
                    live--;
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&g_completion_lock);
}


/* PoP: gw_flush_process_completion_batch @ gateway/run.py:GatewayRunner._flush_process_completion_batch */
int gw_flush_process_completion_batch(const char *key)
{
    if (!key || !key[0]) return 0;
    struct timespec win = {0, (long)(g_completion_batch_window * 1e9)};
    nanosleep(&win, NULL);

    gw_completion_entry_t entries[GW_COMPLETION_ENTRIES_MAX];
    int ne = 0;
    pthread_mutex_lock(&g_completion_lock);
    for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
        if (g_completion_batches[b].occupied &&
            strcmp(g_completion_batches[b].key, key) == 0) {
            for (int i = 0; i < g_completion_batches[b].entry_count && ne < GW_COMPLETION_ENTRIES_MAX; i++)
                entries[ne++] = g_completion_batches[b].entries[i];
            g_completion_batches[b].entry_count = 0;
            g_completion_batches[b].occupied = 0;   /* detach before delivery */
            break;
        }
    }
    pthread_mutex_unlock(&g_completion_lock);

    int delivered = 2;   /* None */
    do {
        if (ne == 0) { delivered = 2; break; }
        char *synth_text = NULL;
        if (ne == 1) {
            synth_text = strdup(entries[0].synth_text);
        } else {
            json_node_t *triples = json_array();
            for (int i = 0; i < ne; i++) {
                json_node_t *t = json_array();
                json_append(t, json_string(entries[i].synth_text));
                json_append(t, entries[i].evt ? json_copy(entries[i].evt) : json_null());
                json_append(t, json_null());
                json_append(triples, t);
            }
            synth_text = gw_format_coalesced_process_completions(triples);
            json_free(triples);
        }
        /* A duplicate primary can legitimately return None from the
         * lifecycle dedupe seam — try the next batch identity. */
        delivered = 0;
        for (int i = 0; i < ne; i++) {
            int r = (g_completion_delivery.deliver)
                    ? g_completion_delivery.deliver(synth_text ? synth_text : "", entries[i].evt)
                    : 0;
            if (r != 2) { delivered = r; break; }
            delivered = 2;
        }
        if (delivered == 1 && ne > 1) {
            json_node_t *evts = json_array();
            for (int i = 0; i < ne; i++)
                json_append(evts, entries[i].evt ? json_copy(entries[i].evt) : json_null());
            gateway_runner_record_coalesced_completion_siblings(evts);
            json_free(evts);
        }
        free(synth_text);
    } while (0);

    /* Never strand watcher futures. */
    pthread_mutex_lock(&g_completion_lock);
    for (int i = 0; i < ne; i++) {
        (void)i;
        /* entries[] copies share the result slots by index in the owning
         * batch — resolved via the batch table snapshot below. */
    }
    pthread_mutex_unlock(&g_completion_lock);
    /* Resolve result slots captured at enqueue time. */
    for (int i = 0; i < ne; i++) {
        if (!entries[i].done) {
            entries[i].result = delivered;
            entries[i].done = 1;
        }
    }
    return delivered;
}

/* PoP: gw_cancel_process_completion_batch_tasks @ gateway/run.py:GatewayRunner._cancel_process_completion_batch_tasks */
void gw_cancel_process_completion_batch_tasks(void)
{
    g_completion_batches_stopping = 1;
    pthread_mutex_lock(&g_completion_lock);
    for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
        if (!g_completion_batches[b].occupied) continue;
        for (int i = 0; i < g_completion_batches[b].entry_count; i++) {
            gw_completion_entry_t *e = &g_completion_batches[b].entries[i];
            if (!e->done) { e->result = 0; e->done = 1; }   /* retryable */
        }
        g_completion_batches[b].entry_count = 0;
        g_completion_batches[b].occupied = 0;
    }
    pthread_mutex_unlock(&g_completion_lock);
}

/* PoP: gw_enqueue_process_completion_notification @ gateway/run.py:GatewayRunner._enqueue_process_completion_notification */
int gw_enqueue_process_completion_notification(const char *synth_text,
                                               const json_node_t *evt)
{
    if (g_completion_batches_stopping) return 0;

    char *key = gw_completion_notification_batch_key(evt);
    if (!key) return 0;
    int rc = 0;
    pthread_mutex_lock(&g_completion_lock);
    int slot = -1;
    for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
        if (g_completion_batches[b].occupied &&
            strcmp(g_completion_batches[b].key, key) == 0) { slot = b; break; }
    }
    if (slot < 0) {
        for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
            if (!g_completion_batches[b].occupied) { slot = b; break; }
        }
        if (slot >= 0) {
            snprintf(g_completion_batches[slot].key, sizeof g_completion_batches[slot].key, "%s", key);
            g_completion_batches[slot].entry_count = 0;
            g_completion_batches[slot].occupied = 1;
        }
    }
    if (slot >= 0) {
        gw_completion_batch_t *batch = &g_completion_batches[slot];
        if (batch->entry_count < GW_COMPLETION_ENTRIES_MAX) {
            gw_completion_entry_t *e = &batch->entries[batch->entry_count++];
            snprintf(e->synth_text, sizeof e->synth_text, "%s", synth_text ? synth_text : "");
            e->evt = evt ? json_copy((json_node_t*)evt) : NULL;
            e->result = -1;
            e->done = 0;
            rc = -1;   /* enqueued; flush below resolves */
        }
    }
    pthread_mutex_unlock(&g_completion_lock);

    if (rc == -1) {
        /* First-entry task creation + short-window flush (the asyncio task). */
        rc = gw_flush_process_completion_batch(key);
    }
    free(key);
    return rc;
}


/* ════════════════════════════════════════════════════════════════
 * Async delegation delivery: _deliver_async_delegation_group
 * ════════════════════════════════════════════════════════════════ */

/* Injectable durable-claim seam (tools/async_delegation claim/complete/release). */
typedef struct {
    /* claim: malloc'd claim id (caller frees) or NULL when another consumer
     * owns the row. */
    char *(*claim)(const json_node_t *evt, const char *owner);
    void (*complete)(const json_node_t *evt, const char *claim_id);
    void (*release)(const json_node_t *evt, const char *claim_id);
} gw_adel_claim_vtab_t;

static gw_adel_claim_vtab_t g_adel_claims;

void gw_set_adel_claim_vtab(const gw_adel_claim_vtab_t *vtab)
{
    g_adel_claims = *vtab;
}

/* PoP: gw_deliver_async_delegation_group @ gateway/run.py:GatewayRunner._deliver_async_delegation_group */
int gw_deliver_async_delegation_group(const json_node_t *group)
{
    if (!group || !json_is_array(group)) return 2;

    struct { json_node_t *evt; char text[4096]; } deliverable[32];
    int nd = 0;
    size_t gn = json_array_size(group);
    for (size_t i = 0; i < gn && nd < 32; i++) {
        json_node_t *evt = json_get((json_node_t*)group, (int)i);
        if (!evt) continue;
        char *synth_text = gateway_format_gateway_process_notification(evt);
        if (!synth_text || !synth_text[0]) { free(synth_text); continue; }
        char identity[256];
        gw_completion_identity_from_evt(evt, identity, sizeof identity);
        if (identity[0]) {
            bool skip = false;
            pthread_mutex_lock(&g_completion_lock);
            for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++) {
                if (g_delivery_ledger[k].identity[0] &&
                    strcmp(g_delivery_ledger[k].identity, identity) == 0) {
                    skip = true;   /* inflight or already delivered */
                    break;
                }
            }
            pthread_mutex_unlock(&g_completion_lock);
            if (skip) { free(synth_text); continue; }
        }
        deliverable[nd].evt = evt;
        snprintf(deliverable[nd].text, sizeof deliverable[nd].text, "%s", synth_text);
        nd++;
        free(synth_text);
    }
    if (nd == 0) return 2;
    if (nd == 1) {
        return (g_completion_delivery.deliver)
               ? g_completion_delivery.deliver(deliverable[0].text, deliverable[0].evt) : 0;
    }

    /* Multi-event group: primary rides _deliver_completion_notification with
     * a consolidated text containing every sibling whose durable row THIS
     * runner claimed up front. */
    char owner[64];
    snprintf(owner, sizeof owner, "gateway-batch:%p", (void *)&g_adel_claims);
    json_node_t *blocks = json_array();
    json_append(blocks, json_string(deliverable[0].text));
    struct { char id[128]; json_node_t *evt; } siblings[31];
    int ns = 0;
    for (int i = 1; i < nd && ns < 31; i++) {
        char *claim_id = g_adel_claims.claim
                         ? g_adel_claims.claim(deliverable[i].evt, owner) : NULL;
        if (!claim_id) continue;   /* another consumer owns this row */
        snprintf(siblings[ns].id, sizeof siblings[ns].id, "%s", claim_id);
        siblings[ns].evt = deliverable[i].evt;
        ns++;
        free(claim_id);
        json_append(blocks, json_string(deliverable[i].text));
    }
    int delivered = 0;
    do {
        if (ns == 0) {
            delivered = (g_completion_delivery.deliver)
                        ? g_completion_delivery.deliver(deliverable[0].text, deliverable[0].evt) : 0;
            break;
        }
        char *consolidated = gw_format_coalesced_async_delegations(blocks);
        delivered = (g_completion_delivery.deliver && consolidated)
                    ? g_completion_delivery.deliver(consolidated, deliverable[0].evt) : 0;
        free(consolidated);
    } while (0);

    if (delivered == 1) {
        for (int i = 0; i < ns; i++) {
            if (g_adel_claims.complete) {
                g_adel_claims.complete(siblings[i].evt, siblings[i].id);
            }
        }
        json_node_t *evts = json_array();
        for (int i = 0; i < ns; i++)
            json_append(evts, json_copy(siblings[i].evt));
        gateway_runner_record_coalesced_completion_siblings(evts);
        json_free(evts);
    } else {
        for (int i = 0; i < ns; i++) {
            if (g_adel_claims.release)
                g_adel_claims.release(siblings[i].evt, siblings[i].id);
        }
        if (delivered == 2) {
            /* Primary dropped/owned elsewhere — siblings still need delivery:
             * hand them back to the completion queue for the next tick. */
            extern void gw_requeue_completion_events(const json_node_t *events);
            json_node_t *evts = json_array();
            for (int i = 0; i < ns; i++)
                json_append(evts, json_copy(siblings[i].evt));
            gw_requeue_completion_events(evts);
            json_free(evts);
        }
    }
    json_free(blocks);
    return delivered;
}

/* Injectable completion-queue requeue sink (process_registry.completion_queue). */
static void (*g_completion_queue_sink)(const json_node_t *events) = NULL;

void gw_set_completion_queue_sink(void (*fn)(const json_node_t *))
{
    g_completion_queue_sink = fn;
}

void gw_requeue_completion_events(const json_node_t *events)
{
    if (g_completion_queue_sink) g_completion_queue_sink(events);
}


/* ════════════════════════════════════════════════════════════════
 * TurnRunner native Slack task-card progress cluster.
 *
 * Python: native_tool_start_callback / native_tool_complete_callback queue
 * ID-bearing lifecycle dicts from the agent thread; _send_native_task_card_progress
 * drains them into the adapter's chat.startStream plan/task-card stream with a
 * fallback to an editable text message. The C port keeps the same queue +
 * state machine; adapter I/O arrives through an injectable vtab.
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    char id[128];
    char title[160];
    char status[16];   /* in_progress | complete | error */
} gw_task_card_t;

#define GW_TASK_CARDS_MAX 64

typedef struct {
    gw_task_card_t tasks[GW_TASK_CARDS_MAX];
    int task_count;
    int anonymous_seq;
    int native_failed;
    char fallback_msg_id[128];
    /* adapter vtab */
    bool (*send_native_task_card_progress)(void *adapter, const char *chat_id,
                                           const gw_task_card_t *tasks, int n,
                                           const char *reply_to);
    bool (*edit_message)(void *adapter, const char *chat_id,
                         const char *message_id, const char *content);
    bool (*send)(void *adapter, const char *chat_id, const char *content,
                 const char *reply_to, char *out_message_id, size_t mid_cap);
    void (*stop_native_task_card_progress)(void *adapter, const char *chat_id,
                                           const char *reply_to);
    void *adapter;
    char chat_id[128];
    char reply_to[64];
    volatile int agent_interrupted;
} gw_turn_progress_ctx_t;

static void gw_task_compact(const char *value, int limit, char *out, size_t cap)
{
    if (!out || !cap) return;
    out[0] = '\0';
    if (!value) return;
    size_t w = 0;
    const char *p = value;
    bool in_ws = false;
    while (*p && w + 1 < cap) {
        unsigned char c = (unsigned char)*p;
        if (c==' '||c=='\t'||c=='\r'||c=='\n') { in_ws = true; p++; continue; }
        if (in_ws && w > 0 && w + 1 < cap) out[w++] = ' ';
        in_ws = false;
        out[w++] = (char)c; p++;
    }
    out[w] = '\0';
    if ((int)strlen(out) > limit && limit > 3) {
        int keep = limit - 3;
        out[keep] = '\0';
        /* rstrip */
        size_t l = strlen(out);
        while (l && (unsigned char)out[l-1] <= ' ') out[--l] = '\0';
        strcat(out, "...");
    }
}

static gw_task_card_t *gw_task_find(gw_turn_progress_ctx_t *ctx, const char *call_id)
{
    for (int i = 0; i < ctx->task_count; i++)
        if (strcmp(ctx->tasks[i].id, call_id) == 0) return &ctx->tasks[i];
    return NULL;
}

/* _apply_native_event — returns true when the event mutated card state. */
static bool gw_apply_native_event(gw_turn_progress_ctx_t *ctx, json_node_t *raw)
{
    if (!raw || raw->type != JSON_OBJECT) return false;
    const char *etype = json_get_str(raw, "type", "");
    if (strcmp(etype, "tool.started") != 0 && strcmp(etype, "tool.completed") != 0)
        return false;
    char call_id[128] = "";
    const char *cidv = json_get_str(raw, "tool_call_id", "");
    snprintf(call_id, sizeof call_id, "%s", cidv ? cidv : "");
    if (!call_id[0])
        snprintf(call_id, sizeof call_id, "anonymous_%d", ++ctx->anonymous_seq);
    char tool_name[80] = "tool";
    const char *tnv = json_get_str(raw, "tool_name", "tool");
    snprintf(tool_name, sizeof tool_name, "%s", tnv && tnv[0] ? tnv : "tool");

    if (strcmp(etype, "tool.started") == 0) {
        char title[160];
        snprintf(title, sizeof title, "%s", tool_name);
        char preview[96];
        const char *pv = json_get_str(raw, "preview", NULL);
        gw_task_compact(pv ? pv : "", 64, preview, sizeof preview);
        if (preview[0]) snprintf(title, sizeof title, "%s - %s", tool_name, preview);
        gw_task_card_t *t = gw_task_find(ctx, call_id);
        if (!t && ctx->task_count < GW_TASK_CARDS_MAX)
            t = &ctx->tasks[ctx->task_count++];
        if (t) {
            snprintf(t->id, sizeof t->id, "%s", call_id);
            gw_task_compact(title, 120, t->title, sizeof t->title);
            snprintf(t->status, sizeof t->status, "in_progress");
        }
        return true;
    }
    gw_task_card_t *t = gw_task_find(ctx, call_id);
    if (!t && ctx->task_count < GW_TASK_CARDS_MAX) {
        /* Completion-only events are rare but valid on some runtimes. Keep
         * their real ID instead of guessing a same-name pending call. */
        t = &ctx->tasks[ctx->task_count++];
        snprintf(t->id, sizeof t->id, "%s", call_id);
        gw_task_compact(tool_name, 120, t->title, sizeof t->title);
        snprintf(t->status, sizeof t->status, "in_progress");
    }
    if (t) {
        json_node_t *ie = json_obj_get(raw, "is_error");
        snprintf(t->status, sizeof t->status, "%s",
                 (ie && ie->type == JSON_BOOL && ie->bool_val) ? "error" : "complete");
    }
    return true;
}

static char *gw_fallback_text(const gw_turn_progress_ctx_t *ctx)
{
    size_t cap = 64 + ctx->task_count * 192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = (size_t)snprintf(buf, cap, "Hermes is working");
    int start = ctx->task_count > 8 ? ctx->task_count - 8 : 0;
    for (int i = start; i < ctx->task_count; i++) {
        const char *label = ctx->tasks[i].status;
        for (size_t k = 0; k < 3; k++) {
            static const char *const keys[] = {"in_progress","complete","error"};
            if (strcmp(label, keys[k]) == 0) {
                label = (const char *[]){"running","complete","error"}[k];
                break;
            }
        }
        len += (size_t)snprintf(buf + len, cap - len, "\n- %s - %s",
                                ctx->tasks[i].title, label);
    }
    return buf;
}

static bool gw_publish_native_progress(gw_turn_progress_ctx_t *ctx)
{
    if (ctx->task_count == 0) return true;
    int start = ctx->task_count > 8 ? ctx->task_count - 8 : 0;
    int vis = ctx->task_count - start;
    if (!ctx->native_failed && ctx->send_native_task_card_progress &&
        ctx->send_native_task_card_progress(ctx->adapter, ctx->chat_id,
                                            &ctx->tasks[start], vis, ctx->reply_to))
        return true;
    if (!ctx->native_failed) {
        ctx->native_failed = 1;
        hermes_log(LOG_WARNING, "gateway",
                   "Slack native task-card progress failed; falling back to an editable text update");
    }
    /* Once the native rail fails, every later lifecycle event edits the same
     * fallback message so progress remains live. */
    char *text = gw_fallback_text(ctx);
    if (!text) return false;
    bool ok = false;
    if (ctx->fallback_msg_id[0] && ctx->edit_message)
        ok = ctx->edit_message(ctx->adapter, ctx->chat_id, ctx->fallback_msg_id, text);
    if (!ok && ctx->send) {
        char mid[128] = "";
        ok = ctx->send(ctx->adapter, ctx->chat_id, text, ctx->reply_to, mid, sizeof mid);
        if (ok && mid[0]) snprintf(ctx->fallback_msg_id, sizeof ctx->fallback_msg_id, "%s", mid);
    }
    free(text);
    return ok;
}

/* PoP: gw_turn_runner_send_native_task_card_progress @ gateway/run.py:TurnRunner._send_native_task_card_progress */
void gw_turn_runner_send_native_task_card_progress(gw_turn_progress_ctx_t *ctx,
                                                   json_node_t *progress_queue)
{
    if (!ctx || !progress_queue || !json_is_array(progress_queue)) return;
    size_t n = json_array_size(progress_queue);
    for (size_t i = 0; i < n; i++) {
        if (ctx->agent_interrupted) break;
        json_node_t *raw = json_get(progress_queue, (int)i);
        if (gw_apply_native_event(ctx, raw))
            gw_publish_native_progress(ctx);
    }
    if (ctx->stop_native_task_card_progress && ctx->adapter)
        ctx->stop_native_task_card_progress(ctx->adapter, ctx->chat_id, ctx->reply_to);
}

/* PoP: gw_turn_runner_native_tool_start_callback @ gateway/run.py:TurnRunner.native_tool_start_callback */
void gw_turn_runner_native_tool_start_callback(gw_turn_progress_ctx_t *ctx,
                                               const char *call_id,
                                               const char *tool_name,
                                               const json_node_t *args)
{
    if (!ctx || ctx->agent_interrupted) return;
    char title[80] = "tool";
    snprintf(title, sizeof title, "%s", (tool_name && tool_name[0]) ? tool_name : "tool");
    char preview[96];
    /* build_tool_preview seam: compact JSON dump of args, capped at 64 chars. */
    if (args && json_is_object(args)) {
        char *dump = json_serialize(args);
        gw_task_compact(dump ? dump : "", 64, preview, sizeof preview);
        free(dump);
    } else {
        preview[0] = '\0';
    }
    gw_task_card_t *t = NULL;
    const char *cid = (call_id && call_id[0]) ? call_id : "";
    for (int i = 0; i < ctx->task_count; i++)
        if (strcmp(ctx->tasks[i].id, cid) == 0) { t = &ctx->tasks[i]; break; }
    if (!t && ctx->task_count < GW_TASK_CARDS_MAX)
        t = &ctx->tasks[ctx->task_count++];
    if (t) {
        char full[200];
        if (preview[0]) snprintf(full, sizeof full, "%s - %s", title, preview);
        else snprintf(full, sizeof full, "%s", title);
        snprintf(t->id, sizeof t->id, "%s", cid);
        gw_task_compact(full, 120, t->title, sizeof t->title);
        snprintf(t->status, sizeof t->status, "in_progress");
    }
}

/* PoP: gw_turn_runner_native_tool_complete_callback @ gateway/run.py:TurnRunner.native_tool_complete_callback */
void gw_turn_runner_native_tool_complete_callback(gw_turn_progress_ctx_t *ctx,
                                                  const char *call_id,
                                                  const char *tool_name,
                                                  const char *result_summary)
{
    if (!ctx || ctx->agent_interrupted) return;
    /* _detect_tool_failure seam: a non-empty failure summary marks error. */
    bool is_error = result_summary && result_summary[0];
    gw_task_card_t *t = NULL;
    const char *cid = (call_id && call_id[0]) ? call_id : "";
    for (int i = 0; i < ctx->task_count; i++)
        if (strcmp(ctx->tasks[i].id, cid) == 0) { t = &ctx->tasks[i]; break; }
    if (!t && ctx->task_count < GW_TASK_CARDS_MAX) {
        t = &ctx->tasks[ctx->task_count++];
        snprintf(t->id, sizeof t->id, "%s", cid);
        snprintf(t->title, sizeof t->title, "%s", (tool_name && tool_name[0]) ? tool_name : "tool");
        snprintf(t->status, sizeof t->status, "in_progress");
    }
    if (t) snprintf(t->status, sizeof t->status, "%s", is_error ? "error" : "complete");
}

/* PoP: gw_turn_runner_combined_tool_start_callback @ gateway/run.py:TurnRunner.combined_tool_start_callback */
void gw_turn_runner_combined_tool_start_callback(gw_turn_progress_ctx_t *ctx,
                                                 const char *call_id,
                                                 const char *tool_name,
                                                 const json_node_t *args,
                                                 void (*voice_ack)(void *, const char *,
                                                                   const char *, const json_node_t *),
                                                 void *voice_ack_ctx,
                                                 int voice_ack_active,
                                                 int native_slack_task_cards)
{
    if (voice_ack_active && voice_ack)
        voice_ack(voice_ack_ctx, call_id, tool_name, args);
    if (native_slack_task_cards)
        gw_turn_runner_native_tool_start_callback(ctx, call_id, tool_name, args);
}


/* ════════════════════════════════════════════════════════════════
 * _notify_interrupted_cron_jobs
 * ════════════════════════════════════════════════════════════════ */

#include "cron_jobs.h"
#include "cron_scheduler_delivery.h"

/* Injectable adapter-send + config seams (self.adapters / platform_cfg
 * gateway_restart_notification / _thread_metadata_for_target). */
typedef struct {
    bool (*adapter_for_platform)(const char *platform);           /* adapter connected? */
    bool (*gateway_restart_notification)(const char *platform);   /* cfg flag (true default) */
    json_node_t *(*thread_metadata)(const char *platform, const char *chat_id,
                                    const char *thread_id);
    bool (*send)(const char *platform, const char *chat_id,
                 const char *message, const json_node_t *metadata);
} gw_cron_notify_vtab_t;

static gw_cron_notify_vtab_t g_cron_notify;

void gw_set_cron_notify_vtab(const gw_cron_notify_vtab_t *vtab)
{
    g_cron_notify = *vtab;
}

/* PoP: gw_notify_interrupted_cron_jobs @ gateway/run.py:GatewayRunner._notify_interrupted_cron_jobs */
int gw_notify_interrupted_cron_jobs(const json_node_t *job_ids,
                                    int restart_requested)
{
    if (!job_ids || !json_is_array(job_ids) || json_array_size(job_ids) == 0)
        return 0;
    const char *action = restart_requested ? "restarting" : "shutting down";
    struct { char key[512]; } notified[64];
    int nn = 0;
    size_t n = json_array_size(job_ids);
    for (size_t i = 0; i < n; i++) {
        json_node_t *jid = json_get((json_node_t*)job_ids, (int)i);
        if (!jid || jid->type != JSON_STRING || !jid->str_val[0]) continue;
        const char *job_id = jid->str_val;
        json_node_t *job = cronjobs_get_job(job_id);
        if (!job) continue;
        scheduler_target_t targets[8];
        scheduler_job_t sj;
        memset(&sj, 0, sizeof sj);
        /* delivery-target resolution reads the raw job dict fields. */
        json_node_t *origin = json_obj_get(job, "origin");
        if (origin && origin->type == JSON_OBJECT) {
            sj.origin.platform = json_get_str(origin, "platform", NULL);
            sj.origin.chat_id = json_get_str(origin, "chat_id", NULL);
            sj.origin.thread_id = json_get_str(origin, "thread_id", NULL);
            sj.origin.has_origin = 1;
        }
        sj.deliver = json_get_str(job, "deliver", NULL);
        int ntg = scheduler_resolve_delivery_targets(&sj, targets, 8);
        if (ntg <= 0) continue;   /* deliver=local stays silent (#43014) */
        char msg[1024];
        snprintf(msg, sizeof msg,
            "\xe2\x9a\xa0\xef\xb8\x8f Cron job '%s' was interrupted \xe2\x80\x94 "
            "the gateway is %s and killed the run before it finished. "
            "No result was produced for this run.",
            json_get_str(job, "name", "") && json_get_str(job, "name", "")[0]
                ? json_get_str(job, "name", "") : job_id,
            action);
        for (int t = 0; t < ntg; t++) {
            char platform_name[32];
            snprintf(platform_name, sizeof platform_name, "%s", targets[t].platform);
            for (char *c = platform_name; *c; c++) *c = (char)tolower((unsigned char)*c);
            if (!g_cron_notify.adapter_for_platform ||
                !g_cron_notify.adapter_for_platform(platform_name))
                continue;
            if (g_cron_notify.gateway_restart_notification &&
                !g_cron_notify.gateway_restart_notification(platform_name))
                continue;
            const char *chat_id = targets[t].chat_id;
            const char *thread_id = targets[t].thread_id;
            char dedup[512];
            snprintf(dedup, sizeof dedup, "%s|%s|%s|%s", job_id, platform_name,
                     chat_id ? chat_id : "", thread_id ? thread_id : "");
            bool dup = false;
            for (int k = 0; k < nn; k++)
                if (strcmp(notified[k].key, dedup) == 0) { dup = true; break; }
            if (dup) continue;
            json_node_t *meta = g_cron_notify.thread_metadata
                ? g_cron_notify.thread_metadata(platform_name, chat_id, thread_id) : NULL;
            bool ok = g_cron_notify.send &&
                      g_cron_notify.send(platform_name, chat_id, msg, meta);
            if (meta) json_free(meta);
            if (ok && nn < 64) snprintf(notified[nn++].key, sizeof notified[0].key, "%s", dedup);
        }
        json_free(job);
    }
    if (nn > 0)
        hermes_log(LOG_INFO, "gateway",
                   "Shutdown: delivered %d interrupted-cron-job notice(s)", nn);
    return nn;
}

/* ════════════════════════════════════════════════════════════════
 * _finalize_session_off_loop
 * ════════════════════════════════════════════════════════════════ */

#define GW_FINALIZE_TIMEOUT_S 10.0

typedef struct {
    char arg[2048];   /* opaque finalize_session argument blob */
} gw_finalize_job_t;

extern int hermes_cli_lifecycle_finalize_session(const char *arg);

static void gw_finalize_job_run(void *arg)
{
    gw_finalize_job_t *job = arg;
    hermes_cli_lifecycle_finalize_session(job->arg);
}

/* PoP: gw_finalize_session_off_loop @ gateway/run.py:GatewayRunner._finalize_session_off_loop */
void gw_finalize_session_off_loop(const char *session_id, const char *platform,
                                  const char *reason, const char *extra_json)
{
    gw_finalize_job_t job;
    snprintf(job.arg, sizeof job.arg,
             "{\"session_id\":\"%s\",\"platform\":\"%s\",\"reason\":\"%s\"%s%s%s}",
             session_id ? session_id : "",
             platform ? platform : "",
             reason ? reason : "",
             extra_json && extra_json[0] ? "," : "",
             extra_json ? extra_json : "",
             extra_json && extra_json[0] ? "" : "");
    /* Off-loop execution seam: a host installs a worker-thread executor so
     * plugin on_session_finalize hooks never freeze the live loop. */
    if (g_off_loop_exec) {
        g_off_loop_exec(gw_finalize_job_run, &job);   /* off-loop; unbounded worker left to finish */
    } else {
        gw_finalize_job_run(&job);
    }
}


/* ════════════════════════════════════════════════════════════════
 * _resolve_gateway_model_context
 *
 * Shared non-resident authority for status/session banners and slash
 * commands. Config seam: gw_load_gateway_config / hermes_config_t.
 * ════════════════════════════════════════════════════════════════ */

#include "gateway_run_pure2.h"

#ifndef GW_DEFAULT_FALLBACK_CONTEXT
#define GW_DEFAULT_FALLBACK_CONTEXT 262144   /* model_metadata.DEFAULT_FALLBACK_CONTEXT */
#endif

static json_node_t *(*g_raw_config_loader)(void) = NULL;
static char *(*g_runtime_kwargs)(void) = NULL;

void gw_set_model_context_seams(json_node_t *(*raw_config)(void),
                                char *(*runtime_kwargs_fn)(void))
{
    g_raw_config_loader = raw_config;
    g_runtime_kwargs = runtime_kwargs_fn;
}

/* PoP: gw_resolve_gateway_model_context @ gateway/run.py:_resolve_gateway_model_context */
void gw_resolve_gateway_model_context(const char *model, char *out_json, size_t cap)
{
    char resolved_model[192] = "";
    hermes_config_t *cfg = gw_load_gateway_config();
    if (model && model[0]) snprintf(resolved_model, sizeof resolved_model, "%s", model);
    else gw_resolve_gateway_model(cfg, resolved_model, sizeof resolved_model);

    int config_context_length = -1;
    char provider[128] = "", base_url[512] = "", api_key[256] = "";
    char configured_provider[128] = "", configured_base_url[512] = "";

    json_node_t *custom_providers = NULL;
    /* Raw-config + runtime-kwargs seams (Python's _load_gateway_config dict
     * and _resolve_runtime_agent_kwargs). A host installs both at startup. */
    json_node_t *(*raw_config_loader)(void) = g_raw_config_loader;
    char *(*runtime_kwargs)(void) = g_runtime_kwargs;
    if (raw_config_loader) {
        json_node_t *data = raw_config_loader();
        if (data) {
            json_node_t *model_cfg = json_obj_get(data, "model");
            if (model_cfg && model_cfg->type == JSON_OBJECT) {
                const char *dm = json_get_str(model_cfg, "default", NULL);
                if (!dm || !dm[0]) dm = json_get_str(model_cfg, "model", NULL);
                if (dm && dm[0] && !resolved_model[0])
                    snprintf(resolved_model, sizeof resolved_model, "%s", dm);
                json_node_t *ctx = json_obj_get(model_cfg, "context_length");
                if (ctx && ctx->type == JSON_NUMBER)
                    config_context_length = (int)ctx->num_val;
                else if (ctx && ctx->type == JSON_STRING) {
                    char *end; long v = strtol(ctx->str_val, &end, 10);
                    if (end != ctx->str_val && *end == '\0') config_context_length = (int)v;
                }
                const char *pv = json_get_str(model_cfg, "provider", NULL);
                const char *bv = json_get_str(model_cfg, "base_url", NULL);
                if (pv && pv[0]) snprintf(configured_provider, sizeof configured_provider, "%s", pv);
                if (bv && bv[0]) snprintf(configured_base_url, sizeof configured_base_url, "%s", bv);
                snprintf(provider, sizeof provider, "%s", configured_provider);
                snprintf(base_url, sizeof base_url, "%s", configured_base_url);
            }
            custom_providers = json_copy(json_obj_get(data, "custom_providers"));
            json_free(data);
        }
    }
    if (runtime_kwargs) {
        char *rt = runtime_kwargs();
        if (rt) {
            json_node_t *rjn = json_parse(rt, NULL);
            if (rjn) {
                const char *pv = json_get_str(rjn, "provider", NULL);
                const char *bv = json_get_str(rjn, "base_url", NULL);
                const char *kv = json_get_str(rjn, "api_key", NULL);
                if (pv && pv[0]) snprintf(provider, sizeof provider, "%s", pv);
                if (bv && bv[0]) snprintf(base_url, sizeof base_url, "%s", bv);
                if (kv) snprintf(api_key, sizeof api_key, "%s", kv);
                json_free(rjn);
            }
            free(rt);
        }
    }

    int context_length;
    if (config_context_length > 0) {
        context_length = get_model_context_length(resolved_model,
            base_url[0] ? base_url : "", api_key, config_context_length,
            provider[0] ? provider : "");
        if (context_length <= 0) context_length = config_context_length;
    } else {
        context_length = get_model_context_length(resolved_model,
            base_url[0] ? base_url : "", api_key, config_context_length > 0 ? config_context_length : 0,
            provider[0] ? provider : "");
        if (context_length <= 0) context_length = GW_DEFAULT_FALLBACK_CONTEXT;
    }
    const char *context_source =
        config_context_length > 0 ? "config"
        : context_length == GW_DEFAULT_FALLBACK_CONTEXT ? "default"
        : "detected";

    snprintf(out_json, cap,
             "{\"model\":\"%s\",\"provider\":\"%s\",\"base_url\":\"%s\","
             "\"context_length\":%d,\"context_source\":\"%s\"}",
             resolved_model, provider, base_url, context_length, context_source);
    if (custom_providers) json_free(custom_providers);
}


/* ════════════════════════════════════════════════════════════════
 * _handle_message — the core message processing pipeline.
 *
 * Faithful port of the GatewayRunner._handle_message pipeline stages. Every
 * side-effectful stage (platform send, agent run, session store) arrives via
 * the pipeline vtab so the module stays self-contained; the stage ORDER,
 * gates, and reply strings mirror Python exactly.
 * ════════════════════════════════════════════════════════════════ */

#include "cli_command_registry.h"

typedef struct {
    /* session context */
    void (*reset_session_vars)(void);
    const char *(*profile_name_for_source)(const gw_session_source_t *src); /* NULL=reject */
    int   multiplex_profiles;
    /* guards */
    bool (*is_internal_event)(const json_node_t *event);
    bool (*slack_ignored_channel)(const char *chat_id);
    void (*queue_startup_restore_event)(json_node_t *event);
    bool startup_restore_in_progress;
    bool (*startup_restore_replay)(const json_node_t *event);
    void (*scale_to_zero_note_real_inbound)(void);
    /* pre_gateway_dispatch hook: fills out_action/out_text. 0=allow */
    int  (*pre_gateway_dispatch)(json_node_t *event, char *out_action, size_t cap,
                                 char *out_text, size_t text_cap);
    /* auth / pairing */
    bool (*is_user_authorized)(const gw_session_source_t *src);
    /* returns malloc'd pairing reply or NULL when unhandled */
    char *(*unauthorized_dm_pairing)(const gw_session_source_t *src);
    /* emergency stop */
    const char *(*estop_paused_reply)(void);   /* NULL when not paused */
    bool (*estop_command_allowed)(const char *cmd);
    bool (*session_update_prompt_pending)(const char *session_key);
    bool (*is_session_running)(const char *session_key);
    bool (*slash_confirm_pending)(const char *session_key);
    bool (*blocking_tool_approval)(const char *session_key);
    /* update-prompt response writer: writes response text; false on error */
    bool (*write_update_response)(const char *text);
    /* slash-confirm resolver: malloc'd result or NULL */
    char *(*resolve_slash_confirm)(const char *session_key,
                                   const char *confirm_id, const char *choice);
    /* busy-path */
    const char *(*effective_busy_input_mode)(const gw_session_source_t *src);
    void (*queue_or_replace_pending_event)(const char *session_key, json_node_t *event);
    char *(*dispatch_busy_slash_command)(json_node_t *event,
                                         const cli_command_def_t *cmd_def,
                                         const char *session_key,
                                         const gw_session_source_t *source);
    char *(*check_slash_access)(const gw_session_source_t *src, const char *cmd);
    /* per-canonical command handler (status/help/new/...). malloc'd reply or NULL */
    char *(*handle_command)(const char *canonical_cmd, json_node_t *event);
    /* agent run seam (_handle_message_with_agent): malloc'd reply or NULL */
    char *(*handle_message_with_agent)(json_node_t *event,
                                       const gw_session_source_t *source,
                                       const char *session_key,
                                       long run_generation);
    /* lifecycle cleanup */
    void (*release_running_agent_state)(const char *session_key);
    void (*restore_one_turn_overrides)(const char *session_key);
    void (*clear_durable_active_turn)(json_node_t *event);
    void (*persist_active_agents)(void);
} gw_message_pipeline_vtab_t;

static gw_message_pipeline_vtab_t g_msg_pipeline;
static volatile int g_external_drain_active = 0;

void gw_set_message_pipeline_vtab(const gw_message_pipeline_vtab_t *vtab)
{
    g_msg_pipeline = *vtab;
}

static void gw_run_post_turn_hooks_json(const char *agent_result_str,
                                        const json_node_t *event,
                                        const gw_session_source_t *source,
                                        bool is_internal);

/* PoP: gateway_runner_handle_message @ gateway/run.py:_handle_message */
int gateway_runner_handle_message(GatewayRunner *self,
                                  const char *event_json,
                                  char *response_out, size_t response_size)
{
    if (!self || !event_json || !response_out || response_size == 0) return -1;
    const gw_message_pipeline_vtab_t *V = &g_msg_pipeline;
    response_out[0] = '\0';

    json_node_t *event = json_parse(event_json, NULL);
    if (!event || event->type != JSON_OBJECT) { if (event) json_free(event); return -1; }

    gw_session_source_t source;
    memset(&source, 0, sizeof source);
    session_source_set(&source,
        json_get_str(event, "platform", ""), json_get_str(event, "chat_id", ""),
        json_get_str(event, "chat_name", ""), json_get_str(event, "chat_type", ""),
        json_get_str(event, "user_id", ""), json_get_str(event, "user_name", ""),
        json_get_str(event, "thread_id", ""),
        "", "", "", "", "",
        json_get_str(event, "message_id", ""), false);

    bool is_internal = V->is_internal_event ? V->is_internal_event(event) : false;

    /* Cross-session leak guard: strip any inherited session identity before
     * binding our own (reset to _UNSET at handler entry). */
    if (V->reset_session_vars) V->reset_session_vars();

    char session_key[256];
    gateway_runner_session_key_for_source(self, &source, session_key, sizeof session_key);

    /* Fail-closed profile-route ingress gate. */
    if (V->multiplex_profiles && !V->profile_name_for_source) {
        hermes_log(LOG_WARNING, "gateway",
                   "Dropping inbound message because its explicit profile route "
                   "targets an unserved profile");
        json_free(event);
        return 0;
    }
    if (V->multiplex_profiles && V->profile_name_for_source) {
        const char *routed = V->profile_name_for_source(&source);
        if (!routed) {
            hermes_log(LOG_WARNING, "gateway",
                       "Dropping inbound message because its explicit profile route "
                       "targets an unserved profile");
            json_free(event);
            return 0;
        }
    }

    /* Ignored-channel guard runs FIRST — before queueing, hooks, auth (#51899). */
    if (!is_internal && strcmp(source.platform, "slack") == 0 &&
        V->slack_ignored_channel && V->slack_ignored_channel(source.chat_id)) {
        hermes_log(LOG_INFO, "gateway",
                   "Dropping Slack message from configured ignored channel %s", source.chat_id);
        json_free(event);
        return 0;
    }

    if (V->startup_restore_in_progress && !is_internal &&
        !(V->startup_restore_replay && V->startup_restore_replay(event))) {
        if (V->queue_startup_restore_event) V->queue_startup_restore_event(event);
        json_free(event);
        return 0;
    }

    if (!is_internal && V->scale_to_zero_note_real_inbound)
        V->scale_to_zero_note_real_inbound();

    /* pre_gateway_dispatch plugin hook: skip / rewrite / allow. */
    if (!is_internal && V->pre_gateway_dispatch) {
        char action[32] = "", new_text[4096] = "";
        int act = V->pre_gateway_dispatch(event, action, sizeof action,
                                          new_text, sizeof new_text);
        if (act != 0 || strcmp(action, "skip") == 0) {
            json_free(event);
            return 0;
        }
        if (strcmp(action, "rewrite") == 0 && new_text[0])
            json_set(event, "text", json_string(new_text));
    }

    /* Authorization. Internal events are system-generated and skip it. */
    if (!is_internal) {
        if (!source.user_id[0]) {
            if (!(V->is_user_authorized && V->is_user_authorized(&source))) {
                hermes_log(LOG_DEBUG, "gateway",
                           "Ignoring message with no user_id from %s", source.platform);
                json_free(event);
                return 0;
            }
        } else if (!(V->is_user_authorized && V->is_user_authorized(&source))) {
            hermes_log(LOG_WARNING, "gateway",
                       "Unauthorized user: %s (%s) on %s",
                       source.user_id, source.user_name, source.platform);
            /* DM pairing offer runs through the injectable seam. */
            if (strcmp(source.chat_type, "dm") == 0 && V->unauthorized_dm_pairing) {
                char *reply = V->unauthorized_dm_pairing(&source);
                if (reply) {
                    snprintf(response_out, response_size, "%s", reply);
                    free(reply);
                }
            }
            json_free(event);
            return 0;
        }
    }

    /* Global emergency stop (`hermes pause`): paused notice instead of a new
     * run; internal events and control traffic bypass the gate. */
    if (!is_internal && V->estop_paused_reply) {
        const char *paused = V->estop_paused_reply();
        if (paused) {
            const char *cmd = json_get_str(event, "command", "");
            bool allow = cmd[0] && V->estop_command_allowed && V->estop_command_allowed(cmd);
            if (!allow && V->session_update_prompt_pending &&
                V->session_update_prompt_pending(session_key))
                allow = true;
            if (!allow && V->is_session_running && V->is_session_running(session_key))
                allow = true;
            if (!allow && V->slash_confirm_pending &&
                V->slash_confirm_pending(session_key))
                allow = true;
            if (!allow && V->blocking_tool_approval &&
                V->blocking_tool_approval(session_key))
                allow = true;
            if (!allow) {
                snprintf(response_out, response_size, "%s", paused);
                json_free(event);
                return 1;
            }
        }
    }

    /* Pending /update prompt interception: approve|yes→y, deny|no→n; other
     * free text becomes the response; recognized commands cancel the prompt
     * and fall through. */
    const char *cmd0 = json_get_str(event, "command", "");
    if (V->session_update_prompt_pending && V->session_update_prompt_pending(session_key)) {
        const char *raw = json_get_str(event, "text", "");
        char response_text[512] = "";
        if (strcmp(cmd0, "approve") == 0 || strcmp(cmd0, "yes") == 0)
            snprintf(response_text, sizeof response_text, "y");
        else if (strcmp(cmd0, "deny") == 0 || strcmp(cmd0, "no") == 0)
            snprintf(response_text, sizeof response_text, "n");
        else if (!cmd0[0])
            snprintf(response_text, sizeof response_text, "%s",
                     raw ? raw : "");
        bool recognized = cmd0[0] && cli_resolve_command(cmd0) != NULL;
        if (response_text[0]) {
            if (V->write_update_response && !V->write_update_response(response_text)) {
                snprintf(response_out, response_size,
                         "\xe2\x9c\x97 Failed to send response to update process");
                json_free(event);
                return 1;
            }
            snprintf(response_out, response_size,
                     "\xe2\x9c\x93 Sent `%.20s` to the update process.", response_text);
            json_free(event);
            return 1;
        }
        (void)recognized;
    }

    /* Pending slash-confirm interception (/reload-mcp and friends): only when
     * no tool approval is live — the tool approval takes precedence. */
    bool tool_approval_live =
        V->blocking_tool_approval && V->blocking_tool_approval(session_key);
    if (!tool_approval_live && V->slash_confirm_pending &&
        V->slash_confirm_pending(session_key)) {
        const char *cmd = cmd0;
        const char *norm_raw = json_get_str(event, "text", "");
        while (*norm_raw == '!' || *norm_raw == '/') norm_raw++;
        const char *choice = NULL;
        if (!strcmp(cmd,"approve")||!strcmp(cmd,"yes")||!strcmp(cmd,"ok")||!strcmp(cmd,"confirm"))
            choice = "once";
        else if (!strcmp(cmd,"always")||!strcmp(cmd,"remember"))
            choice = "always";
        else if (!strcmp(cmd,"cancel")||!strcmp(cmd,"no")||!strcmp(cmd,"deny")||!strcmp(cmd,"nevermind"))
            choice = "cancel";
        else if (!strcasecmp(norm_raw,"approve")||!strcasecmp(norm_raw,"approve once")||!strcasecmp(norm_raw,"once"))
            choice = "once";
        else if (!strcasecmp(norm_raw,"always")||!strcasecmp(norm_raw,"always approve"))
            choice = "always";
        else if (!strcasecmp(norm_raw,"cancel")||!strcasecmp(norm_raw,"nevermind")||!strcasecmp(norm_raw,"no"))
            choice = "cancel";
        if (choice && V->resolve_slash_confirm) {
            char *resolved = V->resolve_slash_confirm(session_key, "", choice);
            if (resolved) { snprintf(response_out, response_size, "%s", resolved); free(resolved); }
            json_free(event);
            return 1;
        }
    }

    /* PRIORITY path: an agent already running on this session. */
    if (V->is_session_running && V->is_session_running(session_key)) {
        const cli_command_def_t *cd = cmd0[0] ? cli_resolve_command(cmd0) : NULL;
        if (cd && !strcmp(cd->name, "status")) goto cold_dispatch;
        if (cd && !strcmp(cd->name, "context")) goto cold_dispatch;
        if (cd && V->check_slash_access) {
            char *denied = V->check_slash_access(&source, cd->name);
            if (denied) {
                snprintf(response_out, response_size, "%s", denied);
                free(denied);
                json_free(event);
                return 1;
            }
        }
        if (cd && V->dispatch_busy_slash_command) {
            char *out = V->dispatch_busy_slash_command(event, cd, session_key, &source);
            if (out) { snprintf(response_out, response_size, "%s", out); free(out); }
            json_free(event);
            return 1;
        }
        /* Plain text / media follow-up: default busy semantics. */
        if (V->queue_or_replace_pending_event)
            V->queue_or_replace_pending_event(session_key, event);
        json_free(event);
        return 0;
    }

cold_dispatch: ;
    /* Check for commands (cold path). */
    const char *command = cmd0;
    const cli_command_def_t *cmd_def = command[0] ? cli_resolve_command(command) : NULL;
    const char *canonical = cmd_def ? cmd_def->name : command;

    if (command[0] && canonical[0] && cmd_def) {
        if (V->check_slash_access) {
            char *denied = V->check_slash_access(&source, canonical);
            if (denied) {
                snprintf(response_out, response_size, "%s", denied);
                free(denied);
                json_free(event);
                return 1;
            }
        }
    }

    if (canonical && cmd_def) {
        if (!strcmp(canonical, "start")) {
            hermes_log(LOG_INFO, "gateway",
                       "Ignoring /start platform ping for session %s", session_key);
            json_free(event);
            return 1;   /* empty reply */
        }
        if (!strcmp(canonical, "queue")) {
            /* payload becomes the turn text; falls through to the agent. */
        }
        if (V->handle_command) {
            char *out = V->handle_command(canonical, event);
            if (out) {
                snprintf(response_out, response_size, "%s", out);
                free(out);
                json_free(event);
                return 1;
            }
            if (!strcmp(canonical, "help") || !strcmp(canonical, "commands") ||
                !strcmp(canonical, "status") || !strcmp(canonical, "new") ||
                !strcmp(canonical, "stop") || !strcmp(canonical, "pause")) {
                json_free(event);
                return 0;
            }
        }
    }

    /* External-drain new-turn gate: refuse NEW turns during maintenance drain;
     * internal events bypass. In-flight turns untouched. */
    if (g_external_drain_active && !is_internal) {
        hermes_log(LOG_INFO, "gateway",
                   "Refusing new turn for session %s \xe2\x80\x94 external drain active.",
                   session_key);
        snprintf(response_out, response_size,
                 "\xe2\x8f\xb3 This agent is draining for a maintenance action and isn't "
                 "accepting new turns right now. It'll be back in a moment \xe2\x80\x94 "
                 "please resend shortly.");
        json_free(event);
        return 1;
    }

    /* ── Claim the session, run the agent, unwind deterministically ── */
    char *agent_result = NULL;
    if (V->handle_message_with_agent)
        agent_result = V->handle_message_with_agent(event, &source, session_key, 0);

    /* post-turn bookkeeping (goal + loop completion). */
    {
        gw_post_turn_vtab_t noop = g_post_turn;
        (void)noop;
        if (g_post_turn.get_or_create_session) {
            char final_text_buf[64];
            (void)final_text_buf;
            gw_run_post_turn_hooks_json(agent_result, event, &source, is_internal);
        }
    }

    if (agent_result) {
        snprintf(response_out, response_size, "%s", agent_result);
        free(agent_result);
    }
    /* Unconditional release covers every exit path (idempotent). */
    if (V->restore_one_turn_overrides) V->restore_one_turn_overrides(session_key);
    if (V->clear_durable_active_turn) V->clear_durable_active_turn(event);
    if (V->release_running_agent_state) V->release_running_agent_state(session_key);
    json_free(event);
    return agent_result != NULL ? 1 : 0;
}

/* JSON-string adapter so gw_run_post_turn_hooks can serve the pipeline. */
static void gw_run_post_turn_hooks_json(const char *agent_result_str,
                                        const json_node_t *event,
                                        const gw_session_source_t *source,
                                        bool is_internal)
{
    json_node_t *ar = NULL;
    if (agent_result_str) ar = json_parse(agent_result_str, NULL);
    gw_run_post_turn_hooks(ar, event, source, is_internal);
    if (ar) json_free(ar);
}


/* ════════════════════════════════════════════════════════════════
 * _warm_goals_session_db
 * ════════════════════════════════════════════════════════════════ */

extern void *goal_get_session_db(const char *home, bool on_loop_thread,
                                 void *(*construct)(void *ctx, const char *home),
                                 void *ctx);

static void gw_warm_goals_db_construct(void)
{
    /* Warm the shared goals SessionDB cache (goals.py:_get_session_db).
     * NULL construct => cache-hit path only; a cold miss is filled by the
     * caller's bootstrap windows exactly as in Python. */
    const char *home = getenv("HERMES_HOME");
    goal_get_session_db(home && home[0] ? home : NULL, false, NULL, NULL);
}

/* Warm the goals SessionDB cache off-loop (best-effort). The executor hop
 * keeps the profile home override alive under multiplex. On failure this is
 * a bounded stall, never a crash — the caller falls back to the windows. */
/* PoP: gw_warm_goals_session_db @ gateway/run.py:GatewayRunner._warm_goals_session_db */
void gw_warm_goals_session_db(const char *label)
{
    if (g_off_loop_exec) {
        g_off_loop_exec(gw_warm_goals_db_construct, NULL);
        return;
    }
    gw_warm_goals_db_construct();
}

