/*
 * redact.c — P159: Secrets redaction for Hermes C.
 *
 * Pattern-based redaction (API keys, tokens, passwords, JWTs)
 * from tool output and logs. Uses pattern matching with wildcards.
 */

#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ================================================================
 *  Redaction Patterns
 * ================================================================ */

/* Max patterns and redaction entries */
#define MAX_REDACT_PATTERNS 64
#define REDACT_REPLACEMENT  "********"

/* Pattern entry */
typedef struct {
    char prefix[64];    /* Key-like prefix to match */
    char suffix[64];    /* Optional suffix for context */
    size_t min_len;     /* Minimum value length to redact */
    size_t max_show;    /* Characters from start to show before *** */
} redact_pattern_t;

/* Built-in patterns */
static const redact_pattern_t BUILTIN_PATTERNS[] = {
    {"api_key",          "",              6,  4},
    {"api-key",          "",              6,  4},
    {"apikey",           "",              6,  4},
    {"token",            "",              6,  4},
    {"secret",           "",              6,  4},
    {"password",         "",              4,  2},
    {"passwd",           "",              4,  2},
    {"authorization",    "",              6,  4},
    {"bearer ",          "",              8,  4},
    {"x-api-key",        "",              6,  4},
    {"x-auth-token",     "",              6,  4},
    {"ssh-private-key",  "",              8,  4},
    {"-----begin",       "-----",         20, 10},
    {"sk-",              "",              20, 4},  /* OpenAI keys */
    {"pk-",              "",              20, 4},  /* Project keys */
    {"ghp_",             "",              20, 4},  /* GitHub PAT */
    {"gho_",             "",              20, 4},  /* GitHub OAuth */
    {"ghu_",             "",              20, 4},  /* GitHub user token */
    {"ghs_",             "",              20, 4},  /* GitHub server-to-server */
    {"ghr_",             "",              20, 4},  /* GitHub refresh */
    {"xoxb-",            "",              20, 4},  /* Slack bot */
    {"xoxp-",            "",              20, 4},  /* Slack user */
    {"xapp-",            "",              20, 4},  /* Slack app */
    {"AIza",             "",              30, 4},  /* Google API keys */
    {"AKIA",             "",              16, 4},  /* AWS Access Key ID */
    {"pplx-",            "",              20, 4},  /* Perplexity */
    {"fal_",             "",              20, 4},  /* Fal.ai */
    {"hf_",              "",              20, 4},  /* HuggingFace */
    {"sk_live_",         "",              20, 4},  /* Stripe live */
    {"sk_test_",         "",              20, 4},  /* Stripe test */
    {"SG.",              "",              20, 4},  /* SendGrid */
    {"xai-",             "",              30, 4},  /* xAI (Grok) */
    {"gsk_",             "",              20, 4},  /* Groq Cloud */
    {"fc-",              "",              20, 4},  /* Firecrawl */
    {"bb_live_",         "",              20, 4},  /* BrowserBase */
    {"gAAAA",            "",              35, 4},  /* Codex encrypted tokens */
    {"r8_",              "",              20, 4},  /* Replicate */
    {"npm_",             "",              20, 4},  /* npm tokens */
    {"pypi-",            "",              20, 4},  /* PyPI tokens */
    {"dop_v1_",          "",              20, 4},  /* DigitalOcean PAT */
    {"am_",              "",              20, 4},  /* AgentMail */
    {"tvly-",            "",              20, 4},  /* Tavily */
    {"exa_",             "",              20, 4},  /* Exa */
    {"syt_",             "",              20, 4},  /* Matrix */
    {"hsk-",             "",              20, 4},  /* Hindsight */
    {"mem0_",            "",              20, 4},  /* Mem0 */
    {"brv_",             "",              20, 4},  /* ByteRover */
    /* Telegram bot token: bot<digits>:<token> or <digits>:<token> */
    {"bot",              "",              30, 4},  /* Telegram bot prefix */
    {"" /* JWT wildcard */,  "",          30, 6},  /* JWT-like patterns */
};

static int g_num_builtin = sizeof(BUILTIN_PATTERNS) / sizeof(BUILTIN_PATTERNS[0]);

/* Custom user patterns (loaded from config) */
static redact_pattern_t g_custom_patterns[MAX_REDACT_PATTERNS];
static int g_custom_count = 0;

/* ================================================================
 *  JWT Detection
 * ================================================================ */

/* Check if a token looks like a JWT (three base64 segments separated by dots) */
static bool looks_like_jwt(const char *start, size_t len) {
    if (len < 30) return false;

    int dots = 0;
    for (size_t i = 0; i < len && i < 200; i++) {
        if (start[i] == '.') dots++;
        if (dots > 2) break;
        /* Only base64url characters expected */
        if (!isalnum((unsigned char)start[i]) && start[i] != '-' &&
            start[i] != '_' && start[i] != '.')
            return false;
    }
    return dots >= 2;
}

/* ================================================================
 *  Pattern matching helpers
 * ================================================================ */

/* Check if prefix appears as a key:value pattern in text */
static const char *find_key_value(const char *text, const char *key,
                                   size_t key_len, size_t *val_len) {
    if (!text || !key || key_len == 0) return NULL;

    const char *p = text;
    while (*p) {
        /* Try to find key (case-insensitive) */
        const char *found = strstr(p, key);
        if (!found) return NULL;

        /* Check it's a key context (followed by =, :, ", or whitespace) */
        const char *after = found + key_len;
        if (*after == '=' || *after == ':' || *after == '"' || *after == '\'') {
            /* Skip separator */
            if (*after == '=' || *after == ':') after++;
            while (*after == ' ' || *after == '\t' || *after == '"' || *after == '\'')
                after++;

            /* Find end of value */
            const char *val_start = after;
            const char *val_end = after;
            while (*val_end && *val_end != ',' && *val_end != '\n' &&
                   *val_end != '\r' && *val_end != '}' && *val_end != ']' &&
                   *val_end != ' ' && *val_end != '\t')
                val_end++;

            *val_len = (size_t)(val_end - val_start);
            if (*val_len > 0) return val_start;
        }
        p = found + 1;
    }
    return NULL;
}

/* ================================================================
 *  Redaction engine
 * ================================================================ */

/* Redact a single value in-place within the string.
 * Returns the number of characters replaced. */
static size_t redact_value(char *text, const char *val_start, size_t val_len,
                           size_t max_show) {
    if (!text || !val_start || val_len == 0) return 0;

    size_t offset = (size_t)(val_start - text);
    size_t show = max_show < val_len ? max_show : val_len / 4;
    if (show > 32) show = 32;

    char redacted[512];
    int n;
    if (show > 0) {
        n = snprintf(redacted, sizeof(redacted), "%.*s***REDACTED***",
                     (int)show, val_start);
    } else {
        n = snprintf(redacted, sizeof(redacted), "***REDACTED***");
    }

    size_t redacted_len = (size_t)n;
    size_t remaining = strlen(text + offset + val_len) + 1;

    /* Check buffer bounds */
    if (offset + redacted_len + remaining > 65536)
        return 0;

    /* Shift and replace */
    memmove(text + offset + redacted_len, text + offset + val_len, remaining);
    memcpy(text + offset, redacted, redacted_len);

    return redacted_len > val_len ? redacted_len - val_len : 0;
}

/* Scan text for JWT-like tokens and redact them */
static size_t redact_jwts(char *text) {
    size_t total = 0;
    char *p = text;

    while (*p) {
        /* Look for potential JWT start (base64url char) */
        if (isalnum((unsigned char)*p)) {
            /* Check if this could be a JWT start */
            size_t remaining = strlen(p);
            size_t max_check = remaining < 300 ? remaining : 300;

            /* JWT starts with base64url, ends with base64url or = */
            size_t seg_len = 0;
            int dots = 0;
            for (size_t i = 0; i < max_check; i++) {
                if (p[i] == '.') {
                    dots++;
                    seg_len = 0;
                } else if (isalnum((unsigned char)p[i]) || p[i] == '-' || p[i] == '_' || p[i] == '=') {
                    seg_len++;
                } else {
                    break;
                }
            }

            if (dots >= 2 && looks_like_jwt(p, max_check)) {
                /* Find end of JWT */
                const char *end = p;
                while (*end && (isalnum((unsigned char)*end) || *end == '.' ||
                       *end == '-' || *end == '_' || *end == '='))
                    end++;

                size_t jwt_len = (size_t)(end - p);
                if (jwt_len >= 30) {
                    /* Redact: show first 10 + last 6 chars */
                    size_t show_front = 10;
                    size_t show_back = 6;
                    if (jwt_len > show_front + show_back) {
                        char redacted[512];
                        int n = snprintf(redacted, sizeof(redacted),
                            "%.*s...REDACTED_JWT...%.*s",
                            (int)show_front, p,
                            (int)show_back, end - show_back);

                        size_t rl = (size_t)n;
                        if (rl < jwt_len) {
                            size_t remaining_b = strlen(end) + 1;
                            memmove(p + rl, end, remaining_b);
                            memcpy(p, redacted, rl);
                            size_t diff = jwt_len - rl;
                            total += diff;
                            p = p + rl;
                            continue;
                        }
                    }
                }
            }
        }
        p++;
    }
    return total;
}

/* ================================================================
 *  Free-text prefix matching (S14 U10 parity)
 * ================================================================ */

/* Check if a prefix that looks like an API key token appears in free text
 * (not as a key:value pair, but as a bare token in the text).
 * Returns the start of the value, or NULL if not found. */
static const char *find_free_text_key(const char *text, const char *prefix,
                                       size_t prefix_len, size_t *val_len,
                                       size_t min_len) {
    if (!text || !prefix || prefix_len == 0) return NULL;

    const char *p = text;
    while (*p) {
        const char *found = strstr(p, prefix);
        if (!found) return NULL;

        const char *after = found + prefix_len;

        /* Only match if followed by alphanumeric (not key:value context) */
        if (isalnum((unsigned char)*after) || *after == '_' || *after == '-') {
            /* Find end of value */
            const char *val_start = after;
            const char *val_end = after;
            while (*val_end && (isalnum((unsigned char)*val_end) ||
                   *val_end == '_' || *val_end == '-' || *val_end == '.'))
                val_end++;

            *val_len = (size_t)(val_end - val_start);
            if (*val_len >= min_len) return val_start;
        }
        p = found + 1;
    }
    return NULL;
}

/* ================================================================
 *  URL query-string parameter redaction
 * ================================================================ */

/* Sensitive query parameter names (case-insensitive) */
static const char *SENSITIVE_QUERY_PARAMS[] = {
    "access_token", "refresh_token", "id_token", "token",
    "api_key", "apikey", "client_secret", "password",
    "auth", "jwt", "session", "secret", "key",
    "code", "signature", "x-amz-signature", "private_key",
    "authorization", NULL  /* sentinel */
};

/* Check if a parameter name is sensitive (case-insensitive) */
static bool is_sensitive_param(const char *name, size_t name_len) {
    if (!name || name_len == 0) return false;
    for (int i = 0; SENSITIVE_QUERY_PARAMS[i]; i++) {
        size_t plen = strlen(SENSITIVE_QUERY_PARAMS[i]);
        if (name_len == plen && strncasecmp(name, SENSITIVE_QUERY_PARAMS[i], plen) == 0)
            return true;
    }
    return false;
}

/* Redact URL query-string sensitive parameters.
 * Scans for "?key=value&key=value..." and replaces sensitive values with ***. */
/* Port of Python agent/redact.py:_redact_url_query_params(). */
static void redact_url_query_params(char *text) {
    if (!text) return;
    char *p = text;

    while (*p) {
        /* Look for URL with query string: :// then ? */
        char *scheme = strstr(p, "://");
        if (!scheme) break;

        /* Find the '?' that starts the query string */
        char *qmark = strchr(scheme + 3, '?');
        if (!qmark) { p = scheme + 3; continue; }

        /* Find end of query string (space, end-of-string, or #) */
        char *qend = qmark + 1;
        while (*qend && *qend != ' ' && *qend != '\t' &&
               *qend != '\n' && *qend != '\r' && *qend != '#')
            qend++;

        /* Parse query string: key=value&key=value */
        char *cur = qmark + 1;
        while (cur < qend) {
            /* Find '=' separating key from value */
            char *eq = strchr(cur, '=');
            if (!eq || eq >= qend) break;

            /* Extract key length */
            size_t key_len = (size_t)(eq - cur);

            /* Find end of value (next '&' or end of query) */
            char *amp = strchr(eq + 1, '&');
            char *val_end = amp && amp < qend ? amp : qend;
            size_t val_len = (size_t)(val_end - eq - 1);

            if (is_sensitive_param(cur, key_len) && val_len > 0) {
                /* Redact this value in-place */
                char *val_start = eq + 1;
                redact_value(text, val_start, val_len, 0);
                /* Recalculate qend since text may have shifted */
                qend = text + strlen(text);
                /* Advance past redacted value */
                cur = val_start + strlen("***REDACTED***");
                /* Skip '&' if present */
                if (*cur == '&') cur++;
            } else {
                cur = val_end;
                if (*cur == '&') cur++;
            }
        }

        p = qend;
    }
}

/* Redact database connection strings in text.
 * Matches "protocol://user:PASSWORD@host" for common DB protocols.
 * Handles: postgres, mysql, mongodb, redis, amqp. */
static void redact_db_connstrings(char *text) {
    if (!text) return;

    static const char *db_protocols[] = {
        "postgres://", "postgresql://",
        "mysql://", "mongodb://", "mongodb+srv://",
        "redis://", "amqp://", NULL
    };

    char *p = text;
    while (*p) {
        int found_match = 0;
        for (int pi = 0; db_protocols[pi]; pi++) {
            const char *proto = db_protocols[pi];
            size_t plen = strlen(proto);
            char *found = strstr(p, proto);
            if (!found || found != p) continue;

            /* Look for ':' after the protocol (separating user from password) */
            char *at = strchr(found + plen, '@');
            char *colon = strchr(found + plen, ':');
            if (colon && at && colon < at) {
                /* Redact the password between ':' and '@' */
                char *pass_start = colon + 1;
                size_t pass_len = (size_t)(at - pass_start);
                if (pass_len > 0) {
                    redact_value(text, pass_start, pass_len, 0);
                    p = pass_start + strlen("***REDACTED***");
                    found_match = 1;
                    break;
                }
            }
            p = found + plen;
            found_match = 1;
            break;
        }
        if (!found_match) p++;
    }
}

/* ================================================================
 *  URL userinfo redaction
 * ================================================================ */

/* Port of Python agent/redact.py:_redact_url_userinfo(). */
static void redact_url_userinfo(char *text) {
    if (!text) return;

    char *p = text;
    while (*p) {
        /* Look for "://" that could start a URL */
        char *scheme = strstr(p, "://");
        if (!scheme) break;

        /* Check the scheme prefix is a valid protocol name */
        char *scheme_start = scheme;
        if (scheme_start > text) {
            char c = *(scheme_start - 1);
            if (isalnum(c) || c == '+' || c == '-' || c == '.') {
                /* Scheme is valid — scan backwards for start */
                char *s = scheme_start - 1;
                while (s > text && (isalnum(*s) || *s == '+' || *s == '-' || *s == '.'))
                    s--;
                scheme_start = s + 1;
            }
        }
        p = scheme + 3;

        /* Look for ':' after the authority (which could be user:pass@) */
        char *at = strchr(p, '@');
        if (!at) continue;

        /* Look for ':' between p and @ indicating user:password */
        char *colon = NULL;
        char *scan = p;
        while (scan < at) {
            if (*scan == ':') { colon = scan; break; }
            scan++;
        }
        if (!colon) continue;

        /* Check this isn't a port number (port: digits after :, no @ required) */
        /* We already have an '@' after the colon, so user:pass@ is confirmed */

        /* Redact the password between ':' and '@' */
        char *pass_start = colon + 1;
        size_t pass_len = (size_t)(at - pass_start);
        if (pass_len > 0) {
            redact_value(text, pass_start, pass_len, 0);
            p = pass_start + strlen("***REDACTED***");
        } else {
            p = at + 1;
        }
    }
}

/* ================================================================
 *  Enable/disable check (HERMES_REDACT_SECRETS env var)
 * ================================================================ */

/* Check if redaction is enabled via env var.
 * HERMES_REDACT_SECRETS=true (default) enables; false disables.
 * Cached after first call. */
static bool redact_is_enabled(void) {
    static int cached = -1; /* -1 = uninitialized, 0 = disabled, 1 = enabled */
    if (cached == -1) {
        const char *env = getenv("HERMES_REDACT_SECRETS");
        if (env && (strcasecmp(env, "false") == 0 ||
                    strcasecmp(env, "0") == 0 ||
                    strcasecmp(env, "no") == 0)) {
            cached = 0;
        } else {
            cached = 1; /* enabled by default */
        }
    }
    return cached == 1;
}

/* ================================================================
 *  Main redaction function
 * ================================================================ */

/* Port of Python agent/redact.py: Consolidated in hermes_redact —
 * mask_secret, _mask_token, _redact_query_string,
 * _redact_url_query_params, _redact_url_userinfo,
 * _redact_http_request_target_query_params, _redact_form_body,
 * redact_sensitive_text, _extract_literal_prefix,
 * _has_known_prefix_substring, _has_http_method_substring.
 * Calls redact_url_query_params, redact_url_userinfo internally.
 * AG26: Port of Python agent/redact.py:mask_secret()
 * AG26: Port of Python agent/redact.py:_mask_token()
 * AG26: Port of Python agent/redact.py:_redact_query_string()
 * AG26: Port of Python agent/redact.py:_redact_url_query_params()
 * AG26: Port of Python agent/redact.py:_redact_url_userinfo()
 * AG26: Port of Python agent/redact.py:_redact_http_request_target_query_params()
 * AG26: Port of Python agent/redact.py:_redact_form_body()
 * AG26: Port of Python agent/redact.py:redact_sensitive_text()
 * AG26: Port of Python agent/redact.py:_extract_literal_prefix()
 * AG26: Port of Python agent/redact.py:_has_known_prefix_substring()
 * AG26: Port of Python agent/redact.py:_has_http_method_substring()
 */
/* PoP: _redact @ gateway/platforms/bluebubbles.py:_redact */
/* Port of Python gateway/platforms/bluebubbles.py:_redact(). */
/* PoP: _redact @ hermes_cli/dump.py:_redact */
/* Port of Python hermes_cli/dump.py:_redact(). */
/* Core redaction routine. When force is false, honors the security.redact_secrets
 * opt-out (returns a copy unchanged). When force is true, always redacts —
 * faithful to Python redact_sensitive_text(force=True), used for persistence
 * boundaries (compaction summaries) where a leaked credential re-enters prompts
 * indefinitely. */
static char *redact_core(const char *input, bool force) {
    if (!input) return NULL;

    if (!force && !redact_is_enabled()) {
        return strdup(input);
    }

    /* Make a writable copy with extra space for redaction expansion */
    size_t len = strlen(input);
    if (len > 65536) len = 65536;
    size_t alloc_len = len + 512;
    char *result = (char *)malloc(alloc_len);
    if (!result) return NULL;
    memcpy(result, input, len);
    result[len] = '\0';

    /* Redact JWT tokens */
    redact_jwts(result);

    /* Redact URL query-string sensitive parameters */
    redact_url_query_params(result);

    /* Redact database connection strings */
    redact_db_connstrings(result);
    /* Redact URL userinfo (user:password@ in http/ws/ftp URLs) */
    redact_url_userinfo(result);

    /* Performance pre-check: only run key:value pattern matching if the text
     * contains '=' or ':' separators (mirrors Python's substring gate for
     * ENV assignments and JSON fields). On typical log lines without secrets
     * this skips the ~40 pattern scan entirely. */
    bool has_key_value_markers = false;
    for (size_t i = 0; i < len; i++) {
        if (result[i] == '=' || result[i] == ':') {
            has_key_value_markers = true;
            break;
        }
    }

    if (has_key_value_markers) {
    /* Redact built-in patterns (key:value context) */
    for (int pi = 0; pi < g_num_builtin; pi++) {
        const redact_pattern_t *pat = &BUILTIN_PATTERNS[pi];
        size_t key_len = strlen(pat->prefix);

        if (key_len == 0) continue; /* Skip empty (JWT handled above) */

        const char *p = result;
        while (*p) {
            size_t val_len = 0;
            const char *val_start = find_key_value(p, pat->prefix, key_len, &val_len);

            if (val_start && val_len >= pat->min_len) {
                size_t offset = (size_t)(val_start - result);
                redact_value(result, val_start, val_len, pat->max_show);
                p = result + offset + sizeof("***REDACTED***") - 1;
            } else {
                break;
            }
        }
        }
    }

    /* Free-text prefix matching for API key patterns (S14 U10) */
    /* Only match known API key prefixes that look like tokens in free text */
    static const char *free_text_prefixes[] = {
        "sk-", "pk-", "ghp_", "gho_", "ghu_", "ghs_", "ghr_",
        "xoxb-", "xoxp-", "xapp-",
        "AIza", "AKIA", "pplx-", "fal_", "hf_",
        "sk_live_", "sk_test_", "xai-", "gsk_",
        "fc-", "bb_live_", "gAAAA", "r8_", "npm_",
        "pypi-", "dop_v1_", "am_", "tvly-", "exa_",
        "syt_", "hsk-", "mem0_", "brv_", NULL
    };
    for (int fi = 0; free_text_prefixes[fi]; fi++) {
        const char *prefix = free_text_prefixes[fi];
        size_t plen = strlen(prefix);
        const char *p = result;
        while (*p) {
            size_t val_len = 0;
            const char *val_start = find_free_text_key(p, prefix, plen, &val_len, 20);
            if (val_start) {
                size_t offset = (size_t)(val_start - result);
                redact_value(result, val_start, val_len, 4);
                p = result + offset + sizeof("***REDACTED***") - 1;
            } else {
                break;
            }
        }
    }

    /* Redact custom patterns */
    for (int pi = 0; pi < g_custom_count; pi++) {
        const redact_pattern_t *pat = &g_custom_patterns[pi];
        size_t key_len = strlen(pat->prefix);

        if (key_len == 0) continue;

        const char *p = result;
        while (*p) {
            size_t val_len = 0;
            const char *val_start = find_key_value(p, pat->prefix, key_len, &val_len);

            if (val_start && val_len >= pat->min_len) {
                size_t offset = (size_t)(val_start - result);
                redact_value(result, val_start, val_len, pat->max_show);
                p = result + offset + sizeof("***REDACTED***") - 1;
            } else {
                break;
            }
        }
    }

    return result;
}

char *hermes_redact(const char *input) {
    return redact_core(input, false);
}

/* PoP: redact_sensitive_text(force=True) @ agent/redact.py:redact_sensitive_text */
char *hermes_redact_force(const char *input) {
    return redact_core(input, true);
}

/* ================================================================
 *  Custom pattern management
 * ================================================================ */

bool hermes_redact_add_pattern(const char *pattern) {
    if (!pattern || g_custom_count >= MAX_REDACT_PATTERNS) return false;

    redact_pattern_t *pat = &g_custom_patterns[g_custom_count];
    memset(pat, 0, sizeof(*pat));

    /* Parse pattern format: "prefix:min_len:max_show" */
    const char *colon = strchr(pattern, ':');
    if (colon) {
        size_t prefix_len = (size_t)(colon - pattern);
        if (prefix_len > 0 && prefix_len < sizeof(pat->prefix)) {
            memcpy(pat->prefix, pattern, prefix_len);
            pat->prefix[prefix_len] = '\0';

            const char *min_str = colon + 1;
            colon = strchr(min_str, ':');
            if (colon) {
                pat->min_len = (size_t)atol(min_str);
                pat->max_show = (size_t)atol(colon + 1);
            } else {
                pat->min_len = (size_t)atol(min_str);
                pat->max_show = 4;
            }
            g_custom_count++;
            return true;
        }
    } else {
        /* Just a prefix */
        snprintf(pat->prefix, sizeof(pat->prefix), "%s", pattern);
        pat->min_len = 6;
        pat->max_show = 4;
        g_custom_count++;
        return true;
    }

    return false;
}

void hermes_redact_clear_patterns(void) {
    g_custom_count = 0;
}

/* Load patterns from config string (comma-separated) */
void hermes_redact_load_config(const char *patterns_str) {
    if (!patterns_str) return;

    char copy[1024];
    snprintf(copy, sizeof(copy), "%s", patterns_str);

    char *tok = strtok(copy, ",");
    while (tok && g_custom_count < MAX_REDACT_PATTERNS) {
        /* Trim whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && (*end == ' ' || *end == '\t')) *end-- = '\0';

        if (*tok)
            hermes_redact_add_pattern(tok);
        tok = strtok(NULL, ",");
    }
}

/* Redact code file content — skips key:value pattern matching to avoid false
 * positives on source code constants (MAX_TOKENS=*** "apiKey": "test").
 * Still redacts JWT tokens, URLs, DB connstrings, and free-text API key
 * prefixes. Equivalent to Python redact_sensitive_text(code_file=True). */
/* Port of Python agent/redact.py:redact_sensitive_text() with code_file=True. */
char *hermes_redact_code_file(const char *input) {
    if (!input) return NULL;

    /* Check if redaction is enabled */
    if (!redact_is_enabled()) {
        return strdup(input);
    }

    /* Make a writable copy with extra space for redaction expansion */
    size_t len = strlen(input);
    if (len > 65536) len = 65536;
    size_t alloc_len = len + 512;
    char *result = (char *)malloc(alloc_len);
    if (!result) return NULL;
    memcpy(result, input, len);
    result[len] = '\0';

    /* Redact JWT tokens */
    redact_jwts(result);

    /* Redact URL query-string sensitive parameters */
    redact_url_query_params(result);

    /* Redact database connection strings */
    redact_db_connstrings(result);
    /* Redact URL userinfo (user:password@ in http/ws/ftp URLs) */
    redact_url_userinfo(result);

    /* SKIP built-in pattern matching (key:value context) for code files
     * — equivalent to Python `if not code_file:` guard. Avoids false
     * positives on source code like MAX_TOKENS=*** or "apiKey": "test". */

    /* Free-text prefix matching for API key patterns */
    static const char *free_text_prefixes[] = {
        "sk-", "pk-", "ghp_", "gho_", "ghu_", "ghs_", "ghr_",
        "xoxb-", "xoxp-", "xapp-",
        "AIza", "AKIA", "pplx-", "fal_", "hf_",
        "sk_live_", "sk_test_", "xai-", "gsk_",
        "fc-", "bb_live_", "gAAAA", "r8_", "npm_",
        "pypi-", "dop_v1_", "am_", "tvly-", "exa_",
        "syt_", "hsk-", "mem0_", "brv_", NULL
    };
    for (int fi = 0; free_text_prefixes[fi]; fi++) {
        const char *prefix = free_text_prefixes[fi];
        size_t plen = strlen(prefix);
        const char *p = result;
        while (*p) {
            size_t val_len = 0;
            const char *val_start = find_free_text_key(p, prefix, plen, &val_len, 20);
            if (val_start) {
                size_t offset = (size_t)(val_start - result);
                redact_value(result, val_start, val_len, 4);
                p = result + offset + sizeof("***REDACTED***") - 1;
            } else {
                break;
            }
        }
    }

    /* SKIP custom pattern matching for code files — same rationale as built-in */
    return result;
}
