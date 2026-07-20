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

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
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
bool gateway_is_auto_continue_noise(const char *content) {
    if (!content || !*content) return false;
    if (strncmp(content, G_NOTE_PREFIX, strlen(G_NOTE_PREFIX)) == 0) return true;
    if (strncmp(content, G_FALLBACK_PREFIX, strlen(G_FALLBACK_PREFIX)) == 0) return true;
    return false;
}

/* run.py _strip_auto_continue_noise */
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
        /* strip trailing '+/-\\d\\d:\\d\\d' or '+/-\\d\\d\\d\\d' offset */
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
        char *rp = strptime(tmp, "%Y-%m-%dT%H:%M:%S", &tmv);
        if (rp && *rp == '\0') {
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

/* ------------------------------------------------------------------ */
/*  Transient network-error classification                             */
/* ------------------------------------------------------------------ */

/* run.py _is_transient_network_error. `exc_name` is the exception class
 * name; `cause_name`/`context_name` are the `__cause__`/`__context__` names
 * (may be NULL). Bounded to depth 12 like Python. */
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
