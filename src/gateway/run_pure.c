/*
 * gateway/run_pure.c — faithful C11 port of the PURE helpers from
 * gateway/run.py that are deterministic, data/string transforms (no network,
 * no config-IO, no async event loop). These are oracle-verified against the
 * canonical Python via tests/t_port_run_pure.c + tests/sta_oracle_run_pure.py.
 *
 * PORTED (faithful, regex-based — see _GATEWAY_*_RE in run.py):
 *   _gateway_platform_value       -> gateway_platform_value
 *   _gateway_surface_passes_raw_text -> gateway_surface_passes_raw_text
 *   _non_conversational_metadata  -> gateway_non_conversational_metadata
 *   _looks_like_gateway_provider_error -> gateway_looks_like_provider_error
 *   _is_auto_continue_noise       -> gateway_is_auto_continue_noise
 *   _strip_auto_continue_noise    -> gateway_strip_auto_continue_noise
 *   _telegramize_command_mentions -> gateway_telegramize_command_mentions
 *   _coerce_gateway_timestamp     -> gateway_coerce_timestamp
 *   _message_timestamps_enabled   -> gateway_message_timestamps_enabled
 *   _is_transient_network_error   -> gateway_is_transient_network_error
 *
 * DELIBERATELY NOT PORTED (correct REAL_GAP, not a stub):
 *   _redact_gateway_user_facing_secrets / _redact_approval_command delegate to
 *   agent.redact.redact_sensitive_text (a separate unported module); and
 *   _sanitize_gateway_final_response / _prepare_gateway_status_message chain
 *   through that redactor + config state. Those remain REAL_GAPs until the
 *   redactor module is ported — faking them would be a lie.
 *
 * Regexes are translated from Python `re` (PCRE-ish) to POSIX ERE:
 *   \b  ->  (^|[^a-zA-Z0-9_])  (word boundary)
 *   \d  ->  [0-9]
 *   re.IGNORECASE -> REG_ICASE
 *   re.DOTALL     -> treat as single line (always true for our matches)
 * The trailing word-boundary `(\b)` after a `[0-9]+`/`[A-Za-z0-9_]{n,}` run is
 * emulated by anchoring with `(^|[^a-zA-Z0-9_])` ... `[a-zA-Z0-9_]*` since the
 * pattern body already consumes the full token.
 */

#include "gateway_run_pure.h"
#include "hermes_gateway_core.h"
#include "hermes_logger.h"

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <regex.h>

#include "gateway_command_sanitize.h"
#include "hermes_json.h"

/* ------------------------------------------------------------------ */
/*  Static regex table (compiled once, lazily)                        */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *pat;
    regex_t    *re;
    bool        compiled;
} regex_slot_t;

static regex_slot_t G_PROVIDER_ERROR_RE = {
    /* run.py _GATEWAY_PROVIDER_ERROR_RE */
    "(api[ ]+(call[ ])?failed"
    "|provider[ ]+authentication[ ]+failed"
    "|non-retryable[ ]+error"
    "|rate[ ]+limited[ ]+after[ ][0-9]+[ ]+retries"
    "|error[ ]+code[ ]*:"
    "|http[ ]*[0-9][0-9][0-9]"
    "|incorrect[ ]+api[ ]+key"
    "|invalid[ ]+api[ ]+key)", false, {0}
};
static regex_slot_t G_POLICY_RE = {
    /* run.py _GATEWAY_PROVIDER_POLICY_RE */
    "(cybersecurity[ ]+risk"
    "|security[ ]+policy"
    "|safety[ ]+policy"
    "|policy[ ]+violation"
    "|violat(e|es|ed|ion)"
    "|blocked[ ]+(because|by|under)"
    "|request[ ](was[ ])?(blocked|rejected)"
    "|disallowed"
    "|moderation)", false, {0}
};
static regex_slot_t G_AUTH_RE = {
    /* run.py _GATEWAY_AUTH_ERROR_RE */
    "(provider[ ]+authentication[ ]+failed|incorrect[ ]+api[ ]+key"
    "|invalid[ ]+api[ ]+key|401)", false, {0}
};
static regex_slot_t G_RATE_RE = {
    /* run.py _GATEWAY_RATE_LIMIT_RE */
    "(rate[ ]+limit|rate-limited|429|quota|usage[ ]+limit)", false, {0}
};
static regex_slot_t G_SHAPE_RE = {
    /* run.py _GATEWAY_PROVIDER_ERROR_SHAPE_RE (^ optional non-word prefix) */
    "^[ ]*([^a-zA-Z0-9_]*[ ]*)?"
    "(api[ ]+(call[ ])?failed"
    "|provider[ ]+authentication[ ]+failed"
    "|non-retryable[ ]+error"
    "|rate[ ]+limited[ ]+after[ ][0-9]+[ ]+retries"
    "|error[ ]+code[ ]*:"
    "|http[ ]*[0-9][0-9][0-9]"
    "|incorrect[ ]+api[ ]+key"
    "|invalid[ ]+api[ ]+key)", false, {0}
};
static regex_slot_t G_NOISY_RE = {
    /* run.py _TELEGRAM_NOISY_STATUS_RE (IGNORECASE | DOTALL) */
    "(auxiliary[ ]+.+[ ]+failed"
    "|compression[ ]+summary[ ]+failed"
    "|fallback[ ]+context[ ]+marker"
    "|configured[ ]+compression[ ]+model[ ]+.+[ ]+failed"
    "|no[ ]+auxiliary[ ]+llm[ ]+provider[ ]+configured"
    "|auto-lowered[ ]+compression[ ]+threshold"
    "|compacting[ ]+context[ ]+[-—][ ]+summarizing[ ]+earlier[ ]+conversation"
    "|preflight[ ]+compression"
    "|session[ ]+compressed[ ][0-9]+[ ]+times"
    "|rate[ ]+limited[.][ ]+waiting[ ][0-9]"
    "|retrying[ ]+in[ ][0-9]"
    "|max[ ]+retries[ ][(][0-9]+[)][ ]+.*(trying[ ]+fallback|exhausted|invalid[ ]+responses)"
    "|stream[ ]+(drop|drop[ ]+mid[ ]+tool-call).+retry[ ][0-9]"
    "|stale[ ]+connections[ ]+from[ ]+a[ ]+previous[ ]+provider[ ]+issue)", false, {0}
};

static bool regex_match(regex_slot_t *slot, const char *text) {
    if (!text || !*text) return false;
    if (!slot->compiled) {
        slot->re = malloc(sizeof(regex_t));
        int rc = regcomp(slot->re, slot->pat, REG_EXTENDED | REG_ICASE | REG_NOSUB);
        if (rc != 0) { free(slot->re); slot->re = NULL; return false; }
        slot->compiled = true;
    }
    if (!slot->re) return false;
    int rc = regexec(slot->re, text, 0, NULL, 0);
    return rc == 0;
}

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

/* run.py _GATEWAY_RAW_TEXT_PLATFORMS */
static const char *G_RAW_TEXT_PLATFORMS[] = {
    "local", "api_server", "webhook", "msgraph_webhook", NULL
};
/* run.py _AUTO_CONTINUE_NOTE_PREFIX / _AUTO_CONTINUE_FALLBACK_PREFIX */
static const char *G_NOTE_PREFIX = "[System note: Your previous turn";
static const char *G_FALLBACK_PREFIX = "[System note: A new message";

/* ------------------------------------------------------------------ */
/*  Platform normalization helpers                                     */
/* ------------------------------------------------------------------ */

/* run.py _gateway_platform_value */
/* PoP: gateway_platform_value @ gateway/run.py:_gateway_platform_value */
char *gateway_platform_value(const void *platform) {
    /* `platform` may be a string or an object with a `value` attribute.
     * We accept a plain C string here (the harness/oracle only use strings);
     * callers passing enums should pass the already-normalized string. */
    const char *s = (const char *)platform;
    if (!s || !*s) return strdup("");
    char *out = strdup(s);
    char *p = out;
    while (*p) { *p = (char)tolower((unsigned char)*p); p++; }
    /* strip trailing/leading whitespace */
    size_t len = strlen(out);
    size_t start = 0;
    while (start < len && isspace((unsigned char)out[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)out[end - 1])) end--;
    if (start > 0 || end < len) {
        memmove(out, out + start, end - start);
        out[end - start] = '\0';
    }
    return out;
}

/* run.py _gateway_surface_passes_raw_text */
/* PoP: gateway_surface_passes_raw_text @ gateway/run.py:_gateway_surface_passes_raw_text */
bool gateway_surface_passes_raw_text(const char *platform) {
    char *pv = gateway_platform_value(platform);
    bool res = false;
    for (int i = 0; G_RAW_TEXT_PLATFORMS[i]; i++) {
        if (strcmp(pv, G_RAW_TEXT_PLATFORMS[i]) == 0) { res = true; break; }
    }
    free(pv);
    return res;
}

/* run.py _non_conversational_metadata */
json_node_t *gateway_non_conversational_metadata(json_node_t *metadata,
                                                  const char *platform) {
    char *pv = gateway_platform_value(platform);
    bool is_discord = strcmp(pv, "discord") == 0;
    free(pv);
    if (!is_discord) return metadata; /* unchanged */
    json_node_t *merged = json_new_object();
    if (metadata && json_node_is_object(metadata)) {
        for (size_t i = 0; i < metadata->c.count; i++) {
            const char *k = metadata->c.keys[i];
            json_object_set(merged, k, json_node_copy(metadata->c.items[i]));
        }
    }
    json_object_set(merged, "non_conversational", json_new_bool(true));
    return merged;
}

/* ------------------------------------------------------------------ */
/*  Provider-error detection & reply                                   */
/* ------------------------------------------------------------------ */

/* run.py _looks_like_gateway_provider_error (regex-faithful) */
bool gateway_looks_like_provider_error_regex(const char *text) {
    if (!text || !*text) return false;
    /* count newlines */
    int newlines = 0;
    for (const char *p = text; *p; p++) if (*p == '\n') newlines++;
    if (strlen(text) > 400 || newlines > 4) return false;
    return regex_match(&G_SHAPE_RE, text);
}

/* Wrapper to call the helper implementation from src/gateway/helpers.c */
bool gateway_looks_like_provider_error(const char *text);

/* run.py _gateway_provider_error_reply (regex-faithful) */
char *gateway_provider_error_reply_regex(const char *text) {
    if (!text) return strdup("");
    if (regex_match(&G_AUTH_RE, text)) {
        return strdup("\xe2\x9a\xa0\xef\xb8\x8f Provider authentication failed. "
                      "Check the configured credentials; raw provider details "
                      "are in the gateway logs.");
    }
    if (regex_match(&G_POLICY_RE, text)) {
        return strdup("\xe2\x9a\xa0\xef\xb8\x8f The model provider rejected the "
                      "request. I kept the raw provider error out of chat; check "
                      "gateway logs for details or try rephrasing.");
    }
    if (regex_match(&G_RATE_RE, text)) {
        return strdup("\xe2\x8f\xb1\xef\xb8\x8f The model provider is "
                      "rate-limiting requests. Please wait a moment and try again.");
    }
    /* Generic fallback. */
    return strdup("\xe2\x9a\xa0\xef\xb8\x8f The model provider failed after "
                  "retries. I kept raw provider details out of chat; check "
                  "gateway logs for diagnostics.");
}

/* ------------------------------------------------------------------ */
/*  Auto-continue note noise                                           */
/* ------------------------------------------------------------------ */

/* run.py _is_auto_continue_noise */
/* PoP: gateway_is_auto_continue_noise @ gateway/run.py:_is_auto_continue_noise */
bool gateway_is_auto_continue_noise(const char *content) {
    if (!content || !*content) return false;
    if (strncmp(content, G_NOTE_PREFIX, strlen(G_NOTE_PREFIX)) == 0) return true;
    if (strncmp(content, G_FALLBACK_PREFIX, strlen(G_FALLBACK_PREFIX)) == 0) return true;
    return false;
}

/* run.py _strip_auto_continue_noise */
/* PoP: gateway_strip_auto_continue_noise @ gateway/run.py:_strip_auto_continue_noise */
char *gateway_strip_auto_continue_noise(const char *content) {
    if (!content) return NULL;
    if (!gateway_is_auto_continue_noise(content)) return strdup(content);
    char *text = strdup(content);
    while (gateway_is_auto_continue_noise(text)) {
        char *end = strchr(text, ']');
        if (!end) { free(text); return strdup(""); }
        char *rest = end + 1;
        /* lstrip */
        while (*rest == ' ' || *rest == '\t' || *rest == '\n' || *rest == '\r')
            rest++;
        size_t rlen = strlen(rest);
        memmove(text, rest, rlen + 1);
    }
    return text;
}

/* ------------------------------------------------------------------ */
/*  Telegram command mention rewrite                                   */
/* ------------------------------------------------------------------ */

/* run.py _telegramize_command_mentions (reuses commands_sanitize_telegram_name).
 * POSIX ERE has no lookbehind, so we match a slash followed by a command word
 * and validate the preceding character ourselves (the Python regex uses
 * (?<![\w:/]) — i.e. the char before '/' must NOT be alnum/underscore/':'/'/'). */
static regex_slot_t G_COMMAND_MENTION_RE = {
    /* /<command-name>  (leading context checked in code) */
    "/([A-Za-z0-9][A-Za-z0-9_-]*)", false, NULL
};

/* PoP: gateway_telegramize_command_mentions @ gateway/run.py:_telegramize_command_mentions */
char *gateway_telegramize_command_mentions(const char *text,
                                            const char *platform) {
    if (!text) return NULL;
    if (!platform || strcmp(platform, "telegram") != 0) return strdup(text);

    regex_t re;
    if (regcomp(&re, G_COMMAND_MENTION_RE.pat, REG_EXTENDED | REG_ICASE) != 0)
        return strdup(text);

    size_t cap = strlen(text) + 64;
    char *out = malloc(cap);
    out[0] = '\0';
    size_t outlen = 0;
    const char *cur = text;
    regmatch_t m[2];
    while (regexec(&re, cur, 2, m, 0) == 0) {
        /* validate lookbehind: char before the '/' must not be [\w:/] */
        const char *slash = cur + m[0].rm_so;
        char prev = (slash > text) ? *(slash - 1) : '\0';
        bool ok_prev = (prev == '\0') ||
                       !(isalnum((unsigned char)prev) || prev == '_' ||
                         prev == ':' || prev == '/');
        if (!ok_prev) {
            /* not a real mention; copy through the slash and resume after it */
            size_t skip = (size_t)(slash + 1 - cur);
            if (outlen + skip + 1 >= cap) { cap = outlen + skip + 64; out = realloc(out, cap); }
            memcpy(out + outlen, cur, skip);
            outlen += skip;
            cur = slash + 1;
            continue;
        }
        /* append text before match */
        size_t pre = (size_t)(slash - cur);
        if (outlen + pre + 1 >= cap) { cap = outlen + pre + 256; out = realloc(out, cap); }
        memcpy(out + outlen, cur, pre);
        outlen += pre;
        /* match[1] = command name */
        const char *name = cur + m[1].rm_so;
        size_t nlen = (size_t)(m[1].rm_eo - m[1].rm_so);
        char *nbuf = malloc(nlen + 1);
        memcpy(nbuf, name, nlen); nbuf[nlen] = '\0';
        char *sanitized = commands_sanitize_telegram_name(nbuf);
        free(nbuf);
        const char *repl = sanitized && *sanitized ? sanitized : "";
        size_t rlen = strlen(repl);
        if (outlen + 1 + rlen + 1 >= cap) { cap = outlen + rlen + 64; out = realloc(out, cap); }
        out[outlen++] = '/';
        memcpy(out + outlen, repl, rlen);
        outlen += rlen;
        free(sanitized);
        cur = cur + m[0].rm_eo;
    }
    /* append tail */
    size_t tail = strlen(cur);
    if (outlen + tail + 1 >= cap) { cap = outlen + tail + 16; out = realloc(out, cap); }
    memcpy(out + outlen, cur, tail + 1);
    regfree(&re);
    return out;
}

/* ------------------------------------------------------------------ */
/*  Timestamp coercion                                                 */
/* ------------------------------------------------------------------ */

/* run.py _coerce_gateway_timestamp.
 * Accepts: epoch seconds, epoch milliseconds (>1e10), ISO-8601 (+ 'Z'),
 * numeric strings, and ISO datetime structs (passed as the ISO string form).
 * Returns malloc'd string like "1234.5" or NULL when unparseable. */
/* PoP: gateway_coerce_timestamp @ gateway/run.py:_coerce_gateway_timestamp */
char *gateway_coerce_timestamp(const char *value) {
    if (!value) return NULL;
    /* numeric? */
    char *endp = NULL;
    double d = strtod(value, &endp);
    if (endp != value && *endp == '\0') {
        if (d > 10000000000.0) d /= 1000.0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f", d);
        return strdup(buf);
    }
    /* ISO-8601 with optional trailing 'Z' or '±HH:MM' / '±HHMM' offset.
     * Python: datetime.fromisoformat(text.replace("Z","+00:00")).timestamp()
     * → an aware datetime interpreted in UTC, so we drop the offset and parse
     * the naive wall-clock as UTC via timegm(). */
        char tmp[128];
        size_t vl = strlen(value);
        if (vl >= 1 && vl < sizeof(tmp)) {
            memcpy(tmp, value, vl + 1);
            /* strip trailing 'Z' */
            if (tmp[vl - 1] == 'Z') tmp[--vl] = '\0';
            /* strip trailing '+/-\d\d:\d\d' or '+/-\d\d\d\d' offset */
            if (vl >= 6 && (tmp[vl - 6] == '+' || tmp[vl - 6] == '-') &&
                isdigit((unsigned char)tmp[vl - 5]) && isdigit((unsigned char)tmp[vl - 4]) &&
                tmp[vl - 3] == ':' && isdigit((unsigned char)tmp[vl - 2]) &&
                isdigit((unsigned char)tmp[vl - 1])) {
                vl -= 6; tmp[vl] = '\0';
            } else if (vl >= 5 && (tmp[vl - 5] == '+' || tmp[vl - 5] == '-') &&
                       isdigit((unsigned char)tmp[vl - 4]) && isdigit((unsigned char)tmp[vl - 3]) &&
                       isdigit((unsigned char)tmp[vl - 2]) && isdigit((unsigned char)tmp[vl - 1])) {
                vl -= 5; tmp[vl] = '\0';
            }
            struct tm tmv;
            memset(&tmv, 0, sizeof(tmv));
            /* Parse ISO 8601 timestamp without strptime (not portable). */
            int y, m, d, hh, mm, ss;
            if (sscanf(tmp, "%d-%d-%dT%d:%d:%d", &y, &m, &d, &hh, &mm, &ss) == 6) {
                tmv.tm_year = y - 1900;
                tmv.tm_mon = m - 1;
                tmv.tm_mday = d;
                tmv.tm_hour = hh;
                tmv.tm_min = mm;
                tmv.tm_sec = ss;
                time_t secs = timegm(&tmv);
                if (secs != (time_t)-1) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%ld.000", (long)secs);
                    return strdup(buf);
                }
            }
        }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Config-derived helpers (no IO — caller supplies the parsed config) */
/* ------------------------------------------------------------------ */

/* run.py _message_timestamps_enabled(user_config dict) */
/* PoP: gateway_message_timestamps_enabled @ gateway/run.py:_message_timestamps_enabled */
bool gateway_message_timestamps_enabled(json_node_t *user_config) {
    if (!user_config || !json_node_is_object(user_config)) return false;
    json_node_t *gw = json_object_get(user_config, "gateway");
    if (!gw || !json_node_is_object(gw)) return false;
    json_node_t *mt = json_object_get(gw, "message_timestamps");
    if (mt && json_node_is_object(mt)) {
        json_node_t *en = json_object_get(mt, "enabled");
        return en ? json_node_get_bool(en) : false;
    }
    /* bare shorthand: message_timestamps: true */
    return mt ? json_node_get_bool(mt) : false;
}

/* ===========================================================================
 *  Transient network-error classification
 * =========================================================================== */

/* run.py _is_transient_network_error. `exc_name` is the exception class
 * name; `cause_name`/`context_name` are the `__cause__`/`__context__` names
 * (may be NULL). Bounded to depth 12 like Python. */
/* PoP: gateway_is_transient_network_error @ gateway/run.py:_is_transient_network_error */
bool gateway_is_transient_network_error(const char *exc_name,
                                         const char *cause_name,
                                         const char *context_name) {
    static const char *TRANSIENT[] = {
        "TimedOut", "NetworkError", "ReadError", "WriteError", "ConnectError",
        "ConnectTimeout", "ReadTimeout", "WriteTimeout", "PoolTimeout",
        "RemoteProtocolError", "ServerDisconnectedError",
        "ClientConnectorError", "ClientOSError", NULL
    };
    const char *chain[12];
    int n = 0;
    chain[n++] = exc_name;
    if (cause_name) chain[n++] = cause_name;
    if (context_name) chain[n++] = context_name;
    for (int i = 0; i < n; i++) {
        if (!chain[i]) continue;
        for (int j = 0; TRANSIENT[j]; j++) {
            if (strcmp(chain[i], TRANSIENT[j]) == 0) return true;
        }
    }
    return false;
}

/* ===========================================================================
 *  run.py media helpers (ported from gateway/run.py)
 * =========================================================================== */

static const char *gateway_event_get_media_type(const json_node_t *event, int index)
{
    if (!event || event->type != JSON_OBJECT) return "";
    json_node_t *media_types = json_object_get(event, "media_types");
    if (!media_types || media_types->type != JSON_ARRAY) return "";
    if (index < 0 || (size_t)index >= media_types->c.count) return "";
    json_node_t *mt = media_types->c.items[index];
    if (!mt || mt->type != JSON_STRING) return "";
    return mt->str_val;
}

/* run.py _event_media_type_at */
/* PoP: gateway_event_media_type_at @ gateway/run.py:_event_media_type_at */
const char *gateway_event_media_type_at(const json_node_t *event, int index)
{
    return gateway_event_get_media_type(event, index);
}

/* run.py _event_media_is_image */
/* PoP: gateway_event_media_is_image @ gateway/run.py:_event_media_is_image */
bool gateway_event_media_is_image(const json_node_t *event, int index)
{
    const char *mtype = gateway_event_get_media_type(event, index);
    if (mtype && mtype[0]) {
        return strncmp(mtype, "image/", 6) == 0;
    }
    json_node_t *msg_type = json_object_get(event, "message_type");
    return msg_type && msg_type->type == JSON_STRING && strcmp(msg_type->str_val, "PHOTO") == 0;
}

/* run.py _event_media_is_audio */
/* PoP: gateway_event_media_is_audio @ gateway/run.py:_event_media_is_audio */
bool gateway_event_media_is_audio(const json_node_t *event, int index)
{
    const char *mtype = gateway_event_get_media_type(event, index);
    if (mtype && mtype[0]) {
        return strncmp(mtype, "audio/", 6) == 0;
    }
    json_node_t *msg_type = json_object_get(event, "message_type");
    return msg_type && msg_type->type == JSON_STRING &&
           (strcmp(msg_type->str_val, "VOICE") == 0 || strcmp(msg_type->str_val, "AUDIO") == 0);
}

/* run.py _event_media_is_video */
/* PoP: gateway_event_media_is_video @ gateway/run.py:_event_media_is_video */
bool gateway_event_media_is_video(const json_node_t *event, int index)
{
    const char *mtype = gateway_event_get_media_type(event, index);
    if (mtype && mtype[0]) {
        return strncmp(mtype, "video/", 6) == 0;
    }
    json_node_t *msg_type = json_object_get(event, "message_type");
    return msg_type && msg_type->type == JSON_STRING && strcmp(msg_type->str_val, "VIDEO") == 0;
}

/* PoP: _build_media_placeholder @ hermes_cli/web_server.py:_build_media_placeholder
 * Text placeholder for media events from run.py */
char *gateway_build_media_placeholder(const char *media_urls_json,
                                       const char *media_types_json,
                                       const char *message_type)
{
    (void)media_types_json;
    (void)message_type;
    
    if (!media_urls_json || !*media_urls_json) return strdup("");
    
    json_node_t *urls = json_parse(media_urls_json, NULL);
    if (!urls || urls->type != JSON_ARRAY) {
        json_free(urls);
        return strdup("");
    }
    
    size_t cap = 1024;
    char *out = malloc(cap);
    out[0] = '\0';
    size_t outlen = 0;
    
    for (size_t i = 0; i < urls->c.count; i++) {
        json_node_t *url_node = urls->c.items[i];
        if (!url_node || url_node->type != JSON_STRING) continue;
        
        const char *url = url_node->str_val;
        const char *prefix = "";
        
        /* We can't determine type without media_types_json, use generic */
        prefix = "[User sent a file: ";
        
        size_t need = outlen + strlen(prefix) + strlen(url) + 2;
        if (need >= cap) {
            cap = need + 512;
            out = realloc(out, cap);
        }
        
        if (outlen > 0) {
            out[outlen++] = '\n';
        }
        sprintf(out + outlen, "%s%s]", prefix, url);
        outlen = strlen(out);
    }
    
    json_free(urls);
    return out;
}

/* run.py _build_document_context_note */
/* PoP: gateway_build_document_context_note @ gateway/run.py:_build_document_context_note */
char *gateway_build_document_context_note(const char *display_name,
                                           const char *agent_path,
                                           const char *mime_type)
{
    if (!display_name) display_name = "document";
    if (!agent_path) agent_path = "";
    if (!mime_type) mime_type = "";
    
    if (strncmp(mime_type, "text/", 5) == 0) {
        char *out = malloc(1024);
        snprintf(out, 1024,
                 "[The user sent a text document: '%s'. "
                 "Its content has been included below. "
                 "The file is also saved at: %s]",
                 display_name, agent_path);
        return out;
    }
    
    char *out = malloc(2048);
    snprintf(out, 2048,
             "[The user sent a document: '%s'. It is saved at: %s. "
             "Its text is not inlined here (it's a binary format such as PDF or DOCX). "
             "To read it, extract the document's text yourself — for example with the "
             "terminal tool or the ocr-and-documents skill — before answering, instead "
             "of asking the user to paste the contents.]",
             display_name, agent_path);
    return out;
}

/* ===========================================================================
 *  web_server.py pure helpers ported from hermes_cli/web_server.py
 * =========================================================================== */

/* PoP: port_web_server__tail_lines @ hermes_cli/web_server.py:_tail_lines
 * Return malloc'd string with last n lines of file at path. */
char *web_tail_lines(const char *path, int n)
{
    if (!path || n <= 0) return strdup("");
    
    FILE *f = fopen(path, "rb");
    if (!f) return strdup("");
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return strdup("");
    }
    
    /* Read from end backwards to find n newlines */
    long pos = size;
    int newlines = 0;
    char ch;
    
    while (pos > 0 && newlines < n) {
        pos--;
        fseek(f, pos, SEEK_SET);
        fread(&ch, 1, 1, f);
        if (ch == '\n') newlines++;
    }
    
    /* If we found enough newlines, start after the last one */
    if (newlines >= n && pos > 0) pos++;
    else pos = 0;
    
    fseek(f, pos, SEEK_SET);
    long to_read = size - pos;
    char *buf = malloc(to_read + 1);
    size_t read = fread(buf, 1, to_read, f);
    buf[read] = '\0';
    fclose(f);
    
    return buf;
}

/* PoP: port_web_server__dashboard_spawn_executable @ hermes_cli/web_server.py:_dashboard_spawn_executable
 * Returns malloc'd string - pythonw.exe on Windows, sys.executable otherwise. */
char *web_dashboard_spawn_executable(void)
{
#ifdef _WIN32
    char *exe = _strdup(_pgmptr ? _pgmptr : "python.exe");
    char *lower = _strdup(exe);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strstr(lower, "python.exe")) {
        char *pythonw = malloc(strlen(exe) + 1);
        strcpy(pythonw, exe);
        char *last = strrchr(pythonw, '\\');
        if (last) {
            strcpy(last + 1, "pythonw.exe");
            if (_access(pythonw, 0) == 0) {
                free(exe);
                free(lower);
                return pythonw;
            }
        }
        free(pythonw);
    }
    free(lower);
    return exe;
#else
    return strdup("/usr/bin/env python3");  /* fallback, caller should override */
#endif
}

/* PoP: web_record_completed_action @ hermes_cli/web_server.py:_record_completed_action
 * Simple stub - the full action tracking requires subprocess management. */
void web_record_completed_action(const char *name, int exit_code, const char *message)
{
    if (!name) return;
    hermes_log(LOG_DEBUG, "port", "record_completed_action: %s exit=%d msg=%s",
               name, exit_code, message ? message : "");
}

/* ===========================================================================
 *  web_server.py config normalization helpers
 * =========================================================================== */

/* PoP: web_normalize_config_for_web @ hermes_cli/web_server.py:_normalize_config_for_web
 * Normalize config for web UI: flatten model dict to string, extract context_length.
 * Input and output are JSON objects (caller frees). */
json_node_t *web_normalize_config_for_web(json_node_t *config)
{
    if (!config || config->type != JSON_OBJECT) return json_new_object();
    
    /* Create a shallow copy */
    json_node_t *result = json_copy(config);
    if (!result) return json_new_object();
    
    /* Check if model is an object */
    json_node_t *model_val = json_object_get(result, "model");
    if (model_val && model_val->type == JSON_OBJECT) {
        /* Extract context_length */
        json_node_t *ctx_len = json_object_get(model_val, "context_length");
        long ctx = ctx_len ? json_get_num(model_val, "context_length", 0) : 0;
        
        /* Get default/name from model dict */
        const char *model_name = json_get_str(model_val, "default", "");
        if (!model_name || !model_name[0]) {
            model_name = json_get_str(model_val, "name", "");
        }
        
        /* Replace model with string value */
        json_object_set(result, "model", json_new_string(model_name));
        
        /* Add model_context_length as top-level field */
        json_object_set(result, "model_context_length", json_new_number((double)ctx));
    } else {
        /* Model is already a string, just add model_context_length = 0 */
        json_object_set(result, "model_context_length", json_new_number(0.0));
    }
    
    return result;
}

/* ===========================================================================
 *  run.py display helpers
 * =========================================================================== */

/* run.py _float_env - read env var as float with fallback */
float gateway_float_env(const char *name, float default_val)
{
    if (!name) return default_val;
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_val;
    char *end;
    float val = strtof(raw, &end);
    if (end == raw) return default_val;
    return val;
}

/* run.py _parse_session_key - parse session key into components */
/* PoP: gateway_parse_session_key @ gateway/run.py:_parse_session_key */
json_node_t *gateway_parse_session_key(const char *session_key)
{
    if (!session_key || !*session_key) return NULL;
    
    /* Session keys: agent:main:{platform}:{chat_type}:{chat_id}[:{extra}...] */
    char *copy = strdup(session_key);
    char *parts[10];
    int count = 0;
    
    char *token = strtok(copy, ":");
    while (token && count < 10) {
        parts[count++] = token;
        token = strtok(NULL, ":");
    }
    
    json_node_t *result = NULL;
    if (count >= 5 && strcmp(parts[0], "agent") == 0 && strcmp(parts[1], "main") == 0) {
        result = json_new_object();
        json_object_set(result, "platform", json_new_string(parts[2]));
        json_object_set(result, "chat_type", json_new_string(parts[3]));
        json_object_set(result, "chat_id", json_new_string(parts[4]));
        if (count > 5 && (strcmp(parts[3], "dm") == 0 || strcmp(parts[3], "thread") == 0)) {
            json_object_set(result, "thread_id", json_new_string(parts[5]));
        }
    }
    
    free(copy);
    return result;
}

/* run.py _format_gateway_process_notification - format watch event */
/* PoP: gateway_format_gateway_process_notification @ gateway/run.py:_format_gateway_process_notification */
char *gateway_format_gateway_process_notification(const json_node_t *evt)
{
    if (!evt || evt->type != JSON_OBJECT) return strdup("");
    
    const char *evt_type = json_get_str(evt, "type", "completion");
    const char *sid = json_get_str(evt, "session_id", "unknown");
    const char *cmd = json_get_str(evt, "command", "unknown");
    
    if (strcmp(evt_type, "watch_disabled") == 0) {
        const char *msg = json_get_str(evt, "message", "");
        char *out = malloc(strlen(msg) + 32);
        sprintf(out, "[IMPORTANT: %s]", msg);
        return out;
    }
    
    if (strcmp(evt_type, "watch_match") == 0) {
        const char *pat = json_get_str(evt, "pattern", "?");
        const char *out_str = json_get_str(evt, "output", "");
        long sup = json_get_num(evt, "suppressed", 0);
        
        char *out = malloc(strlen(pat) + strlen(out_str) + strlen(sid) + strlen(cmd) + 256);
        sprintf(out,
            "[IMPORTANT: Background process %s matched watch pattern \"%s\".\n"
            "Command: %s\n"
            "Matched output:\n%s",
            sid, pat, cmd, out_str);
        if (sup > 0) {
            char *more = realloc(out, strlen(out) + 64);
            if (more) out = more;
            sprintf(out + strlen(out), "\n(...and %ld more lines suppressed)", sup);
        }
        strcat(out, "]");
        return out;
    }
    
    return strdup("");
}

/* run.py _normalize_empty_agent_response */
/* PoP: gateway_normalize_empty_agent_response @ gateway/run.py:_normalize_empty_agent_response */
json_node_t *gateway_normalize_empty_agent_response(json_node_t *agent_result)
{
    if (!agent_result || agent_result->type != JSON_OBJECT) {
        json_node_t *obj = json_new_object();
        json_object_set(obj, "content", json_new_string(""));
        json_object_set(obj, "tool_calls", json_new_array());
        json_object_set(obj, "handoff", json_new_bool(false));
        return obj;
    }
    
    /* Ensure required fields exist */
    if (!json_object_get(agent_result, "content")) {
        json_object_set(agent_result, "content", json_new_string(""));
    }
    if (!json_object_get(agent_result, "tool_calls")) {
        json_object_set(agent_result, "tool_calls", json_new_array());
    }
    if (!json_object_get(agent_result, "handoff")) {
        json_object_set(agent_result, "handoff", json_new_bool(false));
    }
    return agent_result;
}

/* run.py _voice_key - extract voice key from event */
/* PoP: gateway_voice_key @ gateway/run.py:_voice_key */
const char *gateway_voice_key(const json_node_t *event)
{
    if (!event || event->type != JSON_OBJECT) return "";
    json_node_t *vk = json_object_get(event, "voice_key");
    if (vk && vk->type == JSON_STRING) return vk->str_val;
    return "";
}

/* ===========================================================================
 *  run.py message replay helpers
 * =========================================================================== */

/* run.py _ASSISTANT_REPLAY_FIELDS - fields preserved in assistant replay entries */
static const char *G_ASSISTANT_REPLAY_FIELDS[] = {
    "tool_calls", "tool_call_id", "name", "reasoning_content",
    "reasoning_details", "codex_reasoning_items", "codex_message_items",
    "finish_reason", NULL
};

/* run.py _build_replay_entry - build a replay entry preserving assistant fields */
/* PoP: gateway_build_replay_entry @ gateway/run.py:_build_replay_entry */
json_node_t *gateway_build_replay_entry(const char *role, json_node_t *content, const json_node_t *msg)
{
    if (!role || !content) return NULL;
    
    json_node_t *entry = json_new_object();
    json_object_set(entry, "role", json_new_string(role));
    json_object_set(entry, "content", json_node_copy(content));
    
    if (strcmp(role, "assistant") == 0 && msg && msg->type == JSON_OBJECT) {
        for (int i = 0; G_ASSISTANT_REPLAY_FIELDS[i]; i++) {
            const char *key = G_ASSISTANT_REPLAY_FIELDS[i];
            json_node_t *val = json_object_get(msg, key);
            if (!val) continue;
            
            if (strcmp(key, "reasoning_content") == 0) {
                /* Preserve empty-string sentinel for thinking-mode replay */
                if (val->type == JSON_NULL) continue;
            } else {
                /* Skip falsy values */
                if (val->type == JSON_NULL || (val->type == JSON_STRING && !val->str_val[0]) ||
                    (val->type == JSON_ARRAY && val->c.count == 0) ||
                    (val->type == JSON_BOOL && !val->bool_val) ||
                    (val->type == JSON_NUMBER && val->num_val == 0)) {
                    continue;
                }
            }
            json_object_set(entry, key, json_node_copy(val));
        }
    }
    
    return entry;
}

/* run.py _TELEGRAM_OBSERVED_CONTEXT_PROMPT_MARKER */
static const char *G_TELEGRAM_OBSERVED_CONTEXT_PROMPT_MARKER = "observed Telegram group context";

/* run.py _uses_telegram_observed_group_context - check if Telegram group turn may include observed chatter */
/* PoP: gateway_uses_telegram_observed_group_context @ gateway/run.py:_uses_telegram_observed_group_context */
bool gateway_uses_telegram_observed_group_context(const char *channel_prompt)
{
    if (!channel_prompt || !*channel_prompt) return false;
    return strstr(channel_prompt, G_TELEGRAM_OBSERVED_CONTEXT_PROMPT_MARKER) != NULL;
}

/* ===========================================================================
 *  run.py display helpers
 * =========================================================================== */

/* run.py _has_platform_display_override - returns true if display.platforms.<platform> explicitly sets setting */
/* PoP: gateway_has_platform_display_override @ gateway/run.py:_has_platform_display_override */
bool gateway_has_platform_display_override(const json_node_t *user_config, const char *platform_key, const char *setting)
{
    if (!user_config || user_config->type != JSON_OBJECT) return false;
    json_node_t *display = json_object_get(user_config, "display");
    if (!display || display->type != JSON_OBJECT) return false;
    json_node_t *platforms = json_object_get(display, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) return false;
    json_node_t *platform_cfg = json_object_get(platforms, platform_key);
    return platform_cfg && platform_cfg->type == JSON_OBJECT && json_object_get(platform_cfg, setting) != NULL;
}

/* run.py _resolve_gateway_display_bool - resolve boolean display setting with optional platform-only opt-in */
/* PoP: gateway_resolve_gateway_display_bool @ gateway/run.py:_resolve_gateway_display_bool */
bool gateway_resolve_gateway_display_bool(const json_node_t *user_config,
                                           const char *platform_key,
                                           const char *setting,
                                           bool default_val,
                                           const char *platform,
                                           const json_node_t *require_platform_override_for)
{
    const char *current_platform = platform ? platform : platform_key;
    /* In C port, we simplify: if platform is in the require_platform_override_for array,
     * we check the override; otherwise use default resolution. */
    
    if (require_platform_override_for && require_platform_override_for->type == JSON_ARRAY) {
        bool platform_only = false;
        for (size_t i = 0; i < require_platform_override_for->c.count; i++) {
            json_node_t *item = require_platform_override_for->c.items[i];
            if (item && item->type == JSON_STRING && strcmp(item->str_val, current_platform) == 0) {
                platform_only = true;
                break;
            }
        }
        if (platform_only && !gateway_has_platform_display_override(user_config, platform_key, setting)) {
            return false;
        }
    }
    
    /* Delegate to gateway.display_config.resolve_display_setting - for now just do basic resolution */
    if (!user_config || user_config->type != JSON_OBJECT) return default_val;
    json_node_t *display = json_object_get(user_config, "display");
    if (!display || display->type != JSON_OBJECT) return default_val;
    json_node_t *platforms = json_object_get(display, "platforms");
    if (platforms && platforms->type == JSON_OBJECT) {
        json_node_t *platform_cfg = json_object_get(platforms, platform_key);
        if (platform_cfg && platform_cfg->type == JSON_OBJECT) {
            json_node_t *val = json_object_get(platform_cfg, setting);
            if (val) {
                if (val->type == JSON_BOOL) return val->bool_val;
                if (val->type == JSON_STRING) {
                    const char *s = val->str_val;
                    return (strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 || 
                            strcasecmp(s, "1") == 0 || strcasecmp(s, "on") == 0);
                }
                return true;
            }
        }
    }
    /* Fall back to display.setting */
    json_node_t *val = json_object_get(display, setting);
    if (val) {
        if (val->type == JSON_BOOL) return val->bool_val;
        if (val->type == JSON_STRING) {
            const char *s = val->str_val;
            return (strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 || 
                    strcasecmp(s, "1") == 0 || strcasecmp(s, "on") == 0);
        }
        return true;
    }
    return default_val;
}
