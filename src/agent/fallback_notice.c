/*
 * fallback_notice.c — C home for upstream Hermes agent/fallback_notice.py.
 * PoP: w7_fallbacknot_a6d13b_constants @ agent/fallback_notice.py:FALLBACK_STATUS_KIND / _FALLBACK_DETAIL_MAX_CHARS
 * PoP: w7_fallbacknot_a6d13b_reason_text @ agent/fallback_notice.py:fallback_reason_text
 * PoP: w7_fallbacknot_a6d13b_trigger_text @ agent/fallback_notice.py:fallback_trigger_text
 * PoP: w7_fallbacknot_a6d13b_notice_hidden @ agent/fallback_notice.py:fallback_notice_hidden
 *
 * The stashed (reason, detail) tuple normally lives on the agent object and the
 * machinery-kind test lives in gateway.response_filters; both are injected from
 * the fixture. redact_sensitive_text is stubbed as identity on both sides, so
 * the gate tests the label / whitespace / truncation logic of the live module.
 */
#include "noc_w7_fallbacknot_a6d13b.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DETAIL_MAX_CHARS 200

static const struct { const char *name; const char *label; } REASONS[] = {
    {"auth", "authentication failed"},
    {"auth_permanent", "authentication permanently failed"},
    {"billing", "billing or quota exhausted"},
    {"rate_limit", "rate limit"},
    {"upstream_rate_limit", "upstream model rate limit"},
    {"overloaded", "provider overloaded"},
    {"server_error", "provider server error"},
    {"timeout", "request timeout"},
    {"ssl_cert_verification", "TLS certificate verification failed"},
    {"context_overflow", "context window exceeded"},
    {"payload_too_large", "request payload too large"},
    {"image_too_large", "image payload too large"},
    {"model_not_found", "model not found"},
    {"provider_policy_blocked", "provider policy blocked the request"},
    {"content_policy_blocked", "content policy blocked the request"},
    {"format_error", "request format rejected"},
    {"role_alternation", "adjacent same-role messages rejected"},
    {"invalid_encrypted_content", "encrypted reasoning state rejected"},
    {"multimodal_tool_content_unsupported", "multimodal tool content unsupported"},
    {"thinking_signature", "thinking signature rejected"},
    {"long_context_tier", "long-context tier unavailable"},
    {"oauth_long_context_beta_forbidden", "OAuth long-context beta unavailable"},
    {"llama_cpp_grammar_pattern", "grammar pattern rejected"},
    {"unknown", "provider failure"},
};
static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }
static const char *reason_label(const char *r) {
    size_t i;
    if (!r) return NULL;
    for (i = 0; i < sizeof(REASONS)/sizeof(REASONS[0]); i++)
        if (!strcmp(REASONS[i].name, r)) return REASONS[i].label;
    return NULL;
}
static char *reason_text_str(json_t *reason) {
    const char *r = jstr(reason);
    const char *label;
    char *out;
    size_t i, n;
    if (!r) {
        char *e = (char *)malloc(strlen("provider failure") + 1);
        if (e) strcpy(e, "provider failure");
        return e;
    }
    label = reason_label(r);
    if (label) { out = (char *)malloc(strlen(label) + 1); if (out) strcpy(out, label); return out; }
    n = strlen(r);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = r[i] == '_' ? ' ' : r[i];
    out[n] = '\0';
    return out;
}
static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* " ".join(s.split()) over ASCII whitespace */
static char *collapse_ws(const char *s) {
    char *out;
    size_t i = 0, o = 0;
    int pending = 0;
    if (!s) s = "";
    out = (char *)malloc(strlen(s) + 2);
    if (!out) return NULL;
    while (s[i]) {
        if (is_ws((unsigned char)s[i])) { pending = o > 0; i++; continue; }
        if (pending) { out[o++] = ' '; pending = 0; }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}
static char *utf8_prefix(const char *s, size_t max_cp) {
    size_t i = 0, cp = 0;
    char *out;
    while (s[i] && cp < max_cp) {
        unsigned char c = (unsigned char)s[i];
        size_t step = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        i += step; cp++;
    }
    out = (char *)malloc(i + 1);
    if (!out) return NULL;
    memcpy(out, s, i);
    out[i] = '\0';
    return out;
}
static char *rstrip_ascii(char *s) {
    size_t n;
    if (!s) return NULL;
    n = strlen(s);
    while (n > 0 && is_ws((unsigned char)s[n-1])) s[--n] = '\0';
    return s;
}

json_t *w7_fallbacknot_a6d13b_constants(void) {
    json_t *o = json_object();
    json_set(o, "FALLBACK_STATUS_KIND", json_string("fallback"));
    json_set(o, "FALLBACK_DETAIL_MAX_CHARS", json_number(DETAIL_MAX_CHARS));
    return o;
}
json_t *w7_fallbacknot_a6d13b_reason_text(json_t *reason) {
    char *s = reason_text_str(reason);
    json_t *out;
    if (!s) return json_null();
    out = json_string(s);
    free(s);
    return out;
}
json_t *w7_fallbacknot_a6d13b_trigger_text(json_t *reason, json_t *stashed) {
    char *label = reason_text_str(reason);
    const char *r;
    json_t *out;
    if (!label) return json_null();
    r = jstr(reason);
    if (r && stashed && stashed->type == JSON_OBJECT) {
        const char *sr = jstr(json_obj_get(stashed, "reason"));
        const char *detail = jstr(json_obj_get(stashed, "detail"));
        if (sr && !strcmp(sr, r) && detail) {
            char *collapsed = collapse_ws(detail);
            if (collapsed && *collapsed) {
                size_t n = 0;
                { const char *p = collapsed; while (*p) { unsigned char c = (unsigned char)*p; p += c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; n++; } }
                if (n > DETAIL_MAX_CHARS) {
                    char *cut = rstrip_ascii(utf8_prefix(collapsed, DETAIL_MAX_CHARS - 1));
                    size_t total = strlen(label) + 2 + strlen(cut) + strlen("\xe2\x80\xa6") + 1;
                    char *full = (char *)malloc(total);
                    if (full) {
                        snprintf(full, total, "%s: %s\xe2\x80\xa6", label, cut);
                        out = json_string(full);
                        free(full); free(cut); free(collapsed); free(label);
                        return out;
                    }
                    free(cut);
                }
                {
                    size_t total = strlen(label) + 2 + strlen(collapsed) + 1;
                    char *full = (char *)malloc(total);
                    if (full) {
                        snprintf(full, total, "%s: %s", label, collapsed);
                        out = json_string(full);
                        free(full); free(collapsed); free(label);
                        return out;
                    }
                }
            }
            free(collapsed);
        }
    }
    out = json_string(label);
    free(label);
    return out;
}
json_t *w7_fallbacknot_a6d13b_notice_hidden(json_t *display_kind) {
    const char *d = jstr(display_kind);
    return json_bool(d && (!strcmp(d, "internal_notification") || !strcmp(d, "process_complete") ||
                           !strcmp(d, "async_delegation_complete")) ? true : false);
}
