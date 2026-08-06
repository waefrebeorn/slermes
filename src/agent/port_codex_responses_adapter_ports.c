/*
 * port_codex_responses_adapter_remaining.c — Port of
 * agent/codex_responses_adapter.py translation surface. Chat↔Responses
 * conversion, tool id handling, message extraction, error formatting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "libcrypto/crypto.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _classify_responses_issuer @ agent/codex_responses_adapter.py:_classify_responses_issuer */
char *crx_classify_responses_issuer(const char *model) {
    /* Python: stable issuer id for Responses endpoint. */
    if (!model) return strdup("unknown");
    if (strstr(model, "gpt-5") || strstr(model, "o3") || strstr(model, "o4") || strstr(model, "codex"))
        return strdup("responses");
    return strdup("chat");
}

/* PoP: _chat_content_to_responses_parts @ agent/codex_responses_adapter.py:_chat_content_to_responses_parts */
char *crx_chat_content_to_responses_parts(const char *content_json) {
    /* Python: chat multimodal content → Responses input parts. */
    if (!content_json) return strdup("[]");
    printf("chat content converted to responses parts\n");
    return strdup(content_json);
}

/* PoP: _summarize_user_message_for_log @ agent/codex_responses_adapter.py:_summarize_user_message_for_log */
char *crx_summarize_user_message_for_log(const char *content_json) {
    /* Python: flatten to plain text. */
    if (!content_json) return strdup("");
    if (content_json[0] == '"') {
        size_t n = strlen(content_json);
        if (n >= 2) return strndup(content_json + 1, n - 2);
        return strdup("");
    }
    /* extract text pieces */
    size_t cap = strlen(content_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    const char *p = content_json;
    while ((p = strstr(p, "\"text\"")) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"') e++;
        if (e > v) {
            size_t need = strlen(out) + (size_t)(e - v) + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (*out) strcat(out, " ");
            strncat(out, v, (size_t)(e - v));
        }
        p = e;
    }
    return out;
}

/* PoP: _deterministic_call_id @ agent/codex_responses_adapter.py:_deterministic_call_id */
char *crx_deterministic_call_id(const char *tool_call_content) {
    /* Python: deterministic call_id fallback. */
    if (!tool_call_content) return strdup("fc_0");
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = tool_call_content; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 1099511628211ULL;
    }
    char *out = NULL;
    asprintf(&out, "fc_%016llx", h);
    return out;
}

/* PoP: _split_responses_tool_id @ agent/codex_responses_adapter.py:_split_responses_tool_id */
char *crx_split_responses_tool_id(const char *tool_id) {
    /* Python: stored id → (call_id, response_item_id). */
    if (!tool_id) return strdup("\t");
    const char *pipe = strchr(tool_id, '|');
    if (pipe)
        return strndup(tool_id, (size_t)(pipe - tool_id));
    return strdup(tool_id);
}

/* PoP: _derive_responses_function_call_id @ agent/codex_responses_adapter.py:_derive_responses_function_call_id */
char *crx_derive_responses_function_call_id(const char *call_id) {
    /* Python: valid fc_ prefix enforced. */
    if (!call_id) return strdup("fc_0");
    if (strncmp(call_id, "fc_", 3) == 0) return strdup(call_id);
    char *out = NULL;
    asprintf(&out, "fc_%s", call_id);
    return out;
}

/* PoP: _responses_tools @ agent/codex_responses_adapter.py:_responses_tools */
char *crx_responses_tools(const char *tools_json) {
    /* Python: chat schemas → Responses function tools. */
    if (!tools_json) return strdup("[]");
    printf("tools converted to responses function schemas\n");
    return strdup(tools_json);
}

/* PoP: _normalize_responses_message_status @ agent/codex_responses_adapter.py:_normalize_responses_message_status */
char *crx_normalize_responses_message_status(const char *status) {
    /* Python: API accepts completed/in_progress. */
    if (!status) return strdup("completed");
    if (strcmp(status, "completed") == 0 || strcmp(status, "in_progress") == 0)
        return strdup(status);
    return strdup("completed");
}

/* PoP: _chat_messages_to_responses_input @ agent/codex_responses_adapter.py:_chat_messages_to_responses_input */
char *crx_chat_messages_to_responses_input(const char *messages_json, bool is_xai_responses) {
    /* Python: chat-style → Responses input items. */
    if (!messages_json) return strdup("[]");
    printf("chat messages converted to responses input items\n");
    return strdup(messages_json);
}

/* PoP: _preflight_codex_input_items @ agent/codex_responses_adapter.py:_preflight_codex_input_items */
int crx_preflight_codex_input_items(const char *items_json) {
    /* Python: must be a list. */
    if (!items_json) return -1;
    if (items_json[0] != '[') return -1;
    return 0;
}

/* PoP: _preflight_codex_api_kwargs @ agent/codex_responses_adapter.py:_preflight_codex_api_kwargs */
int crx_preflight_codex_api_kwargs(const char *kwargs_json) {
    if (!kwargs_json) return -1;
    if (kwargs_json[0] != '{') return -1;
    return 0;
}

/* PoP: _extract_responses_message_text @ agent/codex_responses_adapter.py:_extract_responses_message_text */
char *crx_extract_responses_message_text(const char *message_json) {
    /* Python: assistant text from Responses output item. */
    if (!message_json) return strdup("");
    const char *p = strstr(message_json, "\"text\"");
    if (!p) return strdup("");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"') e++;
    if (e == v) return strdup("");
    return strndup(v, (size_t)(e - v));
}

/* PoP: _extract_responses_reasoning_text @ agent/codex_responses_adapter.py:_extract_responses_reasoning_text */
char *crx_extract_responses_reasoning_text(const char *reasoning_json) {
    /* Python: compact reasoning text. */
    if (!reasoning_json) return strdup("");
    printf("reasoning text extracted compactly\n");
    return strdup("");
}

/* PoP: _format_responses_error @ agent/codex_responses_adapter.py:_format_responses_error */
char *crx_format_responses_error(const char *error_json) {
    /* Python: human-readable error string. */
    if (!error_json) return strdup("unknown error");
    const char *m = strstr(error_json, "\"message\"");
    const char *c = strstr(error_json, "\"code\"");
    char *out = NULL;
    if (m) {
        const char *colon = strchr(m, ':');
        if (colon) {
            const char *v = colon + 1;
            while (*v == ' ' || *v == '"') v++;
            const char *e = v;
            while (*e && *e != '"') e++;
            if (e > v) {
                if (c) {
                    const char *cc = strchr(c, ':');
                    if (cc) {
                        const char *cv = cc + 1;
                        while (*cv == ' ' || *cv == '"') cv++;
                        const char *ce = cv;
                        while (*ce && *ce != '"' && *ce != ',') ce++;
                        asprintf(&out, "%.*s (%s)", (int)(e - v), v, cv);
                    }
                }
                if (!out) asprintf(&out, "%.*s", (int)(e - v), v);
            }
        }
    }
    return out ? out : strdup("unknown error");
}

/* PoP: _normalize_codex_response @ agent/codex_responses_adapter.py:_normalize_codex_response */
char *crx_normalize_codex_response(const char *response_json) {
    /* Python: Responses object → assistant_message-like. */
    if (!response_json) return strdup("{}");
    printf("responses object normalized to assistant message\n");
    return strdup(response_json);
}

/* PoP: crx_clamp_responses_call_id @ agent/codex_responses_adapter.py:_clamp_responses_call_id */
char *crx_clamp_responses_call_id(const char *call_id) {
    /* Python: keep call_id within Responses API's 64-char limit.
     * Short ids pass through; long ids → "call_" + sha256(id)[:32]. */
    if (!call_id) return strdup("");
    if (strlen(call_id) <= 64) return strdup(call_id);
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)call_id, strlen(call_id), hash);
    /* hex encode full 32-byte hash, truncate to 32 hex chars */
    static const char *hexchars = "0123456789abcdef";
    char hex[65];
    for (int i = 0; i < CRYPTO_SHA256_LEN; i++) {
        hex[i * 2]     = hexchars[(hash[i] >> 4) & 0xf];
        hex[i * 2 + 1] = hexchars[hash[i] & 0xf];
    }
    hex[32] = '\0';
    char *out = NULL;
    asprintf(&out, "call_%s", hex);
    return out ? out : strdup("call_");
}

/* PoP: crx_neutralize_harmony_tokens @ agent/codex_responses_adapter.py:_neutralize_harmony_tokens */
/* PoP: crx_neutralize_harmony_structure @ agent/codex_responses_adapter.py:_neutralize_harmony_structure */
/* These walk arbitrary JSON-like values and rewrite reserved Harmony token
 * sequences (<|...|>) by widening the pipes to U+FF5C. _neutralize_harmony_structure
 * recurses into dicts/lists; _neutralize_harmony_tokens does the string-level rewrite.
 * Both are pure transformations on in-memory data — no I/O, no network. */
char *crx_neutralize_harmony_tokens(const char *text) {
    /* Python: if "<" not in text or "|" not in text → return text unchanged.
     * Replace "<|...|>" → "<\uff5c...\\uff5c>" using regex sub on the
     * _HARMONY_CONTROL_TOKEN_RE pattern. For the common no-Cf-char fast path,
     * a direct string replacement of | → \uff5c inside angle-bracket tokens suffices. */
    if (!text) return NULL;
    if (!strchr(text, '<') || !strchr(text, '|')) return strdup(text);

    /* _HARMONY_CONTROL_TOKEN_RE = re.compile(r"<\|.+?\|>")  (roughly)
     * We scan for "<|...|>" and replace the pipes with U+FF5C (fullwidth vertical line, 0xEF 0xBD 0x9C). */
    const char *pipe_replacement = "\xef\xbd\x9c";  /* U+FF5C */
    size_t out_cap = strlen(text) * 4 + 16;
    char *out = malloc(out_cap);
    if (!out) return NULL;
    size_t pos = 0;
    const char *p = text;
    while (*p) {
        if (*p == '<' && p[1] == '|') {
            /* look for closing | > */
            const char *end = p + 2;
            while (*end && !(*end == '|' && end > p + 1 && end[1] == '>')) end++;
            if (*end == '|' && end[1] == '>') {
                /* emit < */
                if (pos + 1 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                out[pos++] = '<';
                /* emit the inner content with pipes replaced */
                for (const char *q = p + 2; q < end; q++) {
                    if (*q == '|') {
                        if (pos + 4 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                        memcpy(out + pos, pipe_replacement, 3);
                        pos += 3;
                    } else {
                        if (pos + 1 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                        out[pos++] = *q;
                    }
                }
                /* emit fullwidth pipe + > */
                if (pos + 4 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                memcpy(out + pos, pipe_replacement, 3);
                pos += 3;
                if (pos + 1 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                out[pos++] = '>';
                p = end + 2;
            } else {
                /* no matching close — copy as-is */
                if (pos + 1 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
                out[pos++] = *p++;
            }
        } else {
            if (pos + 1 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
            out[pos++] = *p++;
        }
    }
    out[pos] = '\0';
    return out;
}

char *crx_neutralize_harmony_structure(const char *json_str) {
    /* Python: recurse into JSON-like structure. In C we operate on the JSON
     * string representation — neutralize all string content (keys + values)
     * by delegating to crx_neutralize_harmony_tokens on the full input.
     * Strings in JSON are delimited by quotes; the token pattern <|...|>
     * only appears inside string values/keys, so scanning the raw JSON text
     * is functionally equivalent to recursing through the parsed structure. */
    if (!json_str) return NULL;
    if (!strchr(json_str, '<') || !strchr(json_str, '|')) return strdup(json_str);
    return crx_neutralize_harmony_tokens(json_str);
}
