/*
 * browser_tool_eval.c — focused concern module extracted from
 * port_browser_tool.c (refactor-first monolith split). Port of
 * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.
 */

#include "browser_tool_eval.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include "browser_tool_install.h"
const char *browser_get_current_url(void);

struct browser_tool_eval {
    int unused;
};

browser_tool_eval_t *browser_tool_eval_init(void) { return calloc(1, sizeof(browser_tool_eval_t)); }
void browser_tool_eval_cleanup(browser_tool_eval_t *s) { free(s); }

/* PoP: browser_redact_browser_output @ tools/browser_tool.py:_redact_browser_output */
/* PoP: browser_redact_browser_output @ tools/browser_tool.py:_redact_browser_output */
char *browser_redact_browser_output(const char *value_json)
{
    if (!value_json) {
        char *n = malloc(5);
        if (n) strcpy(n, "null");
        return n;
    }

    /* In C we implement a simplified redact_sensitive_text */
    char *result = malloc(strlen(value_json) + 1);
    if (!result) return NULL;
    strcpy(result, value_json);

    /* Redact common secret patterns */
    const char *patterns[] = {
        "sk-ant-", "sk-", "ghp_", "gho_", "ghu_", "ghs_", "ghr_",
        "Bearer ", "bearer ", "api_key", "apikey", "secret", "token",
        NULL
    };

    for (int i = 0; patterns[i]; i++) {
        char *pos = result;
        while ((pos = strstr(pos, patterns[i]))) {
            size_t plen = strlen(patterns[i]);
            /* Find end of token (whitespace, quote, comma, brace) */
            char *end = pos + plen;
            while (*end && *end != ' ' && *end != '\t' && *end != '\n' && 
                   *end != '"' && *end != '\'' && *end != ',' && *end != '}' && *end != ']') {
                end++;
            }
            size_t token_len = end - (pos + plen);
            if (token_len > 0) {
                memset(pos + plen, '*', token_len);
            }
            pos = end;
        }
    }

    return result;
}

/* PoP: browser_blocked_private_page_action @ tools/browser_tool.py:_blocked_private_page_action */
char *browser_blocked_private_page_action(const char *effective_task_id, const char *action)
{
    if (!browser_eval_ssrf_guard_active(effective_task_id)) {
        return NULL;
    }

    char *blocked_url = browser_current_page_private_url(effective_task_id);
    if (!blocked_url) {
        return NULL;
    }

    char *result = malloc(512);
    if (result) {
        snprintf(result, 512,
            "{\"success\":false,\"error\":\"Blocked: page URL targets a private or internal address (%s). Refusing to %s on this page in this browser mode.\"}",
            blocked_url, action);
    }
    free(blocked_url);
    return result;
}

/* PoP: browser_eval_ssrf_guard_active @ tools/browser_tool.py:_eval_ssrf_guard_active */
/* PoP: browser_eval_ssrf_guard_active @ tools/browser_tool.py:_eval_ssrf_guard_active */
bool browser_eval_ssrf_guard_active(const char *effective_task_id)
{
    return (!browser_is_local_backend() &&
            !browser_is_local_sidecar_key(effective_task_id) &&
            !browser_allow_private_urls());
}

/* PoP: browser_expression_targets_private_url @ tools/browser_tool.py:_expression_targets_private_url */
char *browser_expression_targets_private_url(const char *expression)
{
    if (!expression) return NULL;

    /* Scan for http(s)://... literals in JS expression */
    const char *p = expression;
    while ((p = strstr(p, "http://")) || (p = strstr(p, "https://"))) {
        /* Found a URL literal - extract it */
        const char *start = p;
        p += 4; /* skip "http" */
        if (p[-1] == 's') p++; /* skip 's' if https */
        p += 3; /* skip "://" */

        const char *end = p;
        while (*end && *end != ' ' && *end != '\t' && *end != '\n' && 
               *end != '"' && *end != '\'' && *end != ')' && *end != ']' && 
               *end != '>' && *end != ',' && *end != ';') {
            end++;
        }

        size_t url_len = end - start;
        char *url = malloc(url_len + 1);
        if (url) {
            memcpy(url, start, url_len);
            url[url_len] = '\0';
            /* Strip trailing punctuation */
            while (url_len > 0 && (url[url_len-1] == '.' || url[url_len-1] == ',' || url[url_len-1] == ';')) {
                url[--url_len] = '\0';
            }

            if (browser_is_always_blocked_url(url) || !browser_is_safe_url(url)) {
                return url; /* Caller must free */
            }
            free(url);
        }

        p = end;
    }

    return NULL;
}

/* PoP: browser_current_page_private_url @ tools/browser_tool.py:_current_page_private_url */
/* PoP: browser_current_page_private_url @ tools/browser_tool.py:_current_page_private_url */
char *browser_current_page_private_url(const char *effective_task_id)
{
    (void)effective_task_id;

    /* Try local browser tab first */
    const char *current_url = browser_get_current_url();
    if (current_url && current_url[0]) {
        if (browser_is_always_blocked_url(current_url) || !browser_is_safe_url(current_url)) {
            hermes_log(LOG_DEBUG, "port", "browser_current_page_private_url: found private URL %s", current_url);
            return strdup(current_url);
        }
    }

    return NULL;
}

/* PoP: browser_allow_unsafe_browser_evaluate @ tools/browser_tool.py:_allow_unsafe_browser_evaluate */
bool browser_allow_unsafe_browser_evaluate(void)
{
    /* Check config.yaml for browser.allow_unsafe_evaluate */
    const char *env = getenv("HERMES_BROWSER_ALLOW_UNSAFE_EVALUATE");
    if (env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0)) {
        return true;
    }
    return false;
}

/* PoP: browser_decode_js_string_literal @ tools/browser_tool.py:_decode_js_string_literal */
char *browser_decode_js_string_literal(const char *literal)
{
    if (!literal || strlen(literal) < 2) {
        return literal ? strdup(literal) : NULL;
    }

    /* Remove surrounding quotes */
    char quote = literal[0];
    if (quote != '"' && quote != '\'' && quote != '`') {
        return strdup(literal);
    }

    size_t len = strlen(literal);
    if (len < 2 || literal[len-1] != quote) {
        return strdup(literal);
    }

    const char *body = literal + 1;
    size_t body_len = len - 2;

    char *result = malloc(body_len + 1);
    if (!result) return NULL;

    size_t out_idx = 0;
    for (size_t i = 0; i < body_len; i++) {
        if (body[i] == '\\' && i + 1 < body_len) {
            char next = body[i+1];
            switch (next) {
                case 'n': result[out_idx++] = '\n'; break;
                case 't': result[out_idx++] = '\t'; break;
                case 'r': result[out_idx++] = '\r'; break;
                case 'b': result[out_idx++] = '\b'; break;
                case 'f': result[out_idx++] = '\f'; break;
                case 'v': result[out_idx++] = '\v'; break;
                case '\\': result[out_idx++] = '\\'; break;
                case '"': result[out_idx++] = '"'; break;
                case '\'': result[out_idx++] = '\''; break;
                case '`': result[out_idx++] = '`'; break;
                case 'x': /* \xHH */
                    if (i + 3 < body_len && isxdigit(body[i+2]) && isxdigit(body[i+3])) {
                        char hex[3] = {body[i+2], body[i+3], 0};
                        result[out_idx++] = (char)strtol(hex, NULL, 16);
                        i += 3;
                    } else {
                        result[out_idx++] = body[i];
                    }
                    break;
                case 'u': /* \uHHHH */
                    if (i + 5 < body_len && isxdigit(body[i+2]) && isxdigit(body[i+3]) && 
                        isxdigit(body[i+4]) && isxdigit(body[i+5])) {
                        char hex[5] = {body[i+2], body[i+3], body[i+4], body[i+5], 0};
                        uint32_t cp = strtoul(hex, NULL, 16);
                        /* Simplified: only handle BMP */
                        if (cp <= 0x7F) {
                            result[out_idx++] = (char)cp;
                        } else if (cp <= 0x7FF) {
                            result[out_idx++] = 0xC0 | (cp >> 6);
                            result[out_idx++] = 0x80 | (cp & 0x3F);
                        } else {
                            result[out_idx++] = 0xE0 | (cp >> 12);
                            result[out_idx++] = 0x80 | ((cp >> 6) & 0x3F);
                            result[out_idx++] = 0x80 | (cp & 0x3F);
                        }
                        i += 5;
                    } else {
                        result[out_idx++] = body[i];
                    }
                    break;
                default:
                    result[out_idx++] = body[i];
                    break;
            }
        } else {
            result[out_idx++] = body[i];
        }
    }
    result[out_idx] = '\0';
    return result;
}

/* PoP: browser_decoded_js_string_literals @ tools/browser_tool.py:_decoded_js_string_literals */
char **browser_decoded_js_string_literals(const char *expression, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!expression) return NULL;

    /* Count string literals first */
    int count = 0;
    const char *p = expression;
    while ((p = strpbrk(p, "\"'`"))) {
        char quote = *p;
        p++;
        const char *end = p;
        while (*end && *end != quote) {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        if (*end == quote) count++;
        p = end + 1;
    }

    if (count == 0) return NULL;

    char **results = calloc(count, sizeof(char*));
    if (!results) return NULL;

    int idx = 0;
    p = expression;
    while ((p = strpbrk(p, "\"'`")) && idx < count) {
        char quote = *p;
        const char *start = p++;
        const char *end = p;
        while (*end && *end != quote) {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        if (*end == quote) {
            size_t len = end - start + 1;
            char *literal = malloc(len + 1);
            if (literal) {
                memcpy(literal, start, len);
                literal[len] = '\0';
                results[idx++] = browser_decode_js_string_literal(literal);
                free(literal);
            }
            p = end + 1;
        }
    }

    if (out_count) *out_count = idx;
    return results;
}

/* PoP: browser_sensitive_browser_eval_token_reason @ tools/browser_tool.py:_sensitive_browser_eval_token_reason */
char *browser_sensitive_browser_eval_token_reason(const char *expression)
{
    if (!expression) return NULL;

    static const struct {
        const char *token;
        const char *reason;
    } sensitive_tokens[] = {
        {"cookie", "document.cookie"},
        {"localStorage", "web storage"},
        {"sessionStorage", "web storage"},
        {"indexedDB", "IndexedDB"},
        {"caches", "Cache Storage"},
        {"clipboard", "navigator sensitive API"},
        {"credentials", "navigator sensitive API"},
        {"serviceWorker", "navigator sensitive API"},
        {"fetch", "network request"},
        {"XMLHttpRequest", "network request"},
        {"WebSocket", "network request"},
        {"EventSource", "network request"},
        {"sendBeacon", "network beacon"},
        {NULL, NULL}
    };

    /* Get all decoded string literals */
    int lit_count = 0;
    char **literals = browser_decoded_js_string_literals(expression, &lit_count);

    char *concatenated = NULL;
    size_t concat_len = 0;
    for (int i = 0; i < lit_count; i++) {
        concat_len += strlen(literals[i]);
    }
    if (concat_len > 0) {
        concatenated = malloc(concat_len + 1);
        if (concatenated) {
            concatenated[0] = '\0';
            for (int i = 0; i < lit_count; i++) {
                strcat(concatenated, literals[i]);
            }
            for (char *c = concatenated; *c; c++) *c = tolower(*c);
        }
    }

    for (int i = 0; sensitive_tokens[i].token; i++) {
        const char *token = sensitive_tokens[i].token;
        const char *reason = sensitive_tokens[i].reason;

        /* Direct identifier match - case insensitive */
        char *lower_expr = strdup(expression);
        if (lower_expr) {
            for (char *c = lower_expr; *c; c++) *c = tolower(*c);
            char *lower_token = strdup(token);
            if (lower_token) {
                for (char *c = lower_token; *c; c++) *c = tolower(*c);
                if (strstr(lower_expr, lower_token)) {
                    free(lower_expr);
                    free(lower_token);
                    if (concatenated) free(concatenated);
                    for (int j = 0; j < lit_count; j++) free(literals[j]);
                    free(literals);
                    return strdup(reason);
                }
                free(lower_token);
            }
            free(lower_expr);
        }

        /* In string literals */
        if (concatenated) {
            char *lower_token = strdup(token);
            if (lower_token) {
                for (char *c = lower_token; *c; c++) *c = tolower(*c);
                if (strstr(concatenated, lower_token)) {
                    free(lower_token);
                    if (concatenated) free(concatenated);
                    for (int j = 0; j < lit_count; j++) free(literals[j]);
                    free(literals);
                    return strdup(reason);
                }
                free(lower_token);
            }
        }
    }

    if (concatenated) free(concatenated);
    for (int i = 0; i < lit_count; i++) free(literals[i]);
    free(literals);

    return NULL;
}

/* PoP: browser_risky_browser_eval_reason @ tools/browser_tool.py:_risky_browser_eval_reason */
char *browser_risky_browser_eval_reason(const char *expression)
{
    if (!expression) return NULL;

    /* Simplified regex matching - in production would use regex library */
    if (strstr(expression, "document.cookie")) return strdup("document.cookie");
    if (strstr(expression, "localStorage") || strstr(expression, "sessionStorage")) return strdup("web storage");
    if (strstr(expression, "indexedDB")) return strdup("IndexedDB");
    if (strstr(expression, "caches.open") || strstr(expression, "caches.match") || strstr(expression, "caches.keys")) return strdup("Cache Storage");
    if (strstr(expression, "navigator.clipboard") || strstr(expression, "navigator.credentials") || strstr(expression, "navigator.serviceWorker")) return strdup("navigator sensitive API");
    if (strstr(expression, "fetch(") || strstr(expression, "XMLHttpRequest(") || strstr(expression, "WebSocket(") || strstr(expression, "EventSource(")) return strdup("network request");
    if (strstr(expression, "sendBeacon(")) return strdup("network beacon");
    if (strstr(expression, "document.forms") && strstr(expression, ".value")) return strdup("form value extraction");
    if (strstr(expression, "querySelector") && (strstr(expression, "input") || strstr(expression, "textarea") || strstr(expression, "password")) && strstr(expression, ".value")) return strdup("form value extraction");

    return browser_sensitive_browser_eval_token_reason(expression);
}

/* PoP: browser_enforce_browser_eval_policy @ tools/browser_tool.py:_enforce_browser_eval_policy */
char *browser_enforce_browser_eval_policy(const char *expression)
{
    if (browser_allow_unsafe_browser_evaluate()) {
        return NULL;
    }

    char *reason = browser_risky_browser_eval_reason(expression);
    if (!reason) {
        return NULL;
    }

    char *result = malloc(1024);
    if (result) {
        snprintf(result, 1024,
            "Blocked: browser_console(expression=...) tried to use sensitive browser JavaScript primitive (%s). Use browser_snapshot/browser_get_images/browser_console without expression for normal inspection, or set browser.allow_unsafe_evaluate: true in config.yaml only for trusted pages when this access is explicitly required.",
            reason);
    }
    free(reason);
    return result;
}

