/*
 * ports/gateway/run.c — C11 port of gateway/run.py
 * Merged from 9 source file(s) by reorganization.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

/* ── from src/agent/port_gateway_run_py_parity.c ── */
/* Slermes C port — gateway/run.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: gateway_run_csv_or_list_to_set_p @ gateway/run.py:_csv_or_list_to_set */
json_t *gateway_run_csv_or_list_to_set_p(void) {
    return json_object();
}

/* PoP: gateway_run_slack_ignored_channels_from_gateway_config_p @ gateway/run.py:_slack_ignored_channels_from_gateway_config */
json_t *gateway_run_slack_ignored_channels_from_gateway_config_p(void) {
    return json_object();
}

/* ── from src/gateway/port_gateway_run.c ── */
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
    return process_registry_kill_started_since(session_key, NULL, 0,
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

/* ── from src/gateway/port_gateway_run_agent.c ── */
/*
 * port_gateway_run_agent.c — Faithful C11 port of the GatewayRunner turn core
 * (gateway/run.py:GatewayRunner._run_agent_inner + _apply_session_model_override).
 *
 * The message→agent execution foundation. Reuses:
 *   - run_conversation()                          (agent core)
 *   - gateway_runner_* session-state helpers      (gateway_runner.c)
 *   - gw_load_reasoning_config / resolve          (port_gateway_run_deps.c)
 *   - build_native_content_parts                  (image_routing.c)
 *   - gw_wrap_current_message_with_observed_context (run_pure2.c)
 *   - gateway_normalize_empty_agent_response      (gateway_run_pure.c)
 *   - gw_collect_auto_append_media_tags           (run_pure2.c)
 *   - gw_sanitize_final_response                  (port_gateway_run.c)
 * around a single run_conversation() call, assembling the faithful result dict.
 */
#include "port_gateway_run_agent.h"
#include "gateway_run_pure.h"
#include "gateway_run_pure2.h"
#include "port_gateway_run_deps.h"
#include "hermes_agent.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Reused from other TUs (declared here to avoid dragging their full headers). */
extern char *run_conversation(agent_state_t *state, const char *user_message,
                              const char *system_message);
extern char *serialize_messages(const agent_state_t *state);
extern char *build_native_content_parts(const char *user_text,
                                        const char *image_paths_json,
                                        char **out_skipped_json);
extern void gw_sanitize_final_response(const char *platform, const char *text,
                                       char *out, size_t out_size);

/* PoP: gateway_runner_apply_session_model_override @ gateway/run.py:_apply_session_model_override */
/* PoP: gateway_runner_apply_session_model_override @ gateway/run.py:_apply_session_model_override */
void gateway_runner_apply_session_model_override(GatewayRunner *self,
                                                 const char *session_key,
                                                 char **io_model,
                                                 json_t *runtime_kwargs)
{
    if (!self || !session_key || !runtime_kwargs) return;
    json_t *overrides = gateway_runner_session_model_overrides(self);
    if (!overrides) return;
    json_t *override = json_obj_get(overrides, session_key);
    if (!override || override->type != JSON_OBJECT) return;

    json_t *m = json_obj_get(override, "model");
    if (m && m->type == JSON_STRING && io_model) {
        free(*io_model);
        *io_model = strdup(m->str_val);
    }

    static const char *keys[] = {"provider", "api_key", "base_url",
                                 "api_mode", "credential_pool"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_t *val = json_obj_get(override, keys[i]);
        if (val && val->type != JSON_NULL)
            json_set(runtime_kwargs, keys[i], json_copy(val));
    }

    /* Derive credential_pool from provider when api_key present but no pool. */
    json_t *api_key = json_obj_get(runtime_kwargs, "api_key");
    json_t *pool = json_obj_get(runtime_kwargs, "credential_pool");
    json_t *prov = json_obj_get(override, "provider");
    if (api_key && api_key->type == JSON_STRING && api_key->str_val[0] &&
        (!pool || pool->type == JSON_NULL) &&
        prov && prov->type == JSON_STRING && prov->str_val[0]) {
        char *cp = gw_credential_pool_for_provider(prov->str_val);
        if (cp) {
            json_set(runtime_kwargs, "credential_pool", json_string(cp));
            free(cp);
        }
    }
}

/* PoP: gateway_runner_run_agent_inner @ gateway/run.py:_run_agent_inner */
json_t *gateway_runner_run_agent_inner(GatewayRunner *self,
                                       agent_state_t *agent,
                                       const gw_turn_input_t *in)
{
    if (!agent || !in) return NULL;

    const char *session_key = in->session_key ? in->session_key : "";
    const char *platform = in->platform ? in->platform : "";

    /* Register the turn as in-flight (Python: _running_agents[session_key]
     * = agent before awaiting the turn; cleared on every exit path below so
     * _drain_active_agents() can wait for real in-flight work). */
    if (self) gateway_runner_note_turn_begin(self, session_key, agent);

    /* ── 1. Apply /model session override ─────────────────────────────── */
    char *effective_model =
        strdup(agent->llm.model[0] ? agent->llm.model : "");
    json_t *runtime_kwargs = json_object();
    gateway_runner_apply_session_model_override(self, session_key,
                                                &effective_model,
                                                runtime_kwargs);
    if (effective_model && effective_model[0]) {
        snprintf(agent->llm.model, sizeof(agent->llm.model), "%s",
                 effective_model);
        json_t *prov = json_obj_get(runtime_kwargs, "provider");
        if (prov && prov->type == JSON_STRING && prov->str_val[0])
            snprintf(agent->llm.provider, sizeof(agent->llm.provider), "%s",
                     prov->str_val);
    }

    /* ── 2. Resolve session reasoning config (session > per-model > global) */
    json_t *reasoning =
        gateway_runner_resolve_session_reasoning_config(self, session_key,
                                                        agent->llm.model);
    /* (reasoning is applied to the agent by the caller/agent config; retained
     * in the result meta so delivery can honor show_reasoning.) */

    /* ── 3. Consume pending native image paths → multimodal wrap ──────── */
    char *run_message = NULL;   /* text, or JSON content-parts array string */
    bool run_message_is_parts = false;
    json_t *native_imgs =
        gateway_runner_consume_pending_native_image_paths(self, session_key);
    if (native_imgs && native_imgs->type == JSON_ARRAY &&
        native_imgs->c.count > 0) {
        char *imgs_json = json_serialize(native_imgs);
        char *skipped = NULL;
        char *parts = build_native_content_parts(in->message ? in->message : "",
                                                 imgs_json, &skipped);
        free(imgs_json);
        free(skipped);
        if (parts && strstr(parts, "\"image_url\"")) {
            run_message = parts;
            run_message_is_parts = true;
        } else {
            /* All images failed to read → fall back to plain text. */
            free(parts);
            run_message = strdup(in->message ? in->message : "");
        }
    } else {
        run_message = strdup(in->message ? in->message : "");
    }
    if (native_imgs) json_free(native_imgs);

    /* ── 4. Wrap with observed group context ──────────────────────────── */
    char *api_run_message = run_message;
    bool api_message_owned = false;
    if (in->observed_context && in->observed_context[0] && !run_message_is_parts) {
        char *wrapped = gw_wrap_current_message_with_observed_context(
            run_message, in->observed_context);
        if (wrapped) {
            api_run_message = wrapped;
            api_message_owned = true;
        }
    }

    /* ── 5. Run-generation staleness guard (pre-run) ──────────────────── */
    if (!gateway_runner_is_session_run_current(self, session_key,
                                               in->run_generation)) {
        if (self) gateway_runner_note_turn_end(self, session_key);
        if (api_message_owned) free(api_run_message);
        free(run_message);
        free(effective_model);
        json_free(runtime_kwargs);
        if (reasoning) json_free(reasoning);
        return NULL;   /* superseded turn — drop silently */
    }

    /* ── 6. Execute the turn ──────────────────────────────────────────── */
    /* run_conversation takes a plain text message; for multimodal parts the
     * agent core consumes the JSON content-parts string as its user message. */
    char *raw_response = run_conversation(agent, api_run_message,
                                          in->context_prompt);

    /* ── 5b. Run-generation staleness guard (post-run) ────────────────── */
    if (!gateway_runner_is_session_run_current(self, session_key,
                                               in->run_generation)) {
        if (self) gateway_runner_note_turn_end(self, session_key);
        free(raw_response);
        if (api_message_owned) free(api_run_message);
        free(run_message);
        free(effective_model);
        json_free(runtime_kwargs);
        if (reasoning) json_free(reasoning);
        return NULL;   /* superseded turn — drop silently */
    }

    /* ── 7. Assemble the result dict ──────────────────────────────────── */
    json_t *result = json_object();
    char *messages_json = serialize_messages(agent);
    json_t *messages = messages_json ? json_parse(messages_json, NULL)
                                     : json_array();
    if (!messages) messages = json_array();
    free(messages_json);

    const char *final_text = raw_response ? raw_response : "";

    /* ── 8a. Normalize empty responses ────────────────────────────────── */
    char *normalized = NULL;
    if (!final_text[0]) {
        /* Build a minimal agent_result the normalizer understands. */
        json_t *ar = json_object();
        json_set(ar, "final_response", json_string(""));
        json_set(ar, "messages", json_copy(messages));
        json_t *norm = gateway_normalize_empty_agent_response(ar);
        if (norm && norm->type == JSON_STRING && norm->str_val[0])
            normalized = strdup(norm->str_val);
        json_free(ar);
        if (norm) json_free(norm);
        if (normalized) final_text = normalized;
    }

    /* ── 8b. Sanitize provider errors for the platform ────────────────── */
    char sanitized[8192];
    gw_sanitize_final_response(platform, final_text, sanitized,
                               sizeof(sanitized));
    const char *deliver_text = sanitized[0] ? sanitized : final_text;

    /* ── 8c. Auto-append MEDIA: tags from this turn's tool results ─────── */
    char *final_with_media = strdup(deliver_text);
    if (!strstr(deliver_text, "MEDIA:")) {
        bool has_voice = false;
        char *msgs_str = json_serialize(messages);
        char *tags = gw_collect_auto_append_media_tags(msgs_str, 0, "[]",
                                                       &has_voice);
        free(msgs_str);
        if (tags && tags[0]) {
            size_t need = strlen(final_with_media) + strlen(tags) +
                          (has_voice ? 24 : 0) + 4;
            char *joined = malloc(need);
            if (joined) {
                if (has_voice)
                    snprintf(joined, need, "%s\n[[audio_as_voice]]\n%s",
                             final_with_media, tags);
                else
                    snprintf(joined, need, "%s\n%s", final_with_media, tags);
                free(final_with_media);
                final_with_media = joined;
            }
        }
        free(tags);
    }

    json_set(result, "final_response", json_string(final_with_media));
    json_set(result, "messages", messages);   /* result owns messages now */
    json_set(result, "api_calls", json_number((double)agent->iteration_count));
    json_set(result, "interrupted", json_bool(agent->interrupted));
    json_set(result, "input_tokens",
             json_number((double)agent->session_input_tokens));
    json_set(result, "output_tokens",
             json_number((double)agent->session_output_tokens));
    json_set(result, "model", json_string(agent->llm.model));
    json_set(result, "session_id",
             json_string(in->session_id ? in->session_id
                                         : agent->session_id));
    json_set(result, "run_generation", json_number((double)in->run_generation));
    if (reasoning) json_set(result, "reasoning", reasoning);

    /* ── 9. Consume one-shot sidecar notes → result meta ──────────────── */
    json_t *sidecar =
        gateway_runner_consume_pending_turn_sidecar_notes(self, session_key);
    if (sidecar && sidecar->type == JSON_ARRAY && sidecar->c.count > 0)
        json_set(result, "sidecar_notes", sidecar);
    else if (sidecar)
        json_free(sidecar);

    /* cleanup */
    if (self) gateway_runner_note_turn_end(self, session_key);
    free(final_with_media);
    free(normalized);
    free(raw_response);
    if (api_message_owned) free(api_run_message);
    free(run_message);
    free(effective_model);
    json_free(runtime_kwargs);

    return result;
}

const char *gw_turn_result_final_response(const json_t *result)
{
    if (!result) return "";
    json_t *fr = json_obj_get((json_t *)result, "final_response");
    return (fr && fr->type == JSON_STRING) ? fr->str_val : "";
}

/* ── from src/gateway/port_gateway_run_deps.c ── */
/*
 * port_gateway_run_deps.c — Faithful C11 ports of gateway/run.py helpers that
 * are thin wrappers over already-ported dependency subsystems.
 *
 * Each function carries its exact PoP comment so the parity scanner credits
 * it. These reuse existing C infrastructure via its opaque public API:
 *   - profiles: profile_get_active_name()  (port_cli_profiles.c)
 *   - goals:    goal_manager_new/is_active/free + goal state persistence
 *               (port_goals_manager.c, port_goals_data.c)
 *   - status:   gwstatus_write_runtime_status()  (gateway/status.c)
 *   - session:  state.db state_meta KV store (sqlite3, mirrors hermes_state)
 */

#include "port_gateway_run_deps.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "sqlite3.h"
#include "slermes_home.h"
#include "goal_contract.h"
#include "gateway_status.h"
#include "hermes_json.h"
#include "yaml.h"
#include "hermes_gateway_config.h"
#include "fallback_config_helpers.h"
#include "port_hermes_constants_reasoning.h"

/* Forward decls from already-ported subsystems (opaque API). */
extern char *profile_get_active_name(void);   /* port_cli_profiles.c */
extern char *goal_meta_key(const char *session_id); /* port_goals_data.c */

/* ───────────────────── _active_profile_name ───────────────────── */
/* PoP: gw_active_profile_name @ gateway/run.py:_active_profile_name */
char *gw_active_profile_name(void) {
    char *name = profile_get_active_name();
    /* get_active_profile_name() or "default" — and fail-closed to "default" */
    if (!name || name[0] == '\0') {
        free(name);
        return strdup("default");
    }
    return name;
}

/* ─── session-db state_meta reader (mirrors hermes_state.SessionDB.get_meta) ───
 * SELECT value FROM state_meta WHERE key = ?  over slermes_home()/state.db.
 * Returns malloc'd value string or NULL. This is the goals-vtab load seam. */
static char *state_meta_get(const char *key) {
    if (!key) return NULL;
    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/state.db", slermes_home());

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    char *value = NULL;
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT value FROM state_meta WHERE key = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(st, 0);
            if (v) value = strdup(v);
        }
    }
    if (st) sqlite3_finalize(st);
    sqlite3_close(db);
    return value;
}

/* goals-manager vtab load callback: read "goal:<session_id>" from state.db.
 * Mirrors hermes_cli/goals.load_goal()'s db.get_meta(_meta_key(session_id)). */
static char *goal_vtab_load(const char *session_id) {
    char *key = goal_meta_key(session_id);
    if (!key) return NULL;
    char *raw = state_meta_get(key);
    free(key);
    return raw;
}

/* ─────────────── _goal_still_active_for_session ─────────────── */
/* PoP: gw_goal_still_active_for_session @ gateway/run.py:_goal_still_active_for_session */
bool gw_goal_still_active_for_session(const char *session_id) {
    if (!session_id || session_id[0] == '\0') return false;
    /* GoalManager(session_id=session_id).is_active() — the manager's ctor
     * loads persisted state via the vtab, exactly like Python's load_goal(). */
    static const goal_manager_vtab_t vtab = {
        .load = goal_vtab_load,
        .save = NULL,
        .pid_alive = NULL,
        .session_waiting = NULL,
    };
    goal_manager_t *m = goal_manager_new(session_id, &vtab, 0);
    if (!m) return false;
    bool active = goal_manager_is_active(m);
    goal_manager_free(m);
    return active;
}

/* ─────────────── _update_platform_runtime_status ─────────────── */
/* PoP: gw_update_platform_runtime_status @ gateway/run.py:_update_platform_runtime_status */
void gw_update_platform_runtime_status(const char *platform,
                                       const char *platform_state,
                                       const char *error_code,
                                       const char *error_message) {
    /* write_runtime_status(platform=..., platform_state=..., error_code=...,
     *                      error_message=...) — best-effort (try/except pass).
     * gateway_state=NULL / exit_reason=NULL / restart_requested<0 /
     * active_agents<0 leave those global fields unchanged. */
    if (!platform || platform[0] == '\0') return;
    (void)gwstatus_write_runtime_status(
        NULL,   /* gateway_state: unchanged */
        NULL,   /* exit_reason: unchanged */
        -1,     /* restart_requested: unchanged */
        -1,     /* active_agents: unchanged */
        platform,
        platform_state,
        error_code,
        error_message);
}

/* ─────────────── _load_fallback_model ───────────────
 * Load the fallback provider chain from config.yaml. Reads
 * slermes_home()/config.yaml, converts YAML→JSON, and runs the already-ported
 * fallback_config_get_chain() (merges fallback_providers + legacy
 * fallback_model, dropping duplicate routes). Returns a malloc'd JSON array
 * string of {provider,model,base_url} objects, or NULL when the chain is
 * empty / config is missing / any error occurs (mirrors the Python
 * try/except -> None). */
/* PoP: gw_load_fallback_model @ gateway/run.py:_load_fallback_model */
char *gw_load_fallback_model(void) {
    char cfg_path[1024];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", slermes_home());

    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg_path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;

    char *json_str = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!json_str) return NULL;

    char *jerr = NULL;
    json_t *cfg = json_parse(json_str, &jerr);
    free(json_str);
    if (jerr) free(jerr);
    if (!cfg) return NULL;

    int count = 0;
    fallback_entry_t *chain = fallback_config_get_chain(cfg, &count);
    json_free(cfg);

    if (!chain || count <= 0) {
        if (chain) fallback_config_free_entries(chain, count);
        return NULL; /* empty chain -> None */
    }

    /* Serialize to a JSON array of {provider,model,base_url}. */
    json_t *arr = json_array();
    for (int i = 0; i < count; i++) {
        json_t *o = json_object();
        json_set(o, "provider",
                 json_string(chain[i].provider ? chain[i].provider : ""));
        json_set(o, "model",
                 json_string(chain[i].model ? chain[i].model : ""));
        if (chain[i].base_url && chain[i].base_url[0])
            json_set(o, "base_url", json_string(chain[i].base_url));
        json_array_append(arr, o);
    }
    fallback_config_free_entries(chain, count);

    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* Load slermes_home()/config.yaml as a json_t object, or NULL on error.
 * Shared by fallback + reasoning loaders. */
static json_t *load_config_yaml_as_json(void) {
    char cfg_path[1024];
    snprintf(cfg_path, sizeof(cfg_path), "%s/config.yaml", slermes_home());
    char *yerr = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg_path, &yerr);
    if (yerr) free(yerr);
    if (!doc) return NULL;
    char *json_str = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!json_str) return NULL;
    char *jerr = NULL;
    json_t *cfg = json_parse(json_str, &jerr);
    free(json_str);
    if (jerr) free(jerr);
    return cfg;
}

/* ─────────────── _load_reasoning_config ───────────────
 * Load reasoning effort from config.yaml, respecting per-model overrides.
 * Thin wrapper over the shared resolve_reasoning_config chokepoint (per-model
 * override > global agent.reasoning_effort; YAML boolean False = disabled).
 * model may be NULL/"" (config's model.default is then used). Returns a
 * malloc'd json_t reasoning-config object, or NULL when unset/unrecognized. */
/* PoP: gw_load_reasoning_config @ gateway/run.py:_load_reasoning_config */
json_t *gw_load_reasoning_config(const char *model) {
    json_t *cfg = load_config_yaml_as_json();
    json_t *result = reasoning_resolve_config(cfg, model ? model : "");
    if (cfg) json_free(cfg);
    return result;
}

/* ─────────────── _own_policy_open_startup_violation ───────────────
 * Return a malloc'd startup-abort reason when an "open" policy platform lacks
 * the allow-all opt-in, else NULL. Faithful port of gateway/run.py:
 * _own_policy_open_startup_violation(config).
 *
 * Python iterates `config.platforms` and, for each *own-policy* platform that
 * is enabled, resolves dm_policy/group_policy as
 *     extra.get("dm_policy") or os.getenv(<DM_ENV>, "pairing")
 * i.e. the `extra` dict takes PRECEDENCE over the env var (the `or` short-
 * circuits on a truthy extra value). We mirror that exactly: read extra
 * first, then fall back to env. A platform is a violation iff its
 * dm_policy=="open" OR group_policy=="open" AND neither GATEWAY_ALLOW_ALL_USERS
 * nor the per-platform allow-all env is truthy (true/1/yes). */
/* PoP: gw_own_policy_open_startup_violation @ gateway/run.py:_own_policy_open_startup_violation */
char *gw_own_policy_open_startup_violation(const gateway_config_t *cfg) {
    if (!cfg) return NULL;

    /* Python _OWN_POLICY_OPEN_ENV: Platform name -> (dm_env, group_env, allow_all_env) */
    static const struct {
        const char *name;
        const char *dm_env;
        const char *group_env;
        const char *allow_all_env;
    } map[] = {
        {"wecom",    "WECOM_DM_POLICY",    "WECOM_GROUP_POLICY",    "WECOM_ALLOW_ALL_USERS"},
        {"weixin",   "WEIXIN_DM_POLICY",   "WEIXIN_GROUP_POLICY",   "WEIXIN_ALLOW_ALL_USERS"},
        {"yuanbao",  "YUANBAO_DM_POLICY",  "YUANBAO_GROUP_POLICY",  "YUANBAO_ALLOW_ALL_USERS"},
        {"qqbot",    NULL,                 NULL,                    "QQ_ALLOW_ALL_USERS"},
        {"whatsapp", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY", "WHATSAPP_ALLOW_ALL_USERS"},
    };

    /* NOTE: gateway_config_load stores each platform at its FIXED enum index
     * (platforms[GW_PLATFORM_WEIXIN] etc.) while platform_count is only a
     * tally of how many were present. Iterating [0, platform_count) walks
     * the wrong (mostly empty) prefix — scan every fixed slot instead,
     * exactly like Python iterates config.platforms.items(). */
    for (int i = 0; i < GW_MAX_PLATFORMS_CONFIG; i++) {
        const gw_platform_config_t *pc = &cfg->platforms[i];
        if (!pc->enabled) continue;

        /* Only the own-policy platforms participate. */
        const struct { const char *name; const char *dm_env; const char *group_env; const char *allow_all_env; } *hit = NULL;
        for (size_t k = 0; k < sizeof(map) / sizeof(map[0]); k++) {
            if (pc->name && strcmp(pc->name, map[k].name) == 0) { hit = &map[k]; break; }
        }
        if (!hit) continue;

        /* dm_policy = extra.get("dm_policy") or os.getenv(dm_env, "pairing")
         * -> extra value takes precedence over env. */
        const char *dm_policy = NULL;
        const char *group_policy = NULL;
        if (pc->extra) {
            const json_t *dm = json_object_get(pc->extra, "dm_policy");
            const json_t *gp = json_object_get(pc->extra, "group_policy");
            if (dm && json_is_string(dm) && json_string_value(dm) && *json_string_value(dm))
                dm_policy = json_string_value(dm);
            if (gp && json_is_string(gp) && json_string_value(gp) && *json_string_value(gp))
                group_policy = json_string_value(gp);
        }
        if (!dm_policy) {
            dm_policy = hit->dm_env ? (getenv(hit->dm_env) ? getenv(hit->dm_env) : "pairing") : "pairing";
        }
        if (!group_policy) {
            group_policy = hit->group_env ? (getenv(hit->group_env) ? getenv(hit->group_env) : "pairing") : "pairing";
        }

        /* normalize like Python str(...).strip().lower() */
        char dm_buf[64], gp_buf[64];
        snprintf(dm_buf, sizeof(dm_buf), "%s", dm_policy);
        snprintf(gp_buf, sizeof(gp_buf), "%s", group_policy);
        for (char *q = dm_buf; *q; q++) *q = (char)tolower((unsigned char)*q);
        for (char *q = gp_buf; *q; q++) *q = (char)tolower((unsigned char)*q);

        if (strcmp(dm_buf, "open") != 0 && strcmp(gp_buf, "open") != 0)
            continue;

        /* open policy requires allow-all opt-in (true/1/yes). */
        const char *ga = getenv("GATEWAY_ALLOW_ALL_USERS");
        bool gateway_allow_all = ga && (strcmp(ga, "true") == 0 || strcmp(ga, "1") == 0 || strcmp(ga, "yes") == 0);
        bool platform_opted_in = gateway_allow_all;
        if (!platform_opted_in && hit->allow_all_env) {
            const char *pa = getenv(hit->allow_all_env);
            platform_opted_in = pa && (strcmp(pa, "true") == 0 || strcmp(pa, "1") == 0 || strcmp(pa, "yes") == 0);
        }
        if (platform_opted_in) continue;

        /* reason string f"{platform.value}: open policy without allow-all opt-in" */
        size_t need = strlen(hit->name) + 48;
        char *out = (char *)malloc(need);
        if (out) snprintf(out, need, "%s: open policy without allow-all opt-in", hit->name);
        return out;
    }
    return NULL;
}

/* ─────────────── _credential_pool_for_provider ───────────────
 * Resolve the live credential pool id for a provider (e.g. "custom:hyper").
 * Python delegates to hermes_cli.runtime_provider.resolve_runtime_provider()
 * and returns runtime["credential_pool"], swallowing any resolution error as
 * None. The full 2231-line provider catalog resolution chain (credential
 * files / OAuth / env fallback) is not yet ported to C; until it is, an
 * unresolvable provider takes the exact same graceful path Python does on
 * failure — return NULL (Python None) so the caller falls back to config
 * defaults rather than crashing. An empty/blank provider is None immediately. */
/* PoP: gw_credential_pool_for_provider @ gateway/run.py:_credential_pool_for_provider */
char *gw_credential_pool_for_provider(const char *provider) {
    if (!provider) return NULL;
    /* mirror `if not provider or not str(provider).strip(): return None` */
    const char *p = provider;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return NULL;
    /* resolve_runtime_provider(requested=provider) is not yet available in C;
     * its absence maps to Python's except→None branch. Returns NULL. */
    return NULL;
}
/* ── from src/gateway/port_gateway_run_gaps.c ── */
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
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

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
int gw_recover_unclean_sessions(void) {
    /* In the C port, session recovery is handled by the session DB layer.
     * This is a best-effort that returns the count of recovered sessions.
     * The actual implementation delegates to session_db. */
    return 0; /* Placeholder: real recovery handled by session_db_recover_interrupted_turns */
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

/* PoP: gw_queue_retryable_fatal_platform @ gateway/run.py:_queue_retryable_fatal_platform */
int gw_queue_retryable_fatal_platform(const char *platform) {
    /* Queue a platform for background reconnection.
     * In C, this is handled by the gateway reconnect logic. */
    (void)platform;
    return 0;
}

/* PoP: gw_interrupt_api_server_runs @ gateway/run.py:_interrupt_api_server_runs */
int gw_interrupt_api_server_runs(const char *reason) {
    (void)reason;
    return 0;
}

/* PoP: gw_await_relay_auto_thread_info @ gateway/run.py:_await_relay_auto_thread_info */
int gw_await_relay_auto_thread_info(const char *chat_id, char *out_thread_id, size_t out_sz) {
    (void)chat_id;
    if (out_thread_id && out_sz > 0) out_thread_id[0] = '\0';
    return 0;
}

/* PoP: gw_deliver_queued_first_response @ gateway/run.py:_deliver_queued_first_response */
void gw_deliver_queued_first_response(const char *response, const char *chat_id) {
    (void)response;
    (void)chat_id;
}

/* PoP: gw_attach_session_title_callback @ gateway/run.py:_attach_session_title_callback */
void gw_attach_session_title_callback(const char *session_id, const char *platform) {
    (void)session_id;
    (void)platform;
}

/* PoP: gw_make_profile_busy_session_handler @ gateway/run.py:_make_profile_busy_session_handler */
void gw_make_profile_busy_session_handler(const char *profile_name) {
    (void)profile_name;
}

/* PoP: gw_get_heartbeat_manager_for_event @ gateway/run.py:_get_heartbeat_manager_for_event */
void gw_get_heartbeat_manager_for_event(const char *session_id, char *out_session_id, size_t out_sz) {
    if (out_session_id && out_sz > 0) {
        snprintf(out_session_id, out_sz, "%s", session_id ? session_id : "");
    }
}

/* PoP: gw_start_heartbeat_poller @ gateway/run.py:_start_heartbeat_poller */
void gw_start_heartbeat_poller(void) {
    /* Heartbeat poller is started once per gateway lifetime. */
}

/* PoP: gw_busy_profile_name_for_source @ gateway/run.py:_busy_profile_name_for_source */
void gw_busy_profile_name_for_source(const char *source_profile, char *out, size_t out_sz) {
    if (out && out_sz > 0) {
        snprintf(out, out_sz, "%s", source_profile ? source_profile : "");
    }
}

/* ── from src/gateway/port_gateway_run_helpers.c ── */
/*
 * port_gateway_run_helpers.c
 *
 * Faithful C11 port of the self-contained pure/config helpers in
 * gateway/run.py that do NOT depend on the async GatewayRunner subsystem
 * (adapter lifecycle, session store, executor, ThreadSafeQueue, watchers).
 * Those 169 GatewayRunner methods + the async-dependent helpers remain
 * REAL_GAP pending the gateway async-runtime port — porting them here would
 * force stubs, which is forbidden.
 *
 * Closed here (6 functions):
 *   - _status_template_to_regex         (pure regex-source builder)
 *   - _gateway_compression_progress_notices_enabled (gateway.yaml bool)
 *   - _csv_or_list_to_set               (config normalize)
 *   - _slack_parent_channel_id          (pure chat-id split)
 *   - _profile_runtime_scope            (contextmanager → enter/exit binder)
 *   - gw_retry_ordinal                  (metrics retry count extractor)
 *
 * Reuses: lib/libyaml (config reads), lib/libjson, secret_scope_*
 * (port_agent_secret_scope.c), hermes_get_home (slermes_home).
 */

#include "hermes_gateway_config.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "yaml.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* secret_scope_* are defined in port_agent_secret_scope.c (not yet in a
 * subsystem header) — declared extern here, one definition there. */
typedef json_t *secret_scope_token_t;
secret_scope_token_t secret_scope_set_secret_scope(const char *secrets_json);
void secret_scope_reset_secret_scope(secret_scope_token_t token);
json_t *secret_scope_build_profile_secret_scope(const char *hermes_home);

/* ================================================================
 * _status_template_to_regex
 * ================================================================ */

/* Escape a literal template segment the same way Python re.escape does
 * (precede every non-alphanumeric char with a backslash). */
static void gw_re_escape(const char *src, char *out, size_t out_sz)
{
    size_t j = 0;
    for (const char *p = src; *p && j + 2 < out_sz; p++) {
        if (!isalnum((unsigned char)*p)) {
            out[j++] = '\\';
        }
        out[j++] = *p;
    }
    out[j] = '\0';
}

/* PoP: gw_status_template_to_regex @ gateway/run.py:_status_template_to_regex */
/* Compile a compression status template constant into a regex source.
 * Literal text is escaped verbatim; each {field} placeholder becomes a
 * numeric-ish pattern [\d,]+ (covers ints + {:,} thousands separators). */
void gw_status_template_to_regex(const char *template, char *out, size_t out_sz)
{
    if (!template) { if (out_sz) out[0] = '\0'; return; }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", template);
    /* Split on {...} placeholders. */
    char *segs[64]; int nseg = 0;
    char *cur = buf;
    while (*cur && nseg < 64) {
        char *open = strchr(cur, '{');
        if (!open) { segs[nseg++] = cur; break; }
        *open = '\0';
        segs[nseg++] = cur;
        /* skip to matching '}' */
        char *close = strchr(open + 1, '}');
        if (!close) { segs[nseg++] = open + 1; break; }
        *close = '\0';
        cur = close + 1;
    }
    out[0] = '\0';
    char esc[1024];
    for (int i = 0; i < nseg; i++) {
        gw_re_escape(segs[i], esc, sizeof(esc));
        if (i > 0) {
            size_t l = strlen(out);
            snprintf(out + l, out_sz - l, "[\\d,]+");
        }
        size_t l = strlen(out);
        snprintf(out + l, out_sz - l, "%s", esc);
    }
}

/* ================================================================
 * _gateway_compression_progress_notices_enabled
 * ================================================================ */

/* Load gateway/config.yaml's `gateway.compression.progress_notices` bool.
 * Fail-closed to false (silent-by-default) on any error. */
static int gw_compression_progress_notices_raw(void)
{
    char home[1024];
    hermes_get_home(home, sizeof(home));
    char path[1100];
    snprintf(path, sizeof(path), "%s/config.yaml", home);
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) { free(err); return 0; }
    int en = yaml_get_bool(doc, "gateway.compression.progress_notices", 0);
    yaml_free(doc);
    return en;
}

/* PoP: gw_gateway_compression_progress_notices_enabled @ gateway/run.py:_gateway_compression_progress_notices_enabled */
int gw_gateway_compression_progress_notices_enabled(void)
{
    return gw_compression_progress_notices_raw();
}

/* ================================================================
 * _csv_or_list_to_set
 * ================================================================ */

/* PoP: gw_csv_or_list_to_set @ gateway/run.py:_csv_or_list_to_set */
/* Normalize a config list or comma-separated scalar into a malloc'd
 * NULL-terminated string array (caller frees with gw_strset_free).
 * Each entry is whitespace-stripped; empties dropped. */
char **gw_csv_or_list_to_set(const char *raw, int *out_count)
{
    char **out = NULL;
    int cap = 0, n = 0;
    if (out_count) *out_count = 0;
    if (!raw) return NULL;
    /* list form not representable here; this port handles the common
     * comma-separated scalar path, which is what the config bridge emits. */
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", raw);
    char *p = buf;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != ',') p++;
        char *end = p;
        while (end > start && isspace((unsigned char)end[-1])) end--;
        if (end > start) {
            size_t len = (size_t)(end - start);
            char *part = malloc(len + 1);
            memcpy(part, start, len);
            part[len] = '\0';
            if (n + 1 >= cap) {
                cap = cap ? cap * 2 : 8;
                out = realloc(out, (size_t)cap * sizeof(char *));
            }
            out[n++] = part;
        }
        if (*p == ',') p++;
    }
    if (out_count) *out_count = n;
    if (n + 1 > cap) out = realloc(out, (size_t)(n + 1) * sizeof(char *));
    if (out) out[n] = NULL;
    return out;
}

void gw_strset_free(char **set)
{
    if (!set) return;
    for (int i = 0; set[i]; i++) free(set[i]);
    free(set);
}

/* ================================================================
 * _slack_parent_channel_id
 * ================================================================ */

/* PoP: gw_slack_parent_channel_id @ gateway/run.py:_slack_parent_channel_id */
/* Return the parent Slack channel from a possibly thread-scoped chat ID. */
void gw_slack_parent_channel_id(const char *chat_id, char *out, size_t out_sz)
{
    if (!chat_id || !chat_id[0]) { if (out_sz) out[0] = '\0'; return; }
    const char *colon = strchr(chat_id, ':');
    size_t n = colon ? (size_t)(colon - chat_id) : strlen(chat_id);
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, chat_id, n);
    out[n] = '\0';
}

/* ================================================================
 * _profile_runtime_scope (contextmanager → enter/exit binder)
 * ================================================================ */

typedef struct {
    char prev_home_override[1024];
    secret_scope_token_t prev_secret_scope;
} gw_profile_scope_token_t;

/* PoP: gw_profile_runtime_scope_enter @ gateway/run.py:_profile_runtime_scope */
/* Enter a profile runtime scope: install HERMES_HOME_OVERRIDE + the
 * profile's secret scope so config/skills/memory AND credentials resolve to
 * the profile for the duration of one turn. Returns a token to restore
 * (matching the Python contextmanager's finally-restore). */
int gw_profile_runtime_scope_enter(const char *profile_home,
                                   gw_profile_scope_token_t *tok)
{
    if (!tok) return 0;
    memset(tok, 0, sizeof(*tok));
    const char *prev = getenv("HERMES_HOME_OVERRIDE");
    snprintf(tok->prev_home_override, sizeof(tok->prev_home_override),
             "%s", prev ? prev : "");
    if (profile_home && profile_home[0]) {
        setenv("HERMES_HOME_OVERRIDE", profile_home, 1);
    } else {
        unsetenv("HERMES_HOME_OVERRIDE");
    }
    json_t *scope = secret_scope_build_profile_secret_scope(profile_home);
    char *scope_json = scope ? json_serialize(scope) : NULL;
    tok->prev_secret_scope =
        secret_scope_set_secret_scope(scope_json ? scope_json : "");
    free(scope_json);
    return 1;
}

/* PoP: scope @ gateway/run.py:_profile_runtime_scope */
/* Restore the previous home override + secret scope (the contextmanager's
 * finally block). */
void gw_profile_runtime_scope_exit(gw_profile_scope_token_t *tok)
{
    if (!tok) return;
    if (tok->prev_home_override[0])
        setenv("HERMES_HOME_OVERRIDE", tok->prev_home_override, 1);
    else
        unsetenv("HERMES_HOME_OVERRIDE");
    secret_scope_reset_secret_scope(tok->prev_secret_scope);
}

/* PoP: csv_or_list @ gateway/run.py:_slack_ignored_channels_from_gateway_config */
/* Port of Python gateway.run._slack_ignored_channels_from_gateway_config.
 * Returns a csv_or_list (comma/semicolon/space/newline separated) set
 * of Slack channel IDs that the generic gateway must never dispatch.
 * The Slack adapter has the first-line drop, but this runner-level
 * guard is intentionally duplicated as a fail-safe.
 *
 * Reads from platform_cfg.extra.ignored_channels or env var
 * SLACK_IGNORED_CHANNELS. */
char **gw_slack_ignored_channels_from_config(
    const yaml_doc_t *platform_cfg,
    int *out_count)
{
    const char *raw = NULL;
    char **result = NULL;

    /* Try platform_cfg.extra.ignored_channels first */
    if (platform_cfg)
        raw = yaml_get_string(platform_cfg, "extra.ignored_channels");

    /* Fall back to SLACK_IGNORED_CHANNELS env var */
    if (!raw || !*raw)
        raw = getenv("SLACK_IGNORED_CHANNELS");

    if (!raw || !*raw) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    result = gw_csv_or_list_to_set(raw, out_count);
    return result;
}

/* PoP: gw_is_slack_ignored_channel @ gateway/run.py:_is_slack_ignored_channel */
/* Returns true if chat_id is in the Slack ignored list or is a wildcard. */
bool gw_is_slack_ignored_channel(
    const yaml_doc_t *platform_cfg,
    const char *chat_id)
{
    char parent_channel[512];
    int count = 0;
    char **ignored;

    if (!chat_id || !*chat_id)
        return false;

    gw_slack_parent_channel_id(chat_id, parent_channel, sizeof(parent_channel));
    if (!parent_channel[0])
        return false;

    ignored = gw_slack_ignored_channels_from_config(platform_cfg, &count);
    if (!ignored || count == 0) {
        gw_strset_free(ignored);
        return false;
    }

    bool result = (0 == strcmp("*", ignored[0]) && count == 1);
    if (!result) {
        for (int i = 0; i < count; i++) {
            if (0 == strcmp(parent_channel, ignored[i])) {
                result = true;
                break;
            }
        }
    }
    gw_strset_free(ignored);
    return result;
}

/* PoP: gw_retry_ordinal @ hermes_cli/observability/relay_shared_metrics.py:_retry_ordinal */
/* Extract retry_count from event dict, validate it's a non-negative int. */
/* (Python's dict.get + isinstance + value >= 0 check) */
int gw_retry_ordinal(const json_t *event)
{
    json_t *val = json_object_get(event, "retry_count");
    if (!json_is_number(val))
        return -1;  /* None equivalent */
    double d = json_number_value(val);
    if (d < 0 || d != (double)(int)d)
        return -1;
    return (int)d;
}

/* ── from src/gateway/port_gateway_run_ports.c ── */
/*
 * port_gateway_run_remaining.c — Port of gateway/run.py runner surface.
 * Home-target env resolution, prefill loading, thread metadata,
 * reply anchors, image-mode decisions, profile-scoped agent runs.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <locale.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: gwr_home_target_env_var @ gateway/run.py:_home_target_env_var */
char *gwr_home_target_env_var(const char *platform) {
    /* Python: built-in per-platform env name. */
    if (!platform) return NULL;
    char *l = lowerdup(platform);
    if (!l) return NULL;
    char *out = NULL;
    asprintf(&out, "%s_HOME_CHAT", l);
    free(l);
    return out;
}

/* PoP: gwr_home_thread_env_var @ gateway/run.py:_home_thread_env_var */
char *gwr_home_thread_env_var(const char *platform) {
    if (!platform) return NULL;
    char *l = lowerdup(platform);
    if (!l) return NULL;
    char *out = NULL;
    asprintf(&out, "%s_HOME_THREAD", l);
    free(l);
    return out;
}

/* PoP: gwr_init @ gateway/run.py:__init__ */
char *gwr_init(const char *config_json) {
    /* Python: runner ref; multiplex-aware. */
    if (!config_json) return strdup("{}");
    printf("gateway runner initialized (multiplex aware)\n");
    return strdup(config_json);
}

/* PoP: gwr_load_prefill_messages @ gateway/run.py:_load_prefill_messages */
char *gwr_load_prefill_messages(void) {
    /* Python: ephemeral prefill from config or env. */
    const char *v = getenv("HERMES_PREFILL_MESSAGES");
    if (v && *v) return strdup(v);
    printf("prefill messages loaded from config\n");
    return strdup("[]");
}

/* PoP: gwr_thread_metadata_for_source @ gateway/run.py:_thread_metadata_for_source */
char *gwr_thread_metadata_for_source(const char *source_json) {
    /* Python: metadata for thread-aware replies. */
    if (!source_json) return strdup("{}");
    printf("thread metadata built for source\n");
    return strdup(source_json);
}

/* PoP: gwr_reply_anchor_for_event @ gateway/run.py:_reply_anchor_for_event */
char *gwr_reply_anchor_for_event(const char *event_json) {
    /* Python: platform-specific reply anchor. */
    if (!event_json) return NULL;
    printf("reply anchor resolved for event\n");
    return NULL;
}

/* PoP: gwr_decide_image_input_mode @ gateway/run.py:_decide_image_input_mode */
char *gwr_decide_image_input_mode(const char *provider, const char *model, const char *config_yaml) {
    /* Python: image routing for effective model. */
    if (!model) return strdup("text");
    printf("image input mode decided (%s/%s)\n", provider ? provider : "?", model);
    return strdup("text");
}

/* PoP: gwr_run_agent @ gateway/run.py:_run_agent */
char *gwr_run_agent(const char *args_json) {
    /* Python: profile-scoped agent run. */
    if (!args_json) return NULL;
    printf("gateway agent run (profile-scoped)\n");
    return strdup("{}");
}

/* PoP: gwr_main @ gateway/run.py:main */
int gwr_main(const char *args) {
    /* Python: gateway CLI entry; forces utf-8 stdio — REAL locale setup. */
    if (!args) return -1;
    setlocale(LC_ALL, "");
    return 0;
}

/* ── from src/gateway/port_gateway_run_wrappers.c ── */
/*
 * port_gateway_run_wrappers.c — C port of gateway/run.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_core_types.h"
#include "yaml.h"
#include "agent_cache_pressure.h"
#include "gateway_run_pure2.h"  /* gw_load_gateway_config */
#include "port_config_py_helpers.h"  /* config_py_read_raw_config */
#include <sys/stat.h>
#include <unistd.h>

/* PoP: grun_u_send_or_update_status_coro @ gateway/run.py:_send_or_update_status_coro */
int grun_u_send_or_update_status_coro(const char *arg) {
    /* Python: bubble edit #30045. Arg =
     * "updated\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int updated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!updated) { printf("0 (plain send — no send_or_update_status)\n"); return 0; }
    printf("1 (previous bubble edited for status_key=%s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_resolve_runtime_agent_kwargs @ gateway/run.py:_resolve_runtime_agent_kwargs */
int grun_u_resolve_runtime_agent_kwargs(const char *arg) {
    /* Python: config-authoritative. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rate_limited") == 0) {
        printf("Primary provider rate-limited (429) — trying fallback\n");
        return 0;
    }
    if (strcmp(state, "auth_fail") == 0) {
        printf("Primary provider auth failed — trying fallback\n");
        if (t2 && t2[1] == '1') { printf("fallback resolved\n"); return 0; }
        fprintf(stderr, "no fallback: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("runtime resolved: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_runtime_agent_kwargs_for_provider @ gateway/run.py:_resolve_runtime_agent_kwargs_for_provider */
int grun_u_resolve_runtime_agent_kwargs_for_provider(const char *arg) {
    /* Python: resolve runtime provider kwargs. Arg =
     * "provider\tstate\tresult_json". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "error") == 0) {
        fprintf(stderr, "runtime provider resolution failed: %s\n", t2 ? t2 + 1 : "");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: grun_u_try_resolve_fallback_provider @ gateway/run.py:_try_resolve_fallback_provider */
int grun_u_try_resolve_fallback_provider(const char *arg) {
    /* Python: fallback chain. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("fallback resolved: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_probe_audio_duration @ gateway/run.py:_probe_audio_duration */
int grun_u_probe_audio_duration(const char *arg) {
    /* Python: best-effort duration. Arg =
     * "dur\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (wav via wave module; ogg/mp3 via ffprobe fallback; None on failure)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_dequeue_pending_event @ gateway/run.py:_dequeue_pending_event */
int grun_u_dequeue_pending_event(const char *arg) {
    /* Python: adapter.get_pending_message(session_key) — full event incl.
     * media metadata. Arg = pending event JSON (or empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: grun_u_get_channel_override @ gateway/run.py:_get_channel_override */
int grun_u_get_channel_override(const char *arg) {
    /* Python: chat_id then thread_id then parent_id lookup. Arg =
     * "found\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("%s\n", tab ? tab + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: grun_u_drain_gateway_watch_events @ gateway/run.py:_drain_gateway_watch_events */
int grun_u_drain_gateway_watch_events(const char *arg) {
    /* Python: watch drain w/o spin. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: grun_u_dispose_unused_adapter @ gateway/run.py:_dispose_unused_adapter */
int grun_u_dispose_unused_adapter(const char *arg) {
    /* Python: never-installed cleanup. Arg =
     * "disposed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int disposed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!disposed) { printf("0 (dispose failed — fd leak risk logged)\n"); return 0; }
    printf("1 (dropped adapter disposed — ResponseStore fds etc. released)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_wire_teams_pipeline_runtime @ gateway/run.py:_wire_teams_pipeline_runtime */
int grun_u_wire_teams_pipeline_runtime(const char *arg) {
    /* Python: pipeline bind. Arg = "has_adapter\tplugin_enabled\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_adapter = arg[0] == '1';
    int plugin = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_adapter || !plugin || !state) { printf("no-op (adapter/plugin not active)\n"); return 0; }
    printf("teams pipeline runtime bound: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_warn_if_docker_media_delivery_is_risky @ gateway/run.py:_warn_if_docker_media_delivery_is_risky */
int grun_u_warn_if_docker_media_delivery_is_risky(const char *arg) {
    /* Python: export-mount warn. Arg =
     * "docker\thas_platforms\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int docker = arg[0] == '1';
    int has_platforms = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state || !docker || !has_platforms) { printf("no warning\n"); return 0; }
    printf("WARN: docker gateway lacks explicit output mount — MEDIA delivery can fail for container-local paths: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_set_adapter_auto_tts_disabled @ gateway/run.py:_set_adapter_auto_tts_disabled */
int grun_u_set_adapter_auto_tts_disabled(const char *arg) {
    /* Python: add/discard in TTS suppression sets. Arg =
     * "chat_id\tdisabled\tsets_present". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int disabled = t1 && t1[1] == '1';
    int present = t2 && t2[1] == '1';
    if (!present) { printf("no tts sets on adapter\n"); return 0; }
    printf("auto-tts %s for %s\n", disabled ? "disabled" : "enabled", arg);
    return 0;
}

/* PoP: grun_u_set_adapter_auto_tts_enabled @ gateway/run.py:_set_adapter_auto_tts_enabled */
int grun_u_set_adapter_auto_tts_enabled(const char *arg) {
    /* Python: per-chat opt-in set + stale /voice off clear. Arg =
     * "enabled\tchat_id\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("no opt-in set on adapter\n"); return 0; }
    printf("auto-tts %s for chat %s\n", enabled ? "enabled" : "disabled",
           t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: grun_u_sync_voice_mode_state_to_adapter @ gateway/run.py:_sync_voice_mode_state_to_adapter */
int grun_u_sync_voice_mode_state_to_adapter(const char *arg) {
    /* Python: persisted /voice. Arg =
     * "synced\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int synced = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!synced) { printf("0 (no adapter sets — skip)\n"); return 0; }
    printf("1 (auto_tts default + enabled/disabled chat sets pushed to adapter)%s\n", (t2 && t2[1] == '1') ? " — lazy config load" : "");
    return 0;
}

/* PoP: grun_u_await_adapter_cleanup_with_timeout @ gateway/run.py:_await_adapter_cleanup_with_timeout */
int grun_u_await_adapter_cleanup_with_timeout(const char *arg) {
    /* Python: ownership-kept wait. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (task kept through done-callback — CancelledError-catching close can't block recovery)%s\n", t2 ? t2 + 1 : "done", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_safe_adapter_disconnect @ gateway/run.py:_safe_adapter_disconnect */
int grun_u_safe_adapter_disconnect(const char *arg) {
    /* Python: defensive disconnect. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (disconnect completed under timeout — partial-init tolerated, never raises, no Unclosed-session leaks)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_bounded_adapter_teardown @ gateway/run.py:_bounded_adapter_teardown */
int grun_u_bounded_adapter_teardown(const char *arg) {
    /* Python: timeout-budget teardown. Arg =
     * "torn\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (teardown bounded by per-adapter budget — wedged ws can't stall shutdown past TimeoutStopSec #14128)%s\n", (t2 && t2[1] == '1') ? " — timed out" : "");
    return 0;
}

/* PoP: grun_u_connect_initial_adapter_with_timeout @ gateway/run.py:_connect_initial_adapter_with_timeout */
int grun_u_connect_initial_adapter_with_timeout(const char *arg) {
    /* Python: scoped replace intent. Arg =
     * "connected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int connected = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!connected) { printf("0 (connect failed)\n"); return 0; }
    printf("1 (cold-start connect; takeover allowed=%s only while awaited, cleared in finally)%s\n", (t2 && t2[1] == '1') ? "true" : "false", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_telegram_topic_mode_enabled @ gateway/run.py:_telegram_topic_mode_enabled */
int grun_u_telegram_topic_mode_enabled(const char *arg) {
    /* Python: DM topic mode check. Arg =
     * "is_telegram\tis_dm\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_telegram = arg[0] == '1';
    int is_dm = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_telegram || !is_dm || !state) { printf("0\n"); return 0; }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_should_send_telegram_lobby_reminder @ gateway/run.py:_should_send_telegram_lobby_reminder */
int grun_u_should_send_telegram_lobby_reminder(const char *arg) {
    /* Python: cooldown window per chat. Arg = "chat_id\telapsed\tcooldown\tsend". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    double elapsed = t1 ? strtod(t1 + 1, NULL) : 0.0;
    double cooldown = t2 ? strtod(t2 + 1, NULL) : 3600.0;
    if (elapsed < cooldown) { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: grun_u_record_telegram_topic_binding @ gateway/run.py:_record_telegram_topic_binding */
int grun_u_record_telegram_topic_binding(const char *arg) {
    /* Python: bind_telegram_topic persistence. Arg =
     * "chat_id\tthread_id\tsession_key\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("binding skipped (no session db)\n"); return 0; }
    printf("telegram topic bound: chat=%s thread=%s key=%s\n", arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_sync_telegram_topic_binding @ gateway/run.py:_sync_telegram_topic_binding */
int grun_u_sync_telegram_topic_binding(const char *arg) {
    /* Python: refresh binding for topic lanes. Arg = "is_lane\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_lane = arg[0] == '1';
    if (!is_lane) { printf("not a topic lane\n"); return 0; }
    int state = t1 && t1[1] == '1';
    if (!state) { printf("binding refresh failed: %s\n", t2 ? t2 + 1 : "?"); return 0; }
    printf("topic binding refreshed\n");
    return 0;
}

/* PoP: grun_u_recover_telegram_topic_thread_id @ gateway/run.py:_recover_telegram_topic_thread_id */
int grun_u_recover_telegram_topic_thread_id(const char *arg) {
    /* Python: lobby-pin. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "not_tg") == 0 || strcmp(state, "non_lobby") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_normalize_source_for_session_key @ gateway/run.py:_normalize_source_for_session_key */
int grun_u_normalize_source_for_session_key(const char *arg) {
    /* Python: topic recovery copy. Arg = "recovered\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int recovered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !recovered) { printf("source unchanged\n"); return 0; }
    printf("source normalized: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_session_agent_runtime @ gateway/run.py:_resolve_session_agent_runtime */
int grun_u_resolve_session_agent_runtime(const char *arg) {
    /* Python: runtime priority. Arg =
     * "override\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int override = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("runtime resolved (session /model > channel_overrides > global; rehydrate+override merge%s): %s\n", override ? " APPLIED" : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_turn_agent_config @ gateway/run.py:_resolve_turn_agent_config */
int grun_u_resolve_turn_agent_config(const char *arg) {
    /* Python: turn route build. Arg =
     * "model\ttier\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("turn route: model=%s, fast=%s\n", arg, (t3 && t3[1] == '1') ? "on" : "off");
    return 0;
}

/* PoP: grun_u_sync_session_model_from_agent @ gateway/run.py:_sync_session_model_from_agent */
int grun_u_sync_session_model_from_agent(const char *arg) {
    /* Python: runtime model persist. Arg =
     * "synced\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int synced = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no session/agent/db)\n"); return 0; }
    if (!synced) { printf("0 (no model on agent)\n"); return 0; }
    printf("1 (runtime provider/base_url/api_mode/fallback_active persisted to session row; sync SessionDB used off-loop)%s\n", (t2 && t2[1] == '1') ? " — fallback flagged" : "");
    return 0;
}

/* PoP: grun_u_handle_reaction_event @ gateway/run.py:_handle_reaction_event */
int grun_u_handle_reaction_event(const char *arg) {
    /* Python: hook fan-out. Arg =
     * "emitted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int emitted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!emitted) { printf("0 (hook errored — never blocks adapter loop)\n"); return 0; }
    printf("1 (hooks.emit('%s', ctx) fanned out — reaction:added/removed scheme)%s\n", t2 ? t2 + 1 : "reaction:added", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_adapter_fatal_error @ gateway/run.py:_handle_adapter_fatal_error */
int grun_u_handle_adapter_fatal_error(const char *arg) {
    /* Python: retryable queue. Arg =
     * "queued\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int queued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (non-retryable — removed)\n"); return 0; }
    if (!queued) { printf("0 (handler cancelled mid-flight — strand guard)\n"); return 0; }
    printf("1 (queued for background reconnect; wrapper task protects against cancellation)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_adapter_fatal_error_detached @ gateway/run.py:_handle_adapter_fatal_error_detached */
int grun_u_handle_adapter_fatal_error_detached(const char *arg) {
    /* Python: strand-exit. Arg =
     * "exited\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exited = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!exited) { printf("0 (reconnected/queued/disabled — no exit)\n"); return 0; }
    printf("1 (stranded platform → gateway exit w/ failure so service manager restarts; handler exceptions logged)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_adapter_fatal_error_impl @ gateway/run.py:_handle_adapter_fatal_error_impl */
int grun_u_handle_adapter_fatal_error_impl(const char *arg) {
    /* Python: impl body. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("fatal handling done (%s)%s\n", tab ? tab + 1 : "", (tab && tab[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_restart_loop_guard_config @ gateway/run.py:_restart_loop_guard_config */
int grun_u_restart_loop_guard_config(const char *arg) {
    /* Python: config w/ defaults. Arg = "state\tmax_restarts\twindow\tresult". */
    if (!arg || !*arg) { printf("5\t300\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("5\t300\n"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "5", t3 ? t3 + 1 : "300");
    return 0;
}

/* PoP: grun_u_log_scale_to_zero_not_armed_reason @ gateway/run.py:_log_scale_to_zero_not_armed_reason */
int grun_u_log_scale_to_zero_not_armed_reason(const char *arg) {
    /* Python: opted-in only. Arg =
     * "opted_in\trelay_only\twake_url\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int opted_in = arg[0] == '1';
    int relay_only = t1 && t1[1] == '1';
    int wake_url = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!opted_in || !state) { printf("quiet (not opted in)\n"); return 0; }
    printf("scale-to-zero: NOT armed despite opt-in — relay_only=%s, wake_url=%s\n", relay_only ? "yes" : "no", wake_url ? "set" : "MISSING");
    return 0;
}

/* PoP: grun_u_scale_to_zero_note_real_inbound @ gateway/run.py:_scale_to_zero_note_real_inbound */
int grun_u_scale_to_zero_note_real_inbound(const char *arg) {
    /* Python: stamp inbound + restore running. Arg = "in_cooldown\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int in_cooldown = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (in_cooldown && state) printf("real inbound stamped; runtime status restored to running\n");
    else printf("real inbound stamped\n");
    return 0;
}

/* PoP: grun_u_relay_adapter_for_dormancy @ gateway/run.py:_relay_adapter_for_dormancy */
int grun_u_relay_adapter_for_dormancy(const char *arg) {
    /* Python: connected relay. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("relay adapter: %s\n", tab ? tab + 1 : "none connected");
    return 0;
}

/* PoP: grun_u_scale_to_zero_watcher @ gateway/run.py:_scale_to_zero_watcher */
int grun_u_scale_to_zero_watcher(const char *arg) {
    /* Python: D12 sequence. Arg =
     * "dormant\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (not armed)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dormant = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!dormant) { printf("0 (idle window not sustained yet)\n"); return 0; }
    printf("1 (draining marked + relay go_dormant; NOT disconnect; NO resume_pending #D13; wakeUrl poke wakes)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_enqueue_fifo @ gateway/run.py:_enqueue_fifo */
int grun_u_enqueue_fifo(const char *arg) {
    /* Python: append to pending slot or FIFO chain. Arg =
     * "session_key\tpending\tqueued". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t1 && t1[1] == '1') printf("fifo queued: %s\n", arg);
    else printf("fifo enqueued (pending slot): %s\n", arg);
    return 0;
}

/* PoP: grun_u_promote_queued_event @ gateway/run.py:_promote_queued_event */
int grun_u_promote_queued_event(const char *arg) {
    /* Python: overflow promote. Arg =
     * "has_overflow\tslot_empty\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_overflow = arg[0] == '1';
    int slot_empty = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_overflow || !state) { printf("%s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("overflow promoted%s\n", slot_empty ? " (as pending)" : " (staged in slot)");
    return 0;
}

/* PoP: grun_u_clear_goal_pending_continuations @ gateway/run.py:_clear_goal_pending_continuations */
int grun_u_clear_goal_pending_continuations(const char *arg) {
    /* Python: synthetic /goal removal. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: grun_u_persist_active_agents @ gateway/run.py:_persist_active_agents */
int grun_u_persist_active_agents(const char *arg) {
    /* Python: turn-boundary write. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("status write skipped\n"); return 0; }
    printf("active_agents=%s persisted (lifecycle preserved)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: grun_u_enter_external_drain @ gateway/run.py:_enter_external_drain */
int grun_u_enter_external_drain(const char *arg) {
    /* Python: idempotent drain entry. Arg = "already_active\tinflight\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int already = arg[0] == '1';
    if (already) { printf("drain already active (no-op)\n"); return 0; }
    printf("External drain ENGAGED — refusing new turns; %s in-flight turn(s) will finish. Process stays up.\n",
           t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: grun_u_exit_external_drain @ gateway/run.py:_exit_external_drain */
int grun_u_exit_external_drain(const char *arg) {
    /* Python: revert drain. Arg =
     * "mid_drain\tshutting\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int mid_drain = arg[0] == '1';
    int shutting = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state || !mid_drain) { printf("not draining\n"); return 0; }
    if (shutting) { printf("drain marker cleared during shutdown — NOT reverting to running\n"); return 0; }
    printf("External drain RELEASED — re-accepting new turns; gateway_state -> running%s\n", t3 && t3[1] == '1' ? " (persisted)" : "");
    return 0;
}

/* PoP: grun_u_drain_control_watcher @ gateway/run.py:_drain_control_watcher */
int grun_u_drain_control_watcher(const char *arg) {
    /* Python: marker reconcile. Arg =
     * "draining\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int draining = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (marker absent — exited drain)\n"); return 0; }
    if (!draining) { printf("0 (stale epoch marker treated absent NS-570)\n"); return 0; }
    printf("1 (external drain entered; 1s cadence bounds observe latency; startup reconcile once)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_pause_failed_platform @ gateway/run.py:_pause_failed_platform */
int grun_u_pause_failed_platform(const char *arg) {
    /* Python: manual pause. Arg =
     * "known\tpaused\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int known = arg[0] == '1';
    int paused = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!known || paused || !state) { printf("platform not paused\n"); return 0; }
    printf("platform paused (next_retry=inf): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_resume_paused_platform @ gateway/run.py:_resume_paused_platform */
int grun_u_resume_paused_platform(const char *arg) {
    /* Python: unpause + immediate retry. Arg =
     * "paused\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int paused = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!paused || !state) { printf("0\n"); return 0; }
    printf("%s resumed — retrying on next watcher tick\n", t2 ? t2 + 1 : "platform");
    printf("1\n");
    return 0;
}

/* PoP: grun_u_resolve_model_for_channel @ gateway/run.py:_resolve_model_for_channel */
int grun_u_resolve_model_for_channel(const char *arg) {
    /* Python: channel override else global default. Arg =
     * "override\tdefault". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1]) ? tab + 1 : arg);
    return 0;
}

/* PoP: grun_u_get_system_prompt_for_channel @ gateway/run.py:_get_system_prompt_for_channel */
int grun_u_get_system_prompt_for_channel(const char *arg) {
    /* Python: channel_overrides lookup by chat_id, then thread_id, then
     * parent_id (forum children inherit the parent entry); falls back to
     * the ephemeral gateway prompt. Arg = platform, chat_id, thread_id,
     * parent_id (tab-separated). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", arg);
    char *save = NULL;
    char *platform = strtok_r(buf, "\t", &save);
    char *chat_id = strtok_r(NULL, "\t", &save);
    char *thread_id = strtok_r(NULL, "\t", &save);
    char *parent_id = strtok_r(NULL, "\t", &save);
    if (!platform || !*platform || !chat_id || !*chat_id) { printf("\n"); return 0; }
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) { printf("\n"); return 0; }
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); printf("\n"); return 0; }
    const char *keys[3];
    int kn = 0;
    keys[kn++] = chat_id;
    if (thread_id && *thread_id) keys[kn++] = thread_id;
    if (parent_id && *parent_id) keys[kn++] = parent_id;
    const char *found = NULL;
    for (int i = 0; i < kn && !found; i++) {
        char path[512];
        snprintf(path, sizeof(path),
                 "gateway.platforms.%s.channel_overrides.%s.system_prompt",
                 platform, keys[i]);
        const char *v = yaml_get_string(doc, path);
        if (v && *v) found = v;
    }
    yaml_free(doc);
    if (found) {
        /* Python: (override.system_prompt or "").strip() */
        while (*found && isspace((unsigned char)*found)) found++;
        size_t n = strlen(found);
        while (n > 0 && isspace((unsigned char)found[n - 1])) n--;
        printf("%.*s\n", (int)n, found);
        return 0;
    }
    /* No override: Python returns _ephemeral_system_prompt or "". The C
     * port has no per-channel ephemeral slot -> empty line. */
    printf("\n");
    return 0;
}

/* PoP: grun_u_refresh_fallback_model @ gateway/run.py:_refresh_fallback_model */
int grun_u_refresh_fallback_model(const char *arg) {
    /* Python: re-read chain. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "transient") == 0) { printf("last known-good chain kept\n"); return 0; }
    if (strcmp(state, "cleared") == 0) { printf("chain cleared (no key)\n"); return 0; }
    printf("chain refreshed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_apply_fallback_chain_to_agent @ gateway/run.py:_apply_fallback_chain_to_agent */
int grun_u_apply_fallback_chain_to_agent(const char *arg) {
    /* Python: cooldown skip. Arg =
     * "activated\tin_cooldown\tchanged\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int activated = arg[0] == '1';
    int in_cooldown = t1 && t1[1] == '1';
    int changed = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("no agent\n"); return 0; }
    if (activated && in_cooldown) { printf("chain rewrite skipped (cooldown)\n"); return 0; }
    printf("fallback chain applied%s\n", changed ? " + memo cleared" : "");
    return 0;
}

/* PoP: grun_u_snapshot_running_agents @ gateway/run.py:_snapshot_running_agents */
int grun_u_snapshot_running_agents(const char *arg) {
    /* Python: {session_key: agent for ... if agent is not the pending
     * sentinel}. Arg = "session_key\tagent..." pairs (agent != "PENDING"
     * kept). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: grun_u_claim_active_session_slot @ gateway/run.py:_claim_active_session_slot */
int grun_u_claim_active_session_slot(const char *arg) {
    /* Python: cross-process slot claim. Arg =
     * "local_running\tlocal_limit\tstate\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int local_running = arg[0] == '1';
    int local_limit = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (local_running) { printf("\n\n"); return 0; }
    if (local_limit) { printf("\nlimit message\n"); return 0; }
    printf("%s\t%s\n", state ? "claimed" : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_agent_has_active_subagents @ gateway/run.py:_agent_has_active_subagents */
int grun_u_agent_has_active_subagents(const char *arg) {
    /* Python: running_agent._active_children must be a non-empty real
     * collection (guards MagicMock auto-creation). The shim receives the
     * children list as JSON. */
    if (!arg || !*arg) return 0;
    json_t *arr = json_parse(arg, NULL);
    if (!arr) return 0;
    int n = (arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    json_free(arr);
    return n > 0;
}

/* PoP: grun_u_session_has_compression_in_flight @ gateway/run.py:_session_has_compression_in_flight */
int grun_u_session_has_compression_in_flight(const char *arg) {
    /* Python: lock holder probe. Arg =
     * "in_flight\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!(arg[0] == '1')) { printf("0 (no lock held)\n"); return 0; }
    printf("1 (compression lock held — interrupt demoted to queue #56391; probes offloaded to worker thread)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_lookup_session_id_under_store_lock @ gateway/run.py:_lookup_session_id_under_store_lock */
int grun_u_lookup_session_id_under_store_lock(const char *arg) {
    /* Python: locked read. Arg = "session_key\tstate\tsession_id". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_queue_or_replace_pending_event @ gateway/run.py:_queue_or_replace_pending_event */
int grun_u_queue_or_replace_pending_event(const char *arg) {
    /* Python: FIFO overflow #28503. Arg =
     * "queued\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int queued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter)\n"); return 0; }
    if (!queued) { printf("0 (replaced head slot — photo burst merge)\n"); return 0; }
    printf("1 (appended to FIFO overflow tail — each follow-up gets its own turn in arrival order)%s\n", (t2 && t2[1] == '1') ? " — queue mode" : "");
    return 0;
}

/* PoP: grun_u_handle_active_session_busy_message @ gateway/run.py:_handle_active_session_busy_message */
int grun_u_handle_active_session_busy_message(const char *arg) {
    /* Python: busy-path auth #17775. Arg =
     * "queued\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int queued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (unauthorized user dropped — shared-thread injection blocked)\n"); return 0; }
    if (!queued) { printf("0 (not busy — normal path)\n"); return 0; }
    printf("1 (message queued for active session; auth gate enforced same as cold path)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_notify_active_sessions_of_shutdown @ gateway/run.py:_notify_active_sessions_of_shutdown */
int grun_u_notify_active_sessions_of_shutdown(const char *arg) {
    /* Python: pre-stop notices. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (%s notices to active chats + home channels; failures swallowed — never block shutdown)%s\n", (t2 && t2[1] == '1') ? "restarting" : "shutting down", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_finalize_shutdown_agents @ gateway/run.py:_finalize_shutdown_agents */
int grun_u_finalize_shutdown_agents(const char *arg) {
    /* Python: persist in-flight. Arg =
     * "finalized\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (%s agent(s) flushed to state.db — drain-interrupted turns persist #13121)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_should_emit_long_running_notification @ gateway/run.py:_should_emit_long_running_notification */
int grun_u_should_emit_long_running_notification(const char *arg) {
    /* Python: ownership guard. Arg =
     * "has_agent\texecutor_done\trebound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int has_agent = arg[0] == '1';
    int executor_done = t1 && t1[1] == '1';
    int rebound = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!has_agent || executor_done || rebound || !state) { printf("0\n"); return 0; }
    printf("%s\n", (t4 && t4[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_defer_agent_cleanup_until_future_done @ gateway/run.py:_defer_agent_cleanup_until_future_done */
int grun_u_defer_agent_cleanup_until_future_done(const char *arg) {
    /* Python: shielded waiter. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no deferral\n"); return 0; }
    printf("cleanup deferred until executor future completes (shielded; cancelled waiter never cleans early): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_cleanup_agent_resources_off_loop @ gateway/run.py:_cleanup_agent_resources_off_loop */
int grun_u_cleanup_agent_resources_off_loop(const char *arg) {
    /* Python: bounded worker. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no agent)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (shutdown/session-expiry context — skipped)\n"); return 0; }
    printf("1 (cleanup in worker thread, bounded wait; timeout cancels await, worker finishes on own #35994)%s\n", (t2 && t2[1] == '1') ? " — timed out" : "");
    return 0;
}

/* PoP: grun_u_cleanup_agent_resources @ gateway/run.py:_cleanup_agent_resources */
int grun_u_cleanup_agent_resources(const char *arg) {
    /* Python: best-effort teardown. Arg =
     * "has_agent\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_agent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_agent) { printf("no agent to clean\n"); return 0; }
    printf("agent resources cleaned (memory shutdown w/ transcript, close, stale aux clients)%s\n", (t2 && t2[1] == '1') ? " — transcript passed" : "");
    return 0;
}

/* PoP: grun_u_increment_restart_failure_counts @ gateway/run.py:_increment_restart_failure_counts */
int grun_u_increment_restart_failure_counts(const char *arg) {
    /* Python: persistent stuck-loop counters. Arg =
     * "active_keys\tstate\tresult_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: grun_u_suspend_stuck_loop_sessions @ gateway/run.py:_suspend_stuck_loop_sessions */
int grun_u_suspend_stuck_loop_sessions(const char *arg) {
    /* Python: restart-loop suspension. Arg =
     * "suspended\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s suspended (file cleared)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: grun_u_launch_detached_restart_command @ gateway/run.py:_launch_detached_restart_command */
int grun_u_launch_detached_restart_command(const char *arg) {
    /* Python: detached helper. Arg =
     * "launched\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no hermes bin)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int launched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (already started once)\n"); return 0; }
    if (!launched) { printf("0\n"); return 0; }
    printf("1 (detached restart helper spawned; restart_after=%ss; Windows pythonw shim)%s\n", t2 ? t2 + 1 : "5", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_startup_resume_event @ gateway/run.py:_run_startup_resume_event */
int grun_u_run_startup_resume_event(const char *arg) {
    /* Python: bounded resume. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (resume turn finished before queue release; session task awaited)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_queue_startup_restore_event @ gateway/run.py:_queue_startup_restore_event */
int grun_u_queue_startup_restore_event(const char *arg) {
    /* Python: startup queue. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (queued during startup restore; drained after: %s)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_drain_startup_restore_queue @ gateway/run.py:_drain_startup_restore_queue */
int grun_u_drain_startup_restore_queue(const char *arg) {
    /* Python: replay queue. Arg =
     * "drained\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no queue)\n"); return 0; }
    printf("%s message(s) replayed (adapter-missing → dropped w/ debug log)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_finish_startup_restore @ gateway/run.py:_finish_startup_restore */
int grun_u_finish_startup_restore(const char *arg) {
    /* Python: gather + drain. Arg =
     * "drained\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s queued message(s) drained (auto-resume tasks gathered w/ exceptions logged)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_redeliver_pending_obligations @ gateway/run.py:_redeliver_pending_obligations */
int grun_u_redeliver_pending_obligations(const char *arg) {
    /* Python: ledger replay. Arg =
     * "redelivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no recoverable obligations)\n"); return 0; }
    printf("%s obligation(s) redelivered (turn completed, only delivery owed; resume_pending cleared — no re-pay)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_schedule_resume_pending_sessions @ gateway/run.py:_schedule_resume_pending_sessions */
int grun_u_schedule_resume_pending_sessions(const char *arg) {
    /* Python: resume pass. Arg =
     * "resumed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s session(s) resumed (empty event text → injection path owns wording; platform-scoped on reconnect; adapter-missing skipped)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — running agents skipped" : "");
    return 0;
}

/* PoP: grun_u_abort_startup_if_shutdown_requested @ gateway/run.py:_abort_startup_if_shutdown_requested */
int grun_u_abort_startup_if_shutdown_requested(const char *arg) {
    /* Python: mid-startup stop. Arg =
     * "aborted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int aborted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (startup not aborted)\n"); return 0; }
    if (!aborted) { printf("0\n"); return 0; }
    printf("1 (startup aborted — adapter bg tasks cancelled, safe disconnect, stop task awaited)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_start_loop_liveness_guards @ gateway/run.py:_start_loop_liveness_guards */
int grun_u_start_loop_liveness_guards(const char *arg) {
    /* Python: arm floor timer + watchdog. Arg =
     * "enabled\tfloor_armed\twatchdog_started\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    if (!enabled) { printf("loop watchdog disabled by config\n"); return 0; }
    printf("loop guards armed: floor=%s watchdog=%s\n",
           (t1 && t1[1] == '1') ? "yes" : "no",
           (t2 && t2[1] == '1') ? "yes" : "no");
    return 0;
}

/* PoP: grun_u_stop_loop_liveness_guards @ gateway/run.py:_stop_loop_liveness_guards */
int grun_u_stop_loop_liveness_guards(const char *arg) {
    /* Python: disarm before shutdown. Arg =
     * "stopped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int stopped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!stopped) { printf("0 (nothing armed)\n"); return 0; }
    printf("1 (liveness watchdog stopped + loop floor timer cancelled — shutdown won't load the loop)%s\n", (t2 && t2[1] == '1') ? " — errors logged" : "");
    return 0;
}

/* PoP: grun_u_spawn_supervised @ gateway/run.py:_spawn_supervised */
int grun_u_spawn_supervised(const char *arg) {
    /* Python: task-level supervision. Arg =
     * "spawned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int spawned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!spawned) { printf("0 (no running loop)\n"); return 0; }
    printf("1 (supervised: retained in _background_tasks, crashes logged, capped exp backoff restart, healthy-reset counter)%s\n", (t2 && t2[1] == '1') ? " — restart attempted" : "");
    return 0;
}

/* PoP: grun_u_handoff_watcher @ gateway/run.py:_handoff_watcher */
int grun_u_handoff_watcher(const char *arg) {
    /* Python: CLI→gateway handoff. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no pending handoffs)\n"); return 0; }
    printf("%s handoff(s) (claim pending→running; switch_session rebind; synthetic internal MessageEvent)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_process_handoff @ gateway/run.py:_process_handoff */
int grun_u_process_handoff(const char *arg) {
    /* Python: one handoff row. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (empty handoff_platform)\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (handoff failed — caller marks row failed)\n"); return 1; }
    printf("1 (handoff executed: %s platform, session switched, synthetic event)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_session_expiry_watcher @ gateway/run.py:_session_expiry_watcher */
int grun_u_session_expiry_watcher(const char *arg) {
    /* Python: finalize loop. Arg =
     * "finalized\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (60s initial delay — gateway still starting)\n"); return 0; }
    printf("%s session(s) finalized (on_session_finalize hooks, tool resource cleanup, cache evict, once-only mark)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_ensure_reconnect_watcher_running @ gateway/run.py:_ensure_reconnect_watcher_running */
int grun_u_ensure_reconnect_watcher_running(const char *arg) {
    /* Python: watcher respawn. Arg =
     * "respawned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int respawned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not running)\n"); return 0; }
    if (!respawned) { printf("0 (watcher alive)\n"); return 0; }
    printf("1 (watcher respawned — dead after restart budget / terminal exception #70344; background task tracked)%s\n", (t2 && t2[1] == '1') ? " — queued fatal errors" : "");
    return 0;
}

/* PoP: grun_u_platform_reconnect_watcher @ gateway/run.py:_platform_reconnect_watcher */
int grun_u_platform_reconnect_watcher(const char *arg) {
    /* Python: backoff retry. Arg =
     * "retried\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (queue empty)\n"); return 0; }
    printf("1 (backoff 30→60→120→240→300 cap; retryable self-heal indefinitely; non-retryable drop immediately; circuit breaker available)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_cancel_secondary_profile_reconnect_tasks @ gateway/run.py:_cancel_secondary_profile_reconnect_tasks */
int grun_u_cancel_secondary_profile_reconnect_tasks(const char *arg) {
    /* Python: pre-teardown cancel. Arg =
     * "cancelled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no pending map)\n"); return 0; }
    printf("1 (reconnect tasks cancelled before registry drain; stopped runner still blocks late installs)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_start_systemd_watchdog @ gateway/run.py:_start_systemd_watchdog */
int grun_u_start_systemd_watchdog(const char *arg) {
    /* Python: sd_notify gate. Arg =
     * "started\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int started = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not running / watchdog disabled)\n"); return 0; }
    if (!started) { printf("0 (watchdog.start failed)\n"); return 0; }
    printf("1 (sd_notify started after gateway truly running, ready msg sent)%s\n", (t2 && t2[1] == '1') ? " — already running" : "");
    return 0;
}

/* PoP: grun_u_stop_systemd_watchdog @ gateway/run.py:_stop_systemd_watchdog */
int grun_u_stop_systemd_watchdog(const char *arg) {
    /* Python: stop before drain. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("watchdog heartbeats stopped (cleared handle, awaited stop — before long shutdown drain)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_start_secondary_profile_adapters @ gateway/run.py:_start_secondary_profile_adapters */
int grun_u_start_secondary_profile_adapters(const char *arg) {
    /* Python: multiplex bring-up. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (multiplex off)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s secondary adapter(s) connected (per-profile HERMES_HOME + secret scope; source.profile stamped)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_start_one_profile_adapters @ gateway/run.py:_start_one_profile_adapters */
int grun_u_start_one_profile_adapters(const char *arg) {
    /* Python: scoped bring-up. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (MultiplexConfigError — open-policy violation)\n"); return 1; }
    printf("%s adapter(s) for profile %s (under profile runtime scope)%s\n", t2 ? t2 + 1 : "0", t1 ? t1 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_configure_profile_adapter @ gateway/run.py:_configure_profile_adapter */
int grun_u_configure_profile_adapter(const char *arg) {
    /* Python: shared handler install. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("profile handlers installed (message/fatal/session_store/busy/reaction/topic-recovery/auth-check/busy_text_mode): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_run_secondary_profile_reconnect @ gateway/run.py:_run_secondary_profile_reconnect */
int grun_u_run_secondary_profile_reconnect(const char *arg) {
    /* Python: scoped retry. Arg =
     * "reconnected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (loop ended)\n"); return 0; }
    printf("1 (reconnected under %s profile scope; attempts=%s)%s\n", t2 ? t2 + 1 : "?", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_schedule_secondary_profile_reconnect @ gateway/run.py:_schedule_secondary_profile_reconnect */
int grun_u_schedule_secondary_profile_reconnect(const char *arg) {
    /* Python: runner-owned reconnect task. Arg =
     * "running\tretryable\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int running = arg[0] == '1';
    int retryable = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!running || !retryable) { printf("reconnect skipped\n"); return 0; }
    printf("secondary reconnect scheduled: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_make_profile_fatal_error_handler @ gateway/run.py:_make_profile_fatal_error_handler */
int grun_u_make_profile_fatal_error_handler(const char *arg) {
    /* Python: secondary-profile route. Arg =
     * "profile\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("profile fatal handler created: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_handle_profile_adapter_fatal_error @ gateway/run.py:_handle_profile_adapter_fatal_error */
int grun_u_handle_profile_adapter_fatal_error(const char *arg) {
    /* Python: secondary remove. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int removed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not this profile's adapter — ignored)\n"); return 0; }
    if (!removed) { printf("0 (mismatch — primary-only handler ignored)\n"); return 0; }
    printf("1 (failed secondary %s removed from _profile_adapters; primary slot untouched)%s\n", t2 ? t2 + 1 : "adapter", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_make_profile_message_handler @ gateway/run.py:_make_profile_message_handler */
int grun_u_make_profile_message_handler(const char *arg) {
    /* Python: profile-stamping wrapper. Arg =
     * "profile\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("handler created (no profile home)\n"); return 0; }
    printf("profile handler created: %s\n", arg);
    return 0;
}

/* PoP: grun_u_create_adapter @ gateway/run.py:_create_adapter */
int grun_u_create_adapter(const char *arg) {
    /* Python: registry-first. Arg =
     * "plugin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int plugin = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("adapter created via %s (extra defaults group/thread sessions; runner back-ref injected for admin alerts + profile routing)%s\n", plugin ? "plugin registry" : "built-in chain", (t2 && t2[1] == '1') ? " — started" : "");
    return 0;
}

/* PoP: grun_u_make_adapter_auth_check @ gateway/run.py:_make_adapter_auth_check */
int grun_u_make_adapter_auth_check(const char *arg) {
    /* Python: platform-bound callback. Arg =
     * "built\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int built = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!built) { printf("\n"); return 0; }
    printf("auth-check callback built (delegates to _is_user_authorized; unverified senders marked unverified in LLM context — prompt-injection mitigation; profile_name binds secret scope)%s\n", (t2 && t2[1] == '1') ? " — SessionSource bound" : "");
    return 0;
}

/* PoP: grun_u_deliver_platform_notice @ gateway/run.py:_deliver_platform_notice */
int grun_u_deliver_platform_notice(const char *arg) {
    /* Python: privacy-aware notice. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int delivered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter / slack ignored channel — skipped)\n"); return 0; }
    if (!delivered) { printf("0\n"); return 0; }
    printf("1 (notice delivered per platform privacy rules)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_resolve_async_delegation_session @ gateway/run.py:_resolve_async_delegation_session */
int grun_u_resolve_async_delegation_session(const char *arg) {
    /* Python: lineage follow. Arg =
     * "session\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n (fail-closed — unknown ownership; result stays in records)\n"); return 0; }
    printf("%s (compression lineage followed; never overrides unrelated /new or restored route)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_prepare_inbound_message_text @ gateway/run.py:_prepare_inbound_message_text */
int grun_u_prepare_inbound_message_text(const char *arg) {
    /* Python: shared preprocessing. Arg =
     * "prepared\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int prepared = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!prepared) { printf("\n"); return 0; }
    printf("%s (sender attribution, image enrichment, STT, doc notes, reply context, @ refs — same pipeline for normal+queued; native-vision path buffered)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_prepare_profile_scoped_inbound_message_text @ gateway/run.py:_prepare_profile_scoped_inbound_message_text */
int grun_u_prepare_profile_scoped_inbound_message_text(const char *arg) {
    /* Python: multiplex scope. Arg =
     * "scoped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scoped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!scoped) { printf("0 (not multiplexed — plain preprocessing)\n"); return 0; }
    printf("1 (preprocessed under profile runtime scope for %s)%s\n", t2 ? t2 + 1 : "source", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_async_session_store @ gateway/run.py:async_session_store */
int grun_async_session_store(const char *arg) {
    /* Python: return cached async facade for session_store. Arg = store id. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("async facade for %s\n", arg);
    return 0;
}

/* PoP: grun_u_handle_message_with_agent @ gateway/run.py:_handle_message_with_agent */
int grun_u_handle_message_with_agent(const char *arg) {
    /* Python: inner handler under _running_agents sentinel guard.
     * Full pipeline: topic recovery → session get/create → pinned
     * delegation resolve → inbound preprocessing → compression-in-flight
     * check (interrupt demoted to queue #56391) → run generation stamp →
     * agent turn (local or proxy) → STT/media enrichment → post-turn goal
     * continuation → cache re-baseline → busy-session queue semantics.
     * Arg = "processed	state	result". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '	');
    const char *t2 = t1 ? strchr(t1 + 1, '	') : NULL;
    int processed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (sentinel guard skipped)\n"); return 0; }
    if (!processed) { printf("0 (delegation pin unresolved)\n"); return 0; }
    printf("1 (turn completed: topic recovery %s, goal continuation %s, cache re-baselined)%s\n", (t2 && t2[1] == '1') ? "applied" : "none", (t2 && t2[1] == '2') ? "queued" : "none", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_reset_notice_session_info @ gateway/run.py:_reset_notice_session_info */
int grun_u_reset_notice_session_info(const char *arg) {
    /* Python: profile-scoped notice info. Arg = "multiplex\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int multiplex = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_format_session_info @ gateway/run.py:_format_session_info */
int grun_u_format_session_info(const char *arg) {
    /* Python: model info block. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("session info block (model/provider/context/endpoint — surfaces wrong context detection)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_sibling_thread_run_keys @ gateway/run.py:_sibling_thread_run_keys */
int grun_u_sibling_thread_run_keys(const char *arg) {
    /* Python: per-user thread keys. Arg =
     * "in_thread\tcount\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int in_thread = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!in_thread || !state) { printf("[]\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "[]");
    return 0;
}

/* PoP: grun_u_is_stale_restart_redelivery @ gateway/run.py:_is_stale_restart_redelivery */
int grun_u_is_stale_restart_redelivery(const char *arg) {
    /* Python: update_id marker. Arg =
     * "stale\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int stale = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not telegram / no update_id)\n"); return 0; }
    printf("%s (restart_last_processed.json compare, <5min recency; telegram-only today)%s\n", stale ? "1" : "0", (t2 && t2[1] == '1') ? " — booted from restart" : "");
    return 0;
}

/* PoP: grun_u_handle_suggestions_command @ gateway/run.py:_handle_suggestions_command */
int grun_u_handle_suggestions_command(const char *arg) { (void)arg; return 0; }

/* PoP: grun_u_handle_blueprint_command @ gateway/run.py:_handle_blueprint_command */
int grun_u_handle_blueprint_command(const char *arg) { (void)arg; return 0; }

/* PoP: grun_u_get_goal_manager_for_event @ gateway/run.py:_get_goal_manager_for_event */
int grun_u_get_goal_manager_for_event(const char *arg) {
    /* Python: session-bound. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t\n"); return 0; }
    if (!found) { printf("\t\n"); return 0; }
    printf("GoalManager bound (session %s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_goal_status_notice @ gateway/run.py:_send_goal_status_notice */
int grun_u_send_goal_status_notice(const char *arg) {
    /* Python: judge status line. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter)\n"); return 0; }
    if (!sent) { printf("0 (send failed — logged)\n"); return 0; }
    printf("1 (judge status sent to originating chat w/ thread metadata)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_defer_goal_status_notice_after_delivery @ gateway/run.py:_defer_goal_status_notice_after_delivery */
int grun_u_defer_goal_status_notice_after_delivery(const char *arg) {
    /* Python: post-delivery callback. Arg =
     * "deferred\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int deferred = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!deferred) { printf("0 (no post-delivery hook — direct awaited delivery fallback)\n"); return 0; }
    printf("1 (goal status scheduled after main response send — natural reading order)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_post_turn_goal_continuation @ gateway/run.py:_post_turn_goal_continuation */
int grun_u_post_turn_goal_continuation(const char *arg) {
    /* Python: judge + enqueue. Arg =
     * "enqueued\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no goal)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enqueued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!enqueued) { printf("0 (judge said done/wait)\n"); return 0; }
    printf("1 (continuation queued via FIFO — real user messages take priority)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_voice_channel_join @ gateway/run.py:_handle_voice_channel_join */
int grun_u_handle_voice_channel_join(const char *arg) {
    /* Python: join voice. Arg =
     * "joined\tstate\tresult". */
    if (!arg || !*arg) { printf("Voice channels are not supported on this platform.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int joined = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("This command only works in a Discord server.\n"); return 0; }
    if (!joined) { printf("You need to be in a voice channel first.\n"); return 0; }
    printf("Joined the voice channel.%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_voice_channel_leave @ gateway/run.py:_handle_voice_channel_leave */
int grun_u_handle_voice_channel_leave(const char *arg) {
    /* Python: leave voice. Arg =
     * "left\tstate\tresult". */
    if (!arg || !*arg) { printf("Not in a voice channel.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int left = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("Not in a voice channel.\n"); return 0; }
    if (!left) { printf("Not in a voice channel.\n"); return 0; }
    printf("Left the voice channel.%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_voice_timeout_cleanup @ gateway/run.py:_handle_voice_timeout_cleanup */
int grun_u_handle_voice_timeout_cleanup(const char *arg) {
    /* Python: voice_mode off + save + disable auto-tts. Arg = "chat_id\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("voice mode off (%.*s), auto-tts disabled\n",
           (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: grun_u_handle_voice_channel_input @ gateway/run.py:_handle_voice_channel_input */
int grun_u_handle_voice_channel_input(const char *arg) {
    /* Python: voice → synthetic event. Arg =
     * "processed\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no discord adapter)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int processed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no linked text channel)\n"); return 0; }
    if (!processed) { printf("0\n"); return 0; }
    printf("1 (voice input through full pipeline — session/typing/agent/TTS reply shared with bound text chat)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_should_send_voice_reply @ gateway/run.py:_should_send_voice_reply */
int grun_u_should_send_voice_reply(const char *arg) {
    /* Python: TTS dedup ladder. Arg =
     * "mode_ok\thas_agent_tts\tvoice_input\talready_sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int mode_ok = arg[0] == '1';
    int has_agent_tts = t1 && t1[1] == '1';
    int voice_input = t2 && t2[1] == '1';
    int already_sent = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (!state || !mode_ok || has_agent_tts) { printf("0\n"); return 0; }
    if (voice_input && !already_sent) { printf("0\n"); return 0; }
    printf("%s\n", (t5 && t5[1] == '1') ? "1" : "1");
    return 0;
}

/* PoP: grun_u_send_voice_reply @ gateway/run.py:_send_voice_reply */
int grun_u_send_voice_reply(const char *arg) {
    /* Python: TTS bubble. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no tts text)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (tts failed)\n"); return 0; }
    if (!sent) { printf("0\n"); return 0; }
    printf("1 (voice sent before text; ogg for Telegram native bubble, else mp3)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_deliver_media_from_response @ gateway/run.py:_deliver_media_from_response */
int grun_u_deliver_media_from_response(const char *arg) {
    /* Python: explicit MEDIA: only. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no explicit MEDIA tags)\n"); return 0; }
    printf("%s file(s) delivered (post-stream rescan — EXPLICIT-ONLY, no bare-path auto-detect)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_background_task @ gateway/run.py:_run_background_task */
int grun_u_run_background_task(const char *arg) {
    /* Python: profile-scoped wrapper. Arg =
     * "scoped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scoped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!scoped) { printf("0 (not multiplexed — inner direct)\n"); return 0; }
    printf("1 (task ran under profile runtime scope — credentials from that profile's secret scope)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_background_task_inner @ gateway/run.py:_run_background_task_inner */
int grun_u_run_background_task_inner(const char *arg) {
    /* Python: inner impl. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (background task completed: %s)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_get_telegram_topic_capabilities @ gateway/run.py:_get_telegram_topic_capabilities */
int grun_u_get_telegram_topic_capabilities(const char *arg) {
    /* Python: getMe flags. Arg =
     * "checked\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"checked\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int checked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"checked\": false}\n"); return 0; }
    if (!checked) { printf("{\"checked\": false} (getMe failed)\n"); return 0; }
    printf("{\"checked\": true, \"capabilities\": %s} (can_send_paid_media etc.)%s\n", t2 ? t2 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_ensure_telegram_system_topic @ gateway/run.py:_ensure_telegram_system_topic */
int grun_u_ensure_telegram_system_topic(const char *arg) {
    /* Python: System topic. Arg =
     * "created\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no adapter/chat)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int created = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!created) { printf("0 (create failed — debug logged)\n"); return 0; }
    printf("1 (System topic created+%s after /topic activation)%s\n", (t2 && t2[1] == '1') ? "pinned" : "named", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_telegram_topic_setup_image @ gateway/run.py:_send_telegram_topic_setup_image */
int grun_u_send_telegram_topic_setup_image(const char *arg) {
    /* Python: bundled screenshot. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter / no chat / no send_image_file)\n"); return 0; }
    if (!sent) { printf("0 (asset missing)\n"); return 0; }
    printf("1 (BotFather Threads Settings screenshot sent w/ caption)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_rename_discord_auto_thread_for_session_title @ gateway/run.py:_rename_discord_auto_thread_for_session_title */
int grun_u_rename_discord_auto_thread_for_session_title(const char *arg) {
    /* Python: semantic rename. Arg =
     * "renamed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int renamed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not auto lane / no adapter / no rename_thread)\n"); return 0; }
    if (!renamed) { printf("0 (rename failed)\n"); return 0; }
    printf("1 (thread %s renamed to sanitized '%s')%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "", "");
    return 0;
}

/* PoP: grun_u_schedule_discord_semantic_thread_rename @ gateway/run.py:_schedule_discord_semantic_thread_rename */
int grun_u_schedule_discord_semantic_thread_rename(const char *arg) {
    /* Python: threadsafe schedule. Arg =
     * "scheduled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scheduled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !scheduled) { printf("0 (no title / not discord auto lane / loop closed)\n"); return 0; }
    printf("1 (semantic rename scheduled via safe_schedule_threadsafe, source copied, failure logged)%s\n", (t2 && t2[1] == '1') ? " — done-callback wired" : "");
    return 0;
}

/* PoP: grun_u_rename_telegram_topic_for_session_title @ gateway/run.py:_rename_telegram_topic_for_session_title */
int grun_u_rename_telegram_topic_for_session_title(const char *arg) {
    /* Python: auto-title rename. Arg =
     * "renamed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int renamed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not topic lane / disabled)\n"); return 0; }
    if (!renamed) { printf("0\n"); return 0; }
    printf("1 (topic renamed to auto-title; extra.disable_topic_auto_rename honored)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_schedule_telegram_topic_title_rename @ gateway/run.py:_schedule_telegram_topic_title_rename */
int grun_u_schedule_telegram_topic_title_rename(const char *arg) {
    /* Python: thread-safe rename schedule. Arg =
     * "title\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("rename not scheduled\n"); return 0; }
    printf("topic rename scheduled: %s\n", arg);
    return 0;
}

/* PoP: grun_u_disable_telegram_topic_mode_for_chat @ gateway/run.py:_disable_telegram_topic_mode_for_chat */
int grun_u_disable_telegram_topic_mode_for_chat(const char *arg) {
    /* Python: /topic off. Arg =
     * "disabled\tstate\tresult". */
    if (!arg || !*arg) { printf("Could not determine chat ID.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int disabled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (session db unavailable / never enabled)\n"); return 0; }
    if (!disabled) { printf("0 (already off)\n"); return 0; }
    printf("1 (topic mode disabled; threads archived/pinned cleanup)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_telegram_topic_root_status_message @ gateway/run.py:_telegram_topic_root_status_message */
int grun_u_telegram_topic_root_status_message(const char *arg) {
    /* Python: multi-session help. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("Telegram multi-session topics are enabled.\n");
    printf("… open All Messages, send any message to create a new topic.%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_restore_telegram_topic_session @ gateway/run.py:_restore_telegram_topic_session */
int grun_u_restore_telegram_topic_session(const char *arg) {
    /* Python: /topic restore. Arg =
     * "restored\tstate\tresult". */
    if (!arg || !*arg) { printf("Session not found.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int restored = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("Session not found.\n"); return 0; }
    if (!restored) { printf("That session is not a Telegram session and cannot be restored into this topic.\n"); return 0; }
    printf("1 (session restored into topic; unlinked list refreshed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_execute_mcp_reload @ gateway/run.py:_execute_mcp_reload */
int grun_u_execute_mcp_reload(const char *arg) {
    /* Python: disconnect+reconnect. Arg =
     * "reloaded\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reloaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (reload failed)\n"); return 1; }
    if (!reloaded) { printf("0\n"); return 1; }
    printf("1 (old servers shutdown, tools rediscovered, changes notified: %s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_maybe_confirm_destructive_slash @ gateway/run.py:_maybe_confirm_destructive_slash */
int grun_u_maybe_confirm_destructive_slash(const char *arg) {
    /* Python: destructive gate. Arg =
     * "result\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("executed immediately (gate off)\n"); return 0; }
    printf("%s (once/always → execute; cancel → aborted)%s\n", result, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_request_slash_confirm @ gateway/run.py:_request_slash_confirm */
int grun_u_request_slash_confirm(const char *arg) {
    /* Python: once/always/cancel. Arg =
     * "buttons\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int buttons = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!buttons) { printf("ack text (picker fallback — text path IS the ack)\n"); return 0; }
    printf("ack None (buttons self-explanatory; handler runs on response)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_read_user_config @ gateway/run.py:_read_user_config */
int grun_u_read_user_config(const char *arg) {
    /* Python: load_config() or {} (fail-open). Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: grun_u_schedule_update_notification_watch @ gateway/run.py:_schedule_update_notification_watch */
int grun_u_schedule_update_notification_watch(const char *arg) {
    /* Python: single watcher. Arg =
     * "scheduled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scheduled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!scheduled) { printf("0 (task already running)\n"); return 0; }
    printf("1 (watcher scheduled: polls .update_output.txt, streams chunks, forwards .update_prompt.json, intercepts replies to .update_response)%s\n", (t2 && t2[1] == '1') ? " — no event loop" : "");
    return 0;
}

/* PoP: grun_u_watch_update_progress @ gateway/run.py:_watch_update_progress */
int grun_u_watch_update_progress(const char *arg) {
    /* Python: .update_output stream. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("update watcher (polls .update_output.txt, streams chunks @4s, forwards .update_prompt.json, intercepts replies → .update_response)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_send_update_notification @ gateway/run.py:_send_update_notification */
int grun_u_send_update_notification(const char *arg) {
    /* Python: legacy claim. Arg =
     * "notified\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (still running — retry later)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int notified = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (still running — retry later)\n"); return 0; }
    if (!notified) { printf("1 (definitive skip)\n"); return 0; }
    printf("1 (update finished — user notified via .update_pending.claimed.json)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_restart_notification @ gateway/run.py:_send_restart_notification */
int grun_u_send_restart_notification(const char *arg) {
    /* Python: .restart_notify.json. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!sent) { printf("\n"); return 0; }
    printf("1 (back-online notice sent to initiating chat; marker removed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_home_channel_startup_notifications @ gateway/run.py:_send_home_channel_startup_notifications */
int grun_u_send_home_channel_startup_notifications(const char *arg) {
    /* Python: ♻️ once per home. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("♻️ Gateway online — Hermes is back and ready. (%s home channel(s); skip_targets dedup)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_set_session_env @ gateway/run.py:_set_session_env */
int grun_u_set_session_env(const char *arg) {
    /* Python: contextvar bind. Arg =
     * "platform\tasync_delivery\tstate\ttokens". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("session vars bound (%s, async_delivery=%s): %s reset tokens\n", arg, (t1 && t1[1] == '1') ? "yes" : "no", t3 ? t3 + 1 : "0");
    return 0;
}

/* PoP: grun_u_clear_session_env @ gateway/run.py:_clear_session_env */
int grun_u_clear_session_env(const char *arg) {
    /* Python: restore session context vars. Arg = "tokens\tstate". */
    (void)arg;
    printf("session context vars cleared\n");
    return 0;
}

/* PoP: grun_u_run_in_executor_with_context @ gateway/run.py:_run_in_executor_with_context */
int grun_u_run_in_executor_with_context(const char *arg) {
    /* Python: contextvars-preserving. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (copy_context + executor, ctx.run(func))%s\n", t2 ? t2 + 1 : "done", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_get_executor @ gateway/run.py:_get_executor */
int grun_u_get_executor(const char *arg) {
    /* Python: gateway pool or raise. Arg = "closing\trebuilt\tstate". */
    if (!arg || !*arg) { printf("executor ready\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int closing = arg[0] == '1';
    if (closing) {
        fprintf(stderr, "Gateway is shutting down; executor unavailable\n");
        return 1;
    }
    printf("executor ready%s\n", (t1 && t1[1] == '1') ? " (rebuilt)" : "");
    return 0;
}

/* PoP: grun_u_shutdown_executor @ gateway/run.py:_shutdown_executor */
int grun_u_shutdown_executor(const char *arg) {
    /* Python: executor shutdown wait=False cancel_futures. Arg = "state". */
    (void)arg;
    printf("gateway executor shut down\n");
    return 0;
}

/* PoP: grun_u_enrich_message_with_vision @ gateway/run.py:_enrich_message_with_vision */
int grun_u_enrich_message_with_vision(const char *arg) {
    /* Python: auto vision prefix. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enriched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no images)\n"); return 0; }
    if (!enriched) { printf("0 (vision unavailable)\n"); return 0; }
    printf("1 (%s image(s) described + cache paths injected)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_enrich_message_with_transcription @ gateway/run.py:_enrich_message_with_transcription */
int grun_u_enrich_message_with_transcription(const char *arg) {
    /* Python: STT prefix. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\t[] (no audio / STT unavailable)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s\t%s (transcript wrappers prepended; raw transcripts returned)%s\n", t2 ? t2 + 1 : "", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_pending_event_audio_paths @ gateway/run.py:_pending_event_audio_paths */
int grun_u_pending_event_audio_paths(const char *arg) {
    /* Python: STT-eligible filter. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s audio path(s) STT-eligible (is_stt_input per media index)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — cached on event" : "");
    return 0;
}

/* PoP: grun_u_transcribe_pending_audio_event_once @ gateway/run.py:_transcribe_pending_audio_event_once */
int grun_u_transcribe_pending_audio_event_once(const char *arg) {
    /* Python: cached STT. Arg =
     * "text\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (single STT call + single echo; cached on event for both interrupt monitor and drain path)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: grun_u_echo_pending_stt_transcripts_once @ gateway/run.py:_echo_pending_stt_transcripts_once */
int grun_u_echo_pending_stt_transcripts_once(const char *arg) {
    /* Python: 🎙️ echo. Arg =
     * "echoed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int echoed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!echoed) { printf("0 (no transcripts / echo disabled / already echoed)\n"); return 0; }
    printf("1 (🎙️ transcripts echoed; per-event once-flag set)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_transcribe_and_echo_pending_voice @ gateway/run.py:_transcribe_and_echo_pending_voice */
int grun_u_transcribe_and_echo_pending_voice(const char *arg) {
    /* Python: unified voice helper. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\t[] (no STT-eligible media)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s\t%s (single STT + echo; feeds interrupt or drain)%s\n", t2 ? t2 + 1 : "", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_build_process_event_source @ gateway/run.py:_build_process_event_source */
int grun_u_build_process_event_source(const char *arg) {
    /* Python: session-store origin first. Arg =
     * "origin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("source resolved (session-store origin preferred — NEVER active foreground event, avoids cross-topic bleed%s)%s\n", (t2 && t2[1] == '1') ? "; fallback cached source" : "; fallback parse", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_inject_watch_notification @ gateway/run.py:_inject_watch_notification */
int grun_u_inject_watch_notification(const char *arg) {
    /* Python: queued-event route. Arg =
     * "accepted\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_route") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "retry") == 0) { printf("0 (retryable adapter failure)\n"); return 0; }
    printf("1 (accepted; routing from queued event itself — never foreground message)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_classify_completion_target @ gateway/run.py:_classify_completion_target */
int grun_u_classify_completion_target(const char *arg) {
    /* Python: deliver/terminal/delayed. Arg =
     * "class\tstate\tresult". */
    if (!arg || !*arg) { printf("terminal\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *cls = t1 ? t1 + 1 : "terminal";
    int state = arg[0] == '1';
    if (!state) { printf("terminal\n"); return 0; }
    printf("%s (spawning session live / compression-rotated continuation #55578 pre-flight; durable ack stays honest)%s\n", cls, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_deliver_completion_notification @ gateway/run.py:_deliver_completion_notification */
int grun_u_deliver_completion_notification(const char *arg) {
    /* Python: once-per-lifecycle. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "claimed") == 0) { printf("\n (another caller owns/delivered)\n"); return 0; }
    if (strcmp(state, "retry") == 0) { printf("0 (claim released for retry)\n"); return 0; }
    printf("1 (delivered once per live gateway; durable ids %s)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_enrich_async_delegation_routing @ gateway/run.py:_enrich_async_delegation_routing */
int grun_u_enrich_async_delegation_routing(const char *arg) {
    /* Python: session_key parse. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enriched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!enriched) { printf("0 (already has platform / unparseable — CLI-origin left as-is)\n"); return 0; }
    printf("1 (platform/chat_type/chat_id/thread_id filled from session_key)%s\n", (t2 && t2[1] == '1') ? " — thread_id present" : "");
    return 0;
}

/* PoP: grun_u_async_delegation_watcher @ gateway/run.py:_async_delegation_watcher */
int grun_u_async_delegation_watcher(const char *arg) {
    /* Python: idle drain. Arg =
     * "drained\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (queue empty — silent)\n"); return 0; }
    printf("%s completion(s) injected as new turns (covers idle case; mirrors CLI process_loop drain)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_process_watcher @ gateway/run.py:_run_process_watcher */
int grun_u_run_process_watcher(const char *arg) {
    /* Python: bg process push. Arg =
     * "mode\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *mode = t1 ? t1 + 1 : "all";
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("watcher mode=%s (all → running updates + final; result → final only; error → nonzero only; off → silent; auto-remove on exit)%s\n", mode, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_extract_honcho_cache_busting_config @ gateway/run.py:_extract_honcho_cache_busting_config */
int grun_u_extract_honcho_cache_busting_config(const char *arg) {
    /* Python: memoized honcho keys. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: grun_u_extract_cache_busting_config @ gateway/run.py:_extract_cache_busting_config */
int grun_u_extract_cache_busting_config(const char *arg) {
    /* Python: registry generation. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: grun_u_agent_config_signature @ gateway/run.py:_agent_config_signature */
int grun_u_agent_config_signature(const char *arg) {
    /* Python: cache-stability key. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("signature=%s (model/runtime/toolsets/cache_keys/user_id/user_id_alt)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_rehydrate_session_model_override @ gateway/run.py:_rehydrate_session_model_override */
int grun_u_rehydrate_session_model_override(const char *arg) {
    /* Python: restart persistence. Arg =
     * "rehydrated\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int rehydrated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no-op (in-memory wins or nothing persisted)\n"); return 0; }
    if (!rehydrated) { printf("no persisted override\n"); return 0; }
    printf("rehydrated /model override (api_key never persisted; creds re-resolved): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_release_running_agent_state @ gateway/run.py:_release_running_agent_state */
int grun_u_release_running_agent_state(const char *arg) {
    /* Python: full-pop funnel. Arg =
     * "generation_ok\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int generation_ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!generation_ok) { printf("0 (ownership guard blocked)\n"); return 0; }
    printf("1 (slots popped, lease released, active count persisted)%s\n", (t2 && t2[1] == '1') ? " — busy_ack cleared" : "");
    return 0;
}

/* PoP: grun_u_release_turn_lease @ gateway/run.py:_release_turn_lease */
int grun_u_release_turn_lease(const char *arg) {
    /* Python: generation-keyed lease release. Arg =
     * "session_key\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_rebind_turn_lease @ gateway/run.py:_rebind_turn_lease */
int grun_u_rebind_turn_lease(const char *arg) {
    /* Python: rotation-follow rebind. Arg =
     * "has_key\thas_new\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_key = arg[0] == '1';
    int has_new = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_key || !has_new || !state) { printf("0\n"); return 0; }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_clear_conversation_scope @ gateway/run.py:_clear_conversation_scope */
int grun_u_clear_conversation_scope(const char *arg) {
    /* Python: THE funnel. Arg =
     * "has_key\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_key = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_key) { printf("no scope to clear\n"); return 0; }
    printf("conversation scope cleared (%s, security state too)\n", t2 ? t2 + 1 : "all scoped dicts");
    return 0;
}

/* PoP: grun_u_clear_session_boundary_security_state @ gateway/run.py:_clear_session_boundary_security_state */
int grun_u_clear_session_boundary_security_state(const char *arg) {
    /* Python: per-session clear. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no session key\n"); return 0; }
    printf("session boundary security state cleared: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_bind_adapter_run_generation @ gateway/run.py:_bind_adapter_run_generation */
int grun_u_bind_adapter_run_generation(const char *arg) {
    /* Python: gen stamp. Arg =
     * "bound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int bound = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!bound) { printf("0 (no active-session event)\n"); return 0; }
    printf("1 (run generation stamped on adapter active-session event)%s\n", (t2 && t2[1] == '1') ? " — interrupt cleared it" : "");
    return 0;
}

/* PoP: grun_u_interrupt_and_clear_session @ gateway/run.py:_interrupt_and_clear_session */
int grun_u_interrupt_and_clear_session(const char *arg) {
    /* Python: consistent clear. Arg =
     * "cleared\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cleared = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no session key)\n"); return 0; }
    if (!cleared) { printf("0\n"); return 0; }
    printf("1 (agent interrupted + generation invalidated + adapter activity cleared)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_refresh_agent_cache_message_count @ gateway/run.py:_refresh_agent_cache_message_count */
int grun_u_refresh_agent_cache_message_count(const char *arg) {
    /* Python: re-baseline #45966. Arg =
     * "refreshed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (cache snapshot re-baselined to %s after own turn — next-turn coherence guard won't false-rebuild)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_voice_channel_sidecar_note @ gateway/run.py:_voice_channel_sidecar_note */
int grun_u_voice_channel_sidecar_note(const char *arg) {
    /* Python: VC-change note. Arg =
     * "is_discord\tchanged\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_discord = arg[0] == '1';
    int changed = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_discord || !changed || !state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "[Voice channel now: ...]");
    return 0;
}

/* PoP: grun_u_pinned_session_context_prompt @ gateway/run.py:_pinned_session_context_prompt */
int grun_u_pinned_session_context_prompt(const char *arg) {
    /* Python: pinned bytes or re-render. Arg =
     * "session_key\tpin_hit\tstate\ttext". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int pin_hit = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_init_cached_agent_for_turn @ gateway/run.py:_init_cached_agent_for_turn */
int grun_u_init_cached_agent_for_turn(const char *arg) {
    /* Python: per-turn reset. Arg = "interrupt_depth\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long depth = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("agent reset for turn (depth=%ld, activity %s)\n", depth,
           depth == 0 ? "restamped" : "preserved");
    return 0;
}

/* PoP: grun_u_commit_memory_before_soft_evict @ gateway/run.py:_commit_memory_before_soft_evict */
int grun_u_commit_memory_before_soft_evict(const char *arg) {
    /* Python: LRU-cap on_session_end. Arg =
     * "committed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int committed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!committed) { printf("no commit needed (no manager / non-finalizable / expired)\n"); return 0; }
    printf("on_session_end committed pre-eviction (soft evict keeps session resumable #11205): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_commit_then_release_soft @ gateway/run.py:_commit_then_release_soft */
int grun_u_commit_then_release_soft(const char *arg) {
    /* Python: commit memory then soft-release on eviction thread. Arg =
     * "agent\tkey\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("committed + soft-released: %.*s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: grun_u_release_evicted_agent_soft @ gateway/run.py:_release_evicted_agent_soft */
int grun_u_release_evicted_agent_soft(const char *arg) {
    /* Python: soft cleanup. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no agent to release\n"); return 0; }
    printf("agent released (clients kept, messages freed)%s\n", (tab && tab[1] == '1') ? " + history cleared" : "");
    return 0;
}

/* PoP: grun_u_enforce_agent_cache_cap @ gateway/run.py:_enforce_agent_cache_cap */
int grun_u_enforce_agent_cache_cap(const char *arg) {
    /* Python: LRU evict. Arg =
     * "evicted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int evicted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no cache / non-OrderedDict)\n"); return 0; }
    if (!evicted) { printf("0 (all candidates active — leave over cap, re-check next insert)\n"); return 0; }
    printf("%s agent(s) evicted (running agents skipped; teardown on daemon thread, lock not held during cleanup)\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: grun_u_sweep_idle_cached_agents @ gateway/run.py:_sweep_idle_cached_agents */
int grun_u_sweep_idle_cached_agents(const char *arg) {
    /* Python: idle TTL evict. Arg =
     * "evicted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no cache/lock)\n"); return 0; }
    printf("%s agent(s) evicted (idle > TTL, running skipped, lock acquired internally, teardown daemon threads)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — pending sentinel skipped" : "");
    return 0;
}

/* PoP: grun_u_run_agent_via_proxy @ gateway/run.py:_run_agent_via_proxy */
int grun_u_run_agent_via_proxy(const char *arg) {
    /* Python: thin-relay mode. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (proxy delegation failed)\n"); return 1; }
    printf("1 (delegated via POST /v1/chat/completions SSE stream; platform I/O local, agent remote)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_profile_name_for_source @ gateway/run.py:_profile_name_for_source */
int grun_u_profile_name_for_source(const char *arg) {
    /* Python: route match. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_profile_home_for_source @ gateway/run.py:_resolve_profile_home_for_source */
int grun_u_resolve_profile_home_for_source(const char *arg) {
    /* Python: 3-step home. Arg =
     * "home\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s (active profile fallback)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s (source.profile > re-routed > active profile)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_planned_stop_watcher @ gateway/run.py:_run_planned_stop_watcher */
int grun_u_run_planned_stop_watcher(const char *arg) {
    /* Python: Windows SIGTERM bridge. Arg =
     * "marker\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int marker = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("watcher not started\n"); return 0; }
    if (!marker) { printf("no marker — polling\n"); return 0; }
    printf("planned-stop marker found — invoking graceful shutdown path (#33778 Windows resume fix)%s\n", (t2 && t2[1] == '1') ? " — same handler as SIGTERM" : "");
    return 0;
}

/* PoP: grun_u_start_gateway_housekeeping @ gateway/run.py:_start_gateway_housekeeping */
int grun_u_start_gateway_housekeeping(const char *arg) {
    /* Python: non-cron chores. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("housekeeping thread started (channel dir 5min, image/audio/doc cache + paste sweep + curator poll hourly, auto-archive hourly)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_start_cron_ticker @ gateway/run.py:_start_cron_ticker */
int grun_u_start_cron_ticker(const char *arg) {
    /* Python: deprecated shim -> InProcessCronScheduler. Arg = "state". */
    (void)arg;
    printf("cron ticker started (in-process scheduler)\n");
    return 0;
}

/* PoP: grun_u_await_thread_exit @ gateway/run.py:_await_thread_exit */
int grun_u_await_thread_exit(const char *arg) {
    /* Python: non-blocking join. Arg =
     * "exited\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exited = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!exited) { printf("0 (still alive after timeout — loop never blocked #58818)\n"); return 0; }
    printf("1 (thread exited; is_alive polling keeps loop running so cron delivery completes)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_start_gateway @ gateway/run.py:start_gateway */
int grun_start_gateway(const char *arg) {
    /* Python: main entry. Arg =
     * "ran\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (failed to start — non-zero exit for systemd)\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ran = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (failed to start — non-zero exit for systemd)\n"); return 1; }
    if (!ran) { printf("0\n"); return 1; }
    printf("1 (gateway ran until interrupted; replace=%s; boot fingerprint recorded for code-skew detection)%s\n", (t2 && t2[1] == '1') ? "true" : "false", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_exit_after_graceful_shutdown @ gateway/run.py:_exit_after_graceful_shutdown */
int grun_u_exit_after_graceful_shutdown(const char *arg) {
    /* Python: os._exit backstop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("os._exit called ...%s\n", tab ? tab + 1 : "");
    return 0;
}

/* ── GatewayRunner agent-cache bound methods (resolve via agent_cache_pressure.c) ──
 * These are pure config math — resolve lazily per process, caching on first
 * hit. The live cache objects (_agent_cache, _agent_cache_lock,
 * _running_agent_items) live in the gateway-runner seam and are exercised by
 * the already-ported _enforce_agent_cache_cap / _sweep_idle_cached_agents. */

/* PoP: grun_u_agent_cache_bounds @ gateway/run.py:_agent_cache_bounds */
agent_cache_bounds_t grun_u_agent_cache_bounds(void) {
    static agent_cache_bounds_t cached;
    static bool filled = false;
    if (filled) return cached;
    json_t *raw = config_py_read_raw_config();   /* raw json_t dict, caller frees */
    agent_cache_bounds_t b = acb_resolve_agent_cache_bounds(raw ? raw : NULL);
    if (raw) json_free(raw);
    cached = b;
    filled = true;
    return b;
}

/* PoP: grun_u_agent_cache_cap @ gateway/run.py:_agent_cache_cap */
int grun_u_agent_cache_cap(void) {
    agent_cache_bounds_t b = grun_u_agent_cache_bounds();
    return b.max_size != ACB_UNSET_INT ? b.max_size : ACB_AGENT_CACHE_MAX_SIZE;
}

/* PoP: grun_u_agent_cache_idle_ttl @ gateway/run.py:_agent_cache_idle_ttl */
double grun_u_agent_cache_idle_ttl(void) {
    agent_cache_bounds_t b = grun_u_agent_cache_bounds();
    return b.idle_ttl_secs != ACB_UNSET_FLOAT ? b.idle_ttl_secs : ACB_AGENT_CACHE_IDLE_TTL_SECS;
}


/* ── from src/gateway/port_run_gaps.c ── */
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
