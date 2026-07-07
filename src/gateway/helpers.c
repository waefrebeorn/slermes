/*
 * helpers.c — Shared gateway helper utilities.
 * Port of Python gateway/platforms/helpers.py.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
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

/* ================================================================
 *  Session context helpers
 *  Port of Python gateway/session_context.py.
 * ================================================================ */

/* Set a session context variable by setting both the env var and a thread-local.
 * Port of Python gateway/session_context.py set_current_session_id(). */
static pthread_key_t g_session_key;
static pthread_once_t g_session_once = PTHREAD_ONCE_INIT;

static void session_key_destructor(void *value) {
    free(value);
}

static void session_key_init(void) {
    pthread_key_create(&g_session_key, session_key_destructor);
}

/* Set the active session ID in both env and thread-local storage.
 * Port of Python gateway/session_context.py set_current_session_id().
 * AG26: Port of Python gateway/session_context.py:set_current_session_id().
 */
void set_current_session_id(const char *session_id) {
    if (!session_id) return;
    pthread_once(&g_session_once, session_key_init);
    setenv("HERMES_SESSION_ID", session_id, 1);
    char *old = pthread_getspecific(g_session_key);
    free(old);
    pthread_setspecific(g_session_key, strdup(session_id));
}

/* Set all session context variables.
 * Port of Python gateway/session_context.py set_session_vars().
 * AG26: Port of Python gateway/session_context.py:set_session_vars().
 */
void set_session_vars(const char *platform, const char *chat_id,
                       const char *chat_name, const char *thread_id,
                       const char *user_id, const char *user_name,
                       const char *session_key) {
    if (platform && *platform) setenv("HERMES_SESSION_PLATFORM", platform, 1);
    if (chat_id && *chat_id) setenv("HERMES_SESSION_CHAT_ID", chat_id, 1);
    if (chat_name && *chat_name) setenv("HERMES_SESSION_CHAT_NAME", chat_name, 1);
    if (thread_id && *thread_id) setenv("HERMES_SESSION_THREAD_ID", thread_id, 1);
    if (user_id && *user_id) setenv("HERMES_SESSION_USER_ID", user_id, 1);
    if (user_name && *user_name) setenv("HERMES_SESSION_USER_NAME", user_name, 1);
    if (session_key && *session_key) setenv("HERMES_SESSION_KEY", session_key, 1);
}

/* Clear all session context variables.
 * Port of Python gateway/session_context.py clear_session_vars().
 * AG26: Port of Python gateway/session_context.py:clear_session_vars().
 */
void clear_session_vars(void) {
    unsetenv("HERMES_SESSION_PLATFORM");
    unsetenv("HERMES_SESSION_CHAT_ID");
    unsetenv("HERMES_SESSION_CHAT_NAME");
    unsetenv("HERMES_SESSION_THREAD_ID");
    unsetenv("HERMES_SESSION_USER_ID");
    unsetenv("HERMES_SESSION_USER_NAME");
    unsetenv("HERMES_SESSION_KEY");
    unsetenv("HERMES_SESSION_MESSAGE_ID");
    pthread_once(&g_session_once, session_key_init);
    char *old = pthread_getspecific(g_session_key);
    free(old);
    pthread_setspecific(g_session_key, NULL);
}

/* Read a session context variable with env fallback.
 * Port of Python gateway/session_context.py get_session_env().
 * AG26: Port of Python gateway/session_context.py:get_session_env().
 * Returns thread-local value if set, else getenv(), else default.
 * Returns a malloc'd string (caller must free). */
char *get_session_env(const char *name, const char *default_value) {
    if (!name || !*name) return default_value ? strdup(default_value) : strdup("");

    /* Check thread-local for HERMES_SESSION_ID */
    if (strcmp(name, "HERMES_SESSION_ID") == 0) {
        pthread_once(&g_session_once, session_key_init);
        char *val = pthread_getspecific(g_session_key);
        if (val) return strdup(val);
    }

    const char *env = getenv(name);
    if (env) return strdup(env);

    return default_value ? strdup(default_value) : strdup("");
}

/* ================================================================
 *  Process memory monitor
 *  Port of Python gateway/memory_monitor.py.
 *  Reads RSS from /proc/self/statm on Linux.
 * ================================================================ */

/* Get current process RSS in MB by reading /proc/self/statm.
 * Port of Python gateway/memory_monitor.py _get_rss_mb().
 * AG26: Port of Python gateway/memory_monitor.py:_get_rss_mb().
 * Returns RSS in MB, or 0 if unavailable. */
int get_rss_mb(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long page_count = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    if (fscanf(f, "%ld", &page_count) != 1) { fclose(f); return 0; }
    fclose(f);
    if (page_count <= 0 || page_size <= 0) return 0;
    return (int)((page_count * page_size) / (1024 * 1024));
}

/* ================================================================
 *  Channel directory — resolve channel names to IDs
 *  Port of Python gateway/channel_directory.py.
 * ================================================================ */

/* Normalize a channel query: strip #, trim, lowercase.
 * Port of Python gateway/channel_directory.py _normalize_channel_query().
 * AG26: Port of Python gateway/channel_directory.py:_normalize_channel_query().
 */
char *normalize_channel_query(const char *value) {
    if (!value) return strdup("");
    /* Strip leading # */
    const char *s = value;
    while (*s == '#') s++;
    while (*s == ' ') s++;
    char *buf = strdup(s);
    if (!buf) return NULL;
    for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
    return buf;
}

/* Build a session entry ID: chat_id or chat_id:thread_id.
 * Port of Python gateway/channel_directory.py _session_entry_id().
 * AG26: Port of Python gateway/channel_directory.py:_session_entry_id().
 */
char *session_entry_id(const char *chat_id, const char *thread_id) {
    if (!chat_id || !*chat_id) return NULL;
    if (thread_id && *thread_id) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s:%s", chat_id, thread_id);
        return strdup(buf);
    }
    return strdup(chat_id);
}

/* ================================================================
 *  Runtime footer — compact model/context status line
 *  Port of Python gateway/runtime_footer.py.
 * ================================================================ */

/* Collapse $HOME to ~ in a path.
 * Port of Python gateway/runtime_footer.py _home_relative_cwd().
 * AG26: Port of Python gateway/runtime_footer.py:_home_relative_cwd().
 */
char *home_relative_cwd(const char *cwd) {
    if (!cwd || !*cwd) return strdup("");
    const char *home = getenv("HOME");
    if (!home) return strdup(cwd);
    size_t home_len = strlen(home);
    if (strncmp(cwd, home, home_len) == 0) {
        const char *suffix = cwd + home_len;
        if (*suffix == '/' || *suffix == '\0') {
            char buf[1024];
            snprintf(buf, sizeof(buf), "~%s", suffix);
            return strdup(buf);
        }
    }
    return strdup(cwd);
}

/* Drop vendor/ prefix from model name.
 * Port of Python gateway/runtime_footer.py _model_short().
 * AG26: Port of Python gateway/runtime_footer.py:_model_short().
 */
char *model_short(const char *model) {
    if (!model || !*model) return strdup("");
    const char *slash = strrchr(model, '/');
    if (slash) return strdup(slash + 1);
    return strdup(model);
}

/* ================================================================
 *  Delivery helpers — error/silence detection
 *  Port of Python gateway/delivery.py.
 * ================================================================ */

/* Check if a string looks like a Telegram private chat ID (positive integer).
 * Port of Python gateway/delivery.py _looks_like_telegram_private_chat_id().
 * AG26: Port of Python gateway/delivery.py:_looks_like_telegram_private_chat_id().
 */
/* PoP: looks_like_telegram_private_chat_id @ gateway/delivery.py:looks_like_telegram_private_chat_id */
bool looks_like_telegram_private_chat_id(const char *chat_id) {
    if (!chat_id || !*chat_id) return false;
    char *end = NULL;
    long val = strtol(chat_id, &end, 10);
    (void)val;
    return (end && *end == '\0' && val > 0);
}

/* Check if a string looks like an integer.
 * Port of Python gateway/delivery.py _looks_like_int().
 * AG26: Port of Python gateway/delivery.py:_looks_like_int().
 */
bool looks_like_int(const char *value) {
    if (!value || !*value) return false;
    char *end = NULL;
    strtol(value, &end, 10);
    return (end && *end == '\0');
}

/* Check if a delivery result indicates failure.
 * Port of Python gateway/delivery.py _send_result_failed().
 * AG26: Port of Python gateway/delivery.py:_send_result_failed().
 */
bool send_result_failed(const char *result_json) {
    if (!result_json) return true;
    char *jerr = NULL;
    json_node_t *root = json_parse(result_json, &jerr);
    free(jerr);
    if (!root) return true;
    bool failed = false;
    json_node_t *success = json_object_get(root, "success");
    if (success && success->type == JSON_BOOL && !success->bool_val) failed = true;
    json_free(root);
    return failed;
}

/* Check if content is a silence-narration token (no actual reply).
 * Port of Python gateway/delivery.py _is_silence_narration().
 * AG26: Port of Python gateway/delivery.py:_is_silence_narration().
 */
bool is_silence_narration(const char *content) {
    if (!content || !*content) return false;
    size_t len = strlen(content);
    if (len > 64) return false; /* length guard */
    /* Strip whitespace/punctuation wrappers */
    const char *s = content;
    while (*s && (isspace((unsigned char)*s) || *s == '*' || *s == '_' || *s == '`' || *s == '~')) s++;
    if (!*s) return true; /* only wrappers */
    /* Check for silence keywords */
    const char *keywords[] = {"silent", "silence", "no response", "no reply", NULL};
    for (int i = 0; keywords[i]; i++) {
        size_t klen = strlen(keywords[i]);
        if (strncasecmp(s, keywords[i], klen) == 0) {
            const char *after = s + klen;
            while (*after && (isspace((unsigned char)*after) || *after == '.' || *after == ')' || *after == '*' || *after == '_' || *after == '`' || *after == '~')) after++;
            if (!*after) return true;
        }
    }
    return false;
}

/* ================================================================
 *  Display config — resolve display settings
 *  Port of Python gateway/display_config.py.
 * ================================================================ */

/* Normalize a string value.
 * Port of Python gateway/display_config.py _normalise(). */
char *normalise_display_value(const char *value) {
    if (!value) return strdup("");
    char *buf = strdup(value);
    if (!buf) return NULL;
    for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
    return buf;
}

/* ================================================================
 *  Delivery helpers — send result error handling
 *  Port of Python gateway/delivery.py.
 * ================================================================ */

/* Extract error message from a delivery result.
 * Port of Python gateway/delivery.py _send_result_error().
 * AG26: Port of Python gateway/delivery.py:_send_result_error().
 * result_json is a JSON string from a send operation.
 * Returns malloc'd error string or NULL (caller must free). */
char *send_result_error(const char *result_json) {
    if (!result_json || !*result_json) return NULL;
    char *jerr = NULL;
    json_node_t *root = json_parse(result_json, &jerr);
    free(jerr);
    if (!root) return NULL;
    char *error = NULL;
    json_node_t *err_node = json_object_get(root, "error");
    if (err_node && err_node->type == JSON_STRING) {
        error = strdup(err_node->str_val);
    } else {
        json_node_t *desc = json_object_get(root, "description");
        if (desc && desc->type == JSON_STRING)
            error = strdup(desc->str_val);
    }
    json_free(root);
    return error;
}

/* Check if a delivery error is a Telegram thread-not-found failure.
 * Port of Python gateway/delivery.py _is_thread_not_found_delivery_error().
 * AG26: Port of Python gateway/delivery.py:_is_thread_not_found_delivery_error().
 */
bool is_thread_not_found_delivery_error(const char *result_json) {
    char *error = send_result_error(result_json);
    if (!error) return false;
    bool found = (strstr(error, "thread not found") != NULL) ||
                 (strstr(error, "message thread not found") != NULL) ||
                 (strstr(error, "TOPIC_ID_INVALID") != NULL) ||
                 (strstr(error, "chat not found") != NULL);
    free(error);
    return found;
}

/* ================================================================
 *  Display config — resolve_display_setting()
 *  Port of Python gateway/display_config.py resolve_display_setting().
 * ================================================================ */

/* Resolve a display setting with per-platform override support.
 * Resolution order: platform override -> global setting -> default.
 * Returns a char* (caller must free). NULL if fallback was NULL.
 * AG26: Port of Python gateway/display_config.py:resolve_display_setting().
 */
char *resolve_display_setting(json_node_t *user_config,
                               const char *platform_key,
                               const char *setting,
                               const char *fallback) {
    if (!user_config || !setting) return fallback ? strdup(fallback) : NULL;
    json_node_t *display = json_object_get(user_config, "display");
    if (!display || display->type != JSON_OBJECT)
        return fallback ? strdup(fallback) : NULL;
    if (platform_key && *platform_key) {
        json_node_t *platforms = json_object_get(display, "platforms");
        if (platforms && platforms->type == JSON_OBJECT) {
            json_node_t *plat = json_object_get(platforms, platform_key);
            if (plat && plat->type == JSON_OBJECT) {
                json_node_t *val = json_object_get(plat, setting);
                if (val) {
                    if (val->type == JSON_STRING)
                        return normalise_display_value(val->str_val);
                    if (val->type == JSON_BOOL)
                        return strdup(val->bool_val ? "true" : "false");
                    if (val->type == JSON_NUMBER) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%.0f", val->num_val);
                        return strdup(buf);
                    }
                }
            }
        }
    }
    json_node_t *global = json_object_get(display, setting);
    if (global) {
        if (global->type == JSON_STRING)
            return normalise_display_value(global->str_val);
        if (global->type == JSON_BOOL)
            return strdup(global->bool_val ? "true" : "false");
        if (global->type == JSON_NUMBER) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f", global->num_val);
            return strdup(buf);
        }
    }
    return fallback ? strdup(fallback) : NULL;
}

/* ================================================================
 *  Pairing — secure atomic file write
 *  Port of Python gateway/pairing.py _secure_write().
 * ================================================================ */

/* Write data to file with restrictive permissions (owner read/write only).
 * Uses temp-file + atomic rename so readers see complete file only.
 * AG26: Port of Python gateway/pairing.py:_secure_write().
 */
bool secure_write(const char *path, const char *data) {
    if (!path || !data) return false;
    char tmp_path[1060];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.XXXXXX", path);
    int fd = mkstemp(tmp_path);
    if (fd < 0) return false;
    size_t len = strlen(data);
    ssize_t written = write(fd, data, len);
    if (written < 0 || (size_t)written != len) {
        close(fd); unlink(tmp_path); return false;
    }
    fsync(fd); close(fd);
    if (rename(tmp_path, path) != 0) { unlink(tmp_path); return false; }
    chmod(path, 0600);
    return true;
}

/* ================================================================
 *  Memory Monitor — RSS tracking helpers
 *  Port of Python gateway/memory_monitor.py.
 * ================================================================ */

static pthread_t g_memory_monitor_thread;
static bool g_memory_monitor_running = false;
static pthread_mutex_t g_memory_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Log current memory usage via stderr.
 * Port of Python gateway/memory_monitor.py log_memory_usage().
 * AG26: Port of Python gateway/memory_monitor.py:log_memory_usage().
 */
void log_memory_usage(const char *prefix) {
    int rss = get_rss_mb();
    if (rss > 0) {
        fprintf(stderr, "[memory%s] RSS: %d MB\n",
                prefix ? prefix : "", rss);
    }
}

/* Port of Python gateway/memory_monitor.py:_monitor_loop(). */
/* Background thread: polls RSS every interval seconds. */
static void *memory_monitor_loop(void *arg) {
    double interval = *(double *)arg;
    free(arg);
    while (true) {
        pthread_mutex_lock(&g_memory_monitor_mutex);
        bool still_running = g_memory_monitor_running;
        pthread_mutex_unlock(&g_memory_monitor_mutex);
        if (!still_running) break;
        log_memory_usage(NULL);
        struct timespec ts;
        ts.tv_sec = (time_t)interval;
        ts.tv_nsec = (long)((interval - (double)ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    }
    return NULL;
}

/* Start background memory monitoring. Returns true if started.
 * Port of Python start_memory_monitoring().
 * AG26: Port of Python gateway/memory_monitor.py:start_memory_monitoring().
 */
bool start_memory_monitoring(double interval_seconds) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    if (g_memory_monitor_running) { pthread_mutex_unlock(&g_memory_monitor_mutex); return false; }
    g_memory_monitor_running = true;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
    double *interval = malloc(sizeof(double));
    if (!interval) { pthread_mutex_lock(&g_memory_monitor_mutex); g_memory_monitor_running = false; pthread_mutex_unlock(&g_memory_monitor_mutex); return false; }
    *interval = interval_seconds > 0 ? interval_seconds : 300.0;
    if (pthread_create(&g_memory_monitor_thread, NULL, memory_monitor_loop, interval) != 0) {
        free(interval);
        pthread_mutex_lock(&g_memory_monitor_mutex); g_memory_monitor_running = false; pthread_mutex_unlock(&g_memory_monitor_mutex);
        return false;
    }
    pthread_detach(g_memory_monitor_thread);
    return true;
}

/* Port of Python gateway/memory_monitor.py:stop_memory_monitoring(). */
/* Stop background memory monitoring. */
void stop_memory_monitoring(void) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    g_memory_monitor_running = false;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
}

/* Check if memory monitoring is running. */
bool is_memory_monitoring_running(void) {
    pthread_mutex_lock(&g_memory_monitor_mutex);
    bool running = g_memory_monitor_running;
    pthread_mutex_unlock(&g_memory_monitor_mutex);
    return running;
}

/* ================================================================
 *  Auto-continue and timestamp helpers
 *  Port of Python gateway/run.py:_home_target_env_var, _float_env,
 *  _is_fresh_gateway_interruption, etc.
 * ================================================================ */

/* Port of Python gateway/run.py:_home_target_env_var
 * Build the home-target env var name from a platform name.
 * e.g. "telegram" -> "TELEGRAM_HOME_CHANNEL" */
char *resolve_home_target_env(const char *platform_name) {
    if (!platform_name || !*platform_name) return NULL;
    size_t plen = strlen(platform_name);
    char *result = malloc(plen + 20);  /* _HOME_CHANNEL + NUL */
    if (!result) return NULL;
    for (size_t i = 0; i < plen; i++)
        result[i] = (char)toupper((unsigned char)platform_name[i]);
    memcpy(result + plen, "_HOME_CHANNEL", 14);
    result[plen + 13] = '\0';
    return result;
}

/* Port of Python gateway/run.py:_home_thread_env_var
 * Build the home-thread env var name: e.g. "TELEGRAM_HOME_CHANNEL_THREAD_ID" */
char *resolve_home_thread_env(const char *platform_name) {
    if (!platform_name || !*platform_name) return NULL;
    size_t plen = strlen(platform_name);
    char *result = malloc(plen + 27);  /* _HOME_CHANNEL_THREAD_ID + NUL */
    if (!result) return NULL;
    for (size_t i = 0; i < plen; i++)
        result[i] = (char)toupper((unsigned char)platform_name[i]);
    memcpy(result + plen, "_HOME_CHANNEL_THREAD_ID", 24);
    result[plen + 23] = '\0';
    return result;
}

/* Port of Python gateway/run.py:_float_env
 * Read an env var as float, falling back to default on typos/empty. */
double read_float_env(const char *name, double default_val) {
    if (!name) return default_val;
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || *end != '\0') return default_val;  /* parse failure */
    return val;
}

/* Port of Python gateway/run.py:_is_fresh_gateway_interruption
 * Return true when an interruption marker is fresh enough to auto-continue.
 * window_secs <= 0 disables the gate. */
bool is_fresh_gateway_interruption(double timestamp, double now, double window_secs) {
    if (window_secs <= 0.0) return true;
    return (now - timestamp) <= window_secs;
}

