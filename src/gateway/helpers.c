/*
 * helpers.c — Shared gateway helper utilities.
 * Port of Python gateway/platforms/helpers.py.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Message Deduplicator
 * Port of Python gateway/platforms/helpers.py MessageDeduplicator class.
 * ================================================================ */

void msg_dedup_init(msg_dedup_t *d) {
    msg_dedup_init_custom(d, 2000, 300.0);
}

void msg_dedup_init_custom(msg_dedup_t *d, int max_size, double ttl_seconds) {
    if (!d) return;
    d->msg_ids = NULL;
    d->timestamps = NULL;
    d->count = 0;
    d->max_size = max_size > 0 ? max_size : 2000;
    d->ttl_seconds = ttl_seconds > 0 ? ttl_seconds : 300.0;
}

bool msg_dedup_is_duplicate(msg_dedup_t *d, const char *msg_id) {
    if (!d || !msg_id || !*msg_id) return false;

    double now = (double)time(NULL);

    /* Scan existing entries */
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->msg_ids[i], msg_id) == 0) {
            if (now - d->timestamps[i] < d->ttl_seconds) {
                /* Update timestamp and return duplicate */
                d->timestamps[i] = now;
                return true;
            }
            /* Entry expired — remove it (swap with last) */
            free(d->msg_ids[i]);
            d->msg_ids[i] = d->msg_ids[d->count - 1];
            d->timestamps[i] = d->timestamps[d->count - 1];
            d->count--;
            break;
        }
    }

    /* Add new entry */
    char **new_ids = realloc(d->msg_ids, (d->count + 1) * sizeof(char *));
    double *new_ts = realloc(d->timestamps, (d->count + 1) * sizeof(double));
    if (!new_ids || !new_ts) {
        free(new_ids); free(new_ts);
        return false;
    }
    d->msg_ids = new_ids;
    d->timestamps = new_ts;
    d->msg_ids[d->count] = strdup(msg_id);
    d->timestamps[d->count] = now;
    d->count++;

    /* Prune if over max_size */
    if (d->count > d->max_size) {
        double cutoff = now - d->ttl_seconds;
        int write_idx = 0;
        for (int i = 0; i < d->count; i++) {
            if (d->timestamps[i] > cutoff) {
                d->msg_ids[write_idx] = d->msg_ids[i];
                d->timestamps[write_idx] = d->timestamps[i];
                write_idx++;
            } else {
                free(d->msg_ids[i]);
            }
        }
        d->count = write_idx;

        /* If still over max after TTL prune, keep newest */
        if (d->count > d->max_size) {
            /* Simple approach: keep last max_size entries */
            int remove = d->count - d->max_size;
            for (int i = 0; i < remove; i++) {
                free(d->msg_ids[i]);
            }
            for (int i = remove; i < d->count; i++) {
                d->msg_ids[i - remove] = d->msg_ids[i];
                d->timestamps[i - remove] = d->timestamps[i];
            }
            d->count = d->max_size;
        }

        /* Shrink arrays */
        char **shrunk_ids = realloc(d->msg_ids, d->count * sizeof(char *));
        double *shrunk_ts = realloc(d->timestamps, d->count * sizeof(double));
        if (shrunk_ids) d->msg_ids = shrunk_ids;
        if (shrunk_ts) d->timestamps = shrunk_ts;
    }

    return false;
}

void msg_dedup_clear(msg_dedup_t *d) {
    if (!d) return;
    for (int i = 0; i < d->count; i++)
        free(d->msg_ids[i]);
    free(d->msg_ids);
    free(d->timestamps);
    d->msg_ids = NULL;
    d->timestamps = NULL;
    d->count = 0;
}

void msg_dedup_destroy(msg_dedup_t *d) {
    msg_dedup_clear(d);
}

/* ================================================================
 *  Markdown Stripping
 * ================================================================ */

typedef struct {
    const char *pattern;
    const char *replacement;
} md_rule_t;

/* Apply a single replacement rule to a dynamically allocated buffer.
 * Returns new buffer (caller must free). */
static char *apply_rule(const char *text, const md_rule_t *rule) {
    if (!text || !rule) return NULL;
    const char *pat = rule->pattern;
    const char *repl = rule->replacement;
    size_t pat_len = strlen(pat);

    /* Count occurrences */
    int count = 0;
    const char *p = text;
    while ((p = strstr(p, pat)) != NULL) {
        count++;
        p += pat_len;
    }
    if (count == 0) return strdup(text);

    /* Build result */
    size_t repl_len = strlen(repl);
    size_t result_cap = strlen(text) + (repl_len * (size_t)count) + 1;
    char *result = malloc(result_cap);
    if (!result) return NULL;

    const char *src = text;
    char *dst = result;
    while (*src) {
        const char *found = strstr(src, pat);
        if (found) {
            size_t copy_len = (size_t)(found - src);
            memcpy(dst, src, copy_len);
            dst += copy_len;
            memcpy(dst, repl, repl_len);
            dst += repl_len;
            src = found + pat_len;
        } else {
            size_t remaining = strlen(src);
            memcpy(dst, src, remaining);
            dst += remaining;
            break;
        }
    }
    *dst = '\0';
    return result;
}

/* Strip markdown formatting patterns.
 * Port of Python gateway/platforms/helpers.py strip_markdown().
 * This is a simplified C port of Python's regex-based strip_markdown().
 * AG26: Port of Python gateway/platforms/helpers.py:strip_markdown().
 */
char *strip_markdown(const char *text) {
    if (!text) return NULL;

    char *result = strdup(text);
    if (!result) return NULL;

    /* Define replacement rules (order matters) */
    md_rule_t rules[] = {
        /* Code blocks first (remove ``` lines) */
        {"```", ""},
        /* Inline code: `text` → text */
        {"`", ""},
        /* Bold: **text** → text (remove **) */
        {"**", ""},
        /* Italic: *text* → text (remove single *) */
        /* Headings: # text → text */
        {"#", ""},
        /* Link: [text](url) → text — remove ](url) part */
        {"[", ""},
        {"](", " "},
        {")", ""},
        /* Multiple newlines → double newline */
        {"\n\n\n", "\n\n"},
        {"\n\n\n", "\n\n"},
    };
    int n_rules = sizeof(rules) / sizeof(rules[0]);

    for (int i = 0; i < n_rules; i++) {
        char *next = apply_rule(result, &rules[i]);
        if (next) {
            free(result);
            result = next;
        }
    }

    /* Trim leading/trailing whitespace */
    while (*result == ' ' || *result == '\n' || *result == '\t') {
        memmove(result, result + 1, strlen(result));
    }
    size_t len = strlen(result);
    while (len > 0 && (result[len-1] == ' ' || result[len-1] == '\n' || result[len-1] == '\t')) {
        result[--len] = '\0';
    }

    return result;
}

/* ================================================================
 *  Phone Number Redaction
 * Port of Python gateway/platforms/helpers.py redact_phone().
 * AG26: Port of Python gateway/platforms/helpers.py:redact_phone().
 * ================================================================ */

char *redact_phone(const char *phone) {
    if (!phone) return strdup("<none>");
    if (strcmp(phone, "<none>") == 0) return strdup("<none>");

    size_t len = strlen(phone);
    if (len <= 4) return strdup("****");
    if (len <= 8) {
        /* Show first 2 and last 2 */
        char *result = malloc(10);
        if (!result) return NULL;
        snprintf(result, 10, "%.2s****%.2s", phone, phone + len - 2);
        return result;
    }
    /* Show first 4 and last 4 */
    char *result = malloc(14);
    if (!result) return NULL;
    snprintf(result, 14, "%.4s****%.4s", phone, phone + len - 4);
    return result;
}

/* ================================================================
 *  Provider Error Sanitization
 *  Port of Python gateway/run.py _sanitize_gateway_final_response()
 * ================================================================ */

/* Provider error shape patterns — short text that starts with these markers
 * is likely a provider failure envelope, not normal assistant content.
 * Mirrors Python _GATEWAY_PROVIDER_ERROR_SHAPE_RE. */
static const char *PROVIDER_ERROR_START_PATTERNS[] = {
    "api call failed",
    "provider authentication failed",
    "non-retryable error",
    "rate limited after",
    "error code:",
    "http ",
    "incorrect api key",
    "invalid api key",
};
static const int PROVIDER_ERROR_START_COUNT =
    sizeof(PROVIDER_ERROR_START_PATTERNS) / sizeof(PROVIDER_ERROR_START_PATTERNS[0]);

/* Auth error patterns for reply selection.
 * Mirrors Python _GATEWAY_AUTH_ERROR_RE. */
static const char *GATEWAY_AUTH_PATTERNS[] = {
    "auth", "api key", "credential", "unauthorized", "forbidden",
    "invalid token", "token expired", "token revoked",
};
static const int GATEWAY_AUTH_COUNT =
    sizeof(GATEWAY_AUTH_PATTERNS) / sizeof(GATEWAY_AUTH_PATTERNS[0]);

/* Provider policy blocked patterns for reply selection.
 * Mirrors Python _GATEWAY_PROVIDER_POLICY_RE. */
static const char *GATEWAY_POLICY_PATTERNS[] = {
    "no endpoints available matching",
    "guardrail",
    "data policy",
};
static const int GATEWAY_POLICY_COUNT =
    sizeof(GATEWAY_POLICY_PATTERNS) / sizeof(GATEWAY_POLICY_PATTERNS[0]);

/* Rate limit patterns for reply selection.
 * Mirrors Python _GATEWAY_RATE_LIMIT_RE. */
static const char *GATEWAY_RATE_LIMIT_PATTERNS[] = {
    "rate limit", "rate_limit", "too many requests", "throttled",
    "resource_exhausted",
};
static const int GATEWAY_RATE_LIMIT_COUNT =
    sizeof(GATEWAY_RATE_LIMIT_PATTERNS) / sizeof(GATEWAY_RATE_LIMIT_PATTERNS[0]);

/* Noisy Telegram status patterns to filter out.
 * Mirrors Python _TELEGRAM_NOISY_STATUS_RE. */
static const char *NOISY_STATUS_PATTERNS[] = {
    "processing your request",
    "thinking",
    "generating response",
    "computing",
};
static const int NOISY_STATUS_COUNT =
    sizeof(NOISY_STATUS_PATTERNS) / sizeof(NOISY_STATUS_PATTERNS[0]);

/* Case-insensitive start-of-string match for any pattern. */
static bool starts_with_any_i(const char *text, const char *patterns[], int count) {
    if (!text) return false;
    for (int i = 0; i < count; i++) {
        size_t plen = strlen(patterns[i]);
        if (strncasecmp(text, patterns[i], plen) == 0)
            return true;
    }
    return false;
}

/* Count newline characters in text. */
static int count_lines(const char *text) {
    int lines = 1;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') lines++;
    }
    return lines;
}

/* Check if text looks like a provider/infrastructure error (not normal content).
 * Port of Python gateway/run.py _looks_like_gateway_provider_error().
 * Two heuristics from Python: text is short (<=3 lines) AND error marker at start. */
bool gateway_looks_like_provider_error(const char *text) {
    if (!text || !*text) return false;
    if (count_lines(text) > 3) return false;

    /* Skip leading whitespace/punctuation before checking patterns. */
    const char *p = text;
    while (*p && (isspace((unsigned char)*p) || ispunct((unsigned char)*p)))
        p++;

    return starts_with_any_i(p, PROVIDER_ERROR_START_PATTERNS,
                             PROVIDER_ERROR_START_COUNT);
}

/* Map a raw provider error to a short user-safe reply.
 * Port of Python gateway/run.py _gateway_provider_error_reply().
 * AG26: Port of Python gateway/run.py:_gateway_provider_error_reply().
 */
char *gateway_provider_error_reply(const char *text) {
    if (!text) return strdup("");

    /* Skip leading whitespace for pattern matching. */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;

    if (starts_with_any_i(p, GATEWAY_RATE_LIMIT_PATTERNS, GATEWAY_RATE_LIMIT_COUNT)) {
        return strdup("⏱️ The model provider is rate-limiting requests. "
                       "Please wait a moment and try again.");
    }
    if (starts_with_any_i(p, GATEWAY_AUTH_PATTERNS, GATEWAY_AUTH_COUNT)) {
        return strdup("⚠️ Provider authentication failed. "
                       "Check the configured credentials; "
                       "raw provider details are in the gateway logs.");
    }
    if (starts_with_any_i(p, GATEWAY_POLICY_PATTERNS, GATEWAY_POLICY_COUNT)) {
        return strdup("⚠️ The model provider rejected the request. "
                       "Check gateway logs for details or try rephrasing.");
    }
    /* Generic fallback. */
    return strdup("⚠️ The model provider failed after retries. "
                   "Check gateway logs for diagnostics.");
}

/* Sanitize a gateway response before sending to chat.
 * For Telegram: detects provider errors and rewrites them.
 * Secret redaction is done separately via hermes_redact().
 * Returns a malloc'd string (caller must free). */
char *gateway_sanitize_response(const char *platform, const char *text) {
    if (!text) return NULL;

    /* Only apply provider error rewriting for Telegram.
     * Secret redaction is handled separately via hermes_redact(). */
    if (!platform || strcmp(platform, "telegram") != 0)
        return strdup(text);

    if (gateway_looks_like_provider_error(text))
        return gateway_provider_error_reply(text);

    return strdup(text);
}

/* Filter a status message before platform delivery.
 * Port of Python gateway/run.py _prepare_gateway_status_message().
 * Returns NULL if message should be filtered out entirely.
 * Returns a malloc'd string (caller must free) otherwise. */
char *gateway_prepare_status_message(const char *platform, const char *text) {
    if (!text || !*text) return NULL;

    /* Skip leading whitespace. */
    const char *p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return NULL;

    /* Only apply Telegram-specific filtering. */
    if (!platform || strcmp(platform, "telegram") != 0)
        return strdup(p);

    /* Filter noisy status patterns. */
    if (starts_with_any_i(p, NOISY_STATUS_PATTERNS, NOISY_STATUS_COUNT))
        return NULL;

    /* If it looks like a provider error, rewrite it. */
    if (gateway_looks_like_provider_error(p))
        return gateway_provider_error_reply(p);

    return strdup(p);
}

/* ================================================================
 *  Thread Participation Tracker
 * Port of Python gateway/platforms/helpers.py ThreadTracker.
 * ================================================================ */

void thread_tracker_init(thread_tracker_t *t, const char *platform,
                         const char *state_dir) {
    if (!t) return;
    t->thread_ids = NULL;
    t->count = 0;
    t->max_tracked = 500;
    if (platform)
        snprintf(t->platform, sizeof(t->platform), "%s", platform);
    else
        t->platform[0] = '\0';
    if (state_dir)
        snprintf(t->state_dir, sizeof(t->state_dir), "%s", state_dir);
    else
        t->state_dir[0] = '\0';
}

void thread_tracker_load(thread_tracker_t *t) {
    if (!t || !t->state_dir[0] || !t->platform[0]) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_threads.json",
             t->state_dir, t->platform);

    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    char *jerr = NULL;
    json_node_t *json = json_parse(buf, &jerr);
    free(buf); free(jerr);
    if (!json || json->type != JSON_ARRAY) {
        if (json) json_free(json);
        return;
    }

    size_t count = json_len(json);
    for (size_t i = 0; i < count && t->count < t->max_tracked; i++) {
        json_t *elem = json_get(json, (int)i);
        const char *tid = (elem && elem->type == JSON_STRING) ? elem->str_val : NULL;
        if (tid) {
            char **new_ids = realloc(t->thread_ids,
                                     (t->count + 1) * sizeof(char *));
            if (!new_ids) break;
            t->thread_ids = new_ids;
            t->thread_ids[t->count] = strdup(tid);
            t->count++;
        }
    }
    json_free(json);
}

void thread_tracker_mark(thread_tracker_t *t, const char *thread_id) {
    if (!t || !thread_id || !*thread_id) return;

    /* Check if already tracked */
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->thread_ids[i], thread_id) == 0)
            return; /* Already tracked, no need to persist */
    }

    /* Add new */
    char **new_ids = realloc(t->thread_ids, (t->count + 1) * sizeof(char *));
    if (!new_ids) return;
    t->thread_ids = new_ids;
    t->thread_ids[t->count] = strdup(thread_id);
    t->count++;

    /* Prune if over max */
    int start = 0;
    if (t->count > t->max_tracked) {
        start = t->count - t->max_tracked;
        for (int i = 0; i < start; i++)
            free(t->thread_ids[i]);
        for (int i = start; i < t->count; i++)
            t->thread_ids[i - start] = t->thread_ids[i];
        t->count = t->max_tracked;
    }

    /* Persist to JSON file */
    if (t->state_dir[0] && t->platform[0]) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s_threads.json",
                 t->state_dir, t->platform);

        json_node_t *json = json_array();
        if (json) {
            for (int i = 0; i < t->count; i++)
                json_append(json, json_string(t->thread_ids[i]));
            char *serialized = json_serialize(json);
            if (serialized) {
                FILE *f = fopen(path, "w");
                if (f) {
                    fputs(serialized, f);
                    fclose(f);
                }
                free(serialized);
            }
            json_free(json);
        }
    }
}

bool thread_tracker_has(thread_tracker_t *t, const char *thread_id) {
    if (!t || !thread_id) return false;
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->thread_ids[i], thread_id) == 0)
            return true;
    }
    return false;
}

void thread_tracker_destroy(thread_tracker_t *t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++)
        free(t->thread_ids[i]);
    free(t->thread_ids);
    t->thread_ids = NULL;
    t->count = 0;
}

/* ================================================================
 *  Gateway restart helpers
 *  Port of Python gateway/restart.py.
 * ================================================================ */

/* Parse a drain timeout value, falling back to DEFAULT_RESTART_DRAIN_TIMEOUT (30s).
 * Port of Python gateway/restart.py parse_restart_drain_timeout().
 * AG26: Port of Python gateway/restart.py:parse_restart_drain_timeout().
 */
#define DEFAULT_RESTART_DRAIN_TIMEOUT 30.0

double parse_restart_drain_timeout(const char *raw) {
    if (!raw || !*raw) return DEFAULT_RESTART_DRAIN_TIMEOUT;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || val < 0.0) return DEFAULT_RESTART_DRAIN_TIMEOUT;
    return val;
}

/* ================================================================
 *  WhatsApp identity normalization
 *  Port of Python gateway/whatsapp_identity.py.
 * ================================================================ */

/* Strip WhatsApp JID/LID syntax down to its stable numeric identifier.
 * Removes + prefix, @domain, :device suffixes.
 * Port of Python gateway/whatsapp_identity.py normalize_whatsapp_identifier().
 * AG26: Port of Python gateway/whatsapp_identity.py:normalize_whatsapp_identifier().
 * Caller must free the returned string. */
char *normalize_whatsapp_identifier(const char *value) {
    if (!value) return strdup("");
    /* Copy and strip */
    size_t len = strlen(value);
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    const char *src = value;
    char *dst = buf;
    /* Strip leading + */
    if (*src == '+') src++;
    while (*src) {
        if (*src == '@' || *src == ':') break;
        *dst++ = *src++;
    }
    *dst = '\0';
    /* Trim trailing whitespace */
    while (dst > buf && (*(dst-1) == ' ' || *(dst-1) == '\t')) dst--;
    *dst = '\0';
    return buf;
}

/* Check if a string is a safe WhatsApp identifier (alphanumeric, @, ., +, -).
 * Port of Python gateway/whatsapp_identity.py _SAFE_IDENTIFIER_RE. */
static bool is_safe_identifier(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '@' && *p != '.' && *p != '+' && *p != '-')
            return false;
    }
    return true;
}

/* Resolve WhatsApp phone/LID aliases by reading lid-mapping-*.json files.
 * Port of Python gateway/whatsapp_identity.py expand_whatsapp_aliases().
 * AG26: Port of Python gateway/whatsapp_identity.py:expand_whatsapp_aliases().
 * Returns a malloc'd JSON array of alias strings, or NULL.
 * Caller must free with json_free(). */
json_node_t *expand_whatsapp_aliases(const char *identifier) {
    char *norm = normalize_whatsapp_identifier(identifier);
    if (!norm || !*norm) { free(norm); return NULL; }
    if (!is_safe_identifier(norm)) { free(norm); return NULL; }

    json_node_t *result = json_new_array();
    if (!result) { free(norm); return NULL; }

    /* Add normalized identifier first */
    json_array_append(result, json_new_string(norm));

    /* Try to read lid-mapping files */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        char path[1024];
        const char *suffixes[] = {"", "_reverse", NULL};
        for (int si = 0; suffixes[si]; si++) {
            snprintf(path, sizeof(path), "%s/whatsapp/session/lid-mapping-%s%s.json",
                     home, norm, suffixes[si]);
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            FILE *f = fopen(path, "r");
            if (!f) continue;
            char *buf = malloc((size_t)st.st_size + 1);
            if (!buf) { fclose(f); continue; }
            size_t n = fread(buf, 1, (size_t)st.st_size, f);
            fclose(f);
            buf[n] = '\0';
            char *jerr = NULL;
            json_node_t *mapped = json_parse(buf, &jerr);
            free(buf); free(jerr);
            if (mapped && mapped->type == JSON_STRING) {
                char *alias = normalize_whatsapp_identifier(mapped->str_val);
                if (alias && *alias) {
                    /* Check if already in result */
                    bool found = false;
                    for (size_t i = 0; i < json_len(result); i++) {
                        json_t *e = json_get(result, (int)i);
                        if (e && e->type == JSON_STRING && strcmp(e->str_val, alias) == 0) {
                            found = true; break;
                        }
                    }
                    if (!found) json_array_append(result, json_new_string(alias));
                }
                free(alias);
            }
            if (mapped) json_free(mapped);
        }
    }

    free(norm);
    return result;
}

/* Return the canonical WhatsApp identity (shortest alias).
 * Port of Python gateway/whatsapp_identity.py canonical_whatsapp_identifier().
 * AG26: Port of Python gateway/whatsapp_identity.py:canonical_whatsapp_identifier().
 * Caller must free the returned string. */
char *canonical_whatsapp_identifier(const char *identifier) {
    char *norm = normalize_whatsapp_identifier(identifier);
    if (!norm || !*norm) return norm;

    json_node_t *aliases = expand_whatsapp_aliases(norm);
    if (!aliases) return norm; /* Caller owns norm */

    size_t count = json_len(aliases);
    const char *best = NULL;
    size_t best_len = 0;
    for (size_t i = 0; i < count; i++) {
        json_t *e = json_get(aliases, (int)i);
        const char *candidate = (e && e->type == JSON_STRING) ? e->str_val : NULL;
        if (!candidate) continue;
        size_t clen = strlen(candidate);
        if (!best || clen < best_len || (clen == best_len && strcmp(candidate, best) < 0)) {
            best = candidate;
            best_len = clen;
        }
    }

    char *result;
    if (best) {
        result = strdup(best);
    } else {
        result = strdup(norm);
    }
    free(norm);
    json_free(aliases);
    return result;
}
