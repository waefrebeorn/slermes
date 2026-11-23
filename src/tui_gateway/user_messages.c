/*
 * user_messages.c — real C11 port of upstream Hermes tui_gateway/user_messages.py
 * (gate w13_usermessag_547811). Pure user-facing copy; no gateway state or I/O.
 * Every returned string is heap allocated and owned by the caller.
 *
 * PoP: w13_usermessag_547811_identity @ tui_gateway/user_messages.py:_identity
 * PoP: w13_usermessag_547811_turn_error_title @ tui_gateway/user_messages.py:turn_error_title
 * PoP: w13_usermessag_547811_turn_error_hint @ tui_gateway/user_messages.py:turn_error_hint
 * PoP: w13_usermessag_547811_turn_error_text @ tui_gateway/user_messages.py:turn_error_text
 * PoP: w13_usermessag_547811_busy_message @ tui_gateway/user_messages.py:busy_message
 * PoP: w13_usermessag_547811_agent_init_failed_message @ tui_gateway/user_messages.py:agent_init_failed_message
 * PoP: w13_usermessag_547811_resume_failed_message @ tui_gateway/user_messages.py:resume_failed_message
 */
#include "noc_w13_usermessag_547811.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- small scratch buffer ------------------------------------------------ */
typedef struct { char *p; size_t len, cap; } um_sb_t;

static void um_init(um_sb_t *b) {
    b->cap = 256; b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void um_reserve(um_sb_t *b, size_t need) {
    if (!b->p) return;
    if (b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap;
    while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc);
    if (np) { b->p = np; b->cap = nc; }
}
static void um_addn(um_sb_t *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    um_reserve(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void um_adds(um_sb_t *b, const char *s) {
    if (s) um_addn(b, s, strlen(s));
}
static void um_addf(um_sb_t *b, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n > 0) {
        um_reserve(b, (size_t)n);
        vsnprintf(b->p + b->len, (size_t)n + 1, fmt, ap2);
        b->len += (size_t)n;
    }
    va_end(ap2);
}
static char *um_done(um_sb_t *b) {
    if (!b->p) return (char *)calloc(1, 1);
    b->p[b->len] = '\0';   /* NUL at (possibly shortened) length */
    return b->p;
}
static char *um_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

/* ---- Python-ish scalar coercion ----------------------------------------- */
/* str(x or "") — falsy JSON becomes "". */
static char *um_str_or_empty(const json_t *n) {
    if (!n || n->type == JSON_NULL) return um_dup("");
    if (n->type == JSON_STRING) return um_dup(n->str_val ? n->str_val : "");
    if (n->type == JSON_BOOL) return um_dup(n->bool_val ? "True" : "");
    if (n->type == JSON_NUMBER) {
        if (n->num_val == 0.0) return um_dup("");
        um_sb_t b; um_init(&b);
        if (n->num_val == (double)(long long)n->num_val)
            um_addf(&b, "%lld", (long long)n->num_val);
        else
            um_addf(&b, "%g", n->num_val);
        return um_done(&b);
    }
    return um_dup("");
}
/* str(x) — None becomes "None", False becomes "False". */
static char *um_str_plain(const char *s) {
    return um_dup(s ? s : "None");
}
static int um_is_false_literal(const json_t *n) {
    return n && n->type == JSON_BOOL && !n->bool_val;
}
static int um_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* str.strip() — ASCII whitespace (fixtures use ASCII whitespace). */
static char *um_strip(const char *s) {
    if (!s) return um_dup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && um_is_ws((unsigned char)*a)) a++;
    while (b > a && um_is_ws((unsigned char)*(b - 1))) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; }
    return p;
}
/* " ".join(s.split()) — collapse all whitespace runs, strip ends. */
static char *um_collapse_ws(const char *s) {
    um_sb_t b; um_init(&b);
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    int pending = 0;
    while (*p) {
        if (um_is_ws(*p)) { pending = 1; p++; continue; }
        if (pending && b.len) um_adds(&b, " ");
        pending = 0;
        um_addn(&b, (const char *)p, 1);
        p++;
    }
    return um_done(&b);
}
static size_t um_utf8_count(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static size_t um_utf8_prefix(const char *s, size_t chars) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p && n < chars) { if ((*p & 0xC0) != 0x80) n++; p++; }
    return (size_t)(p - (const unsigned char *)(s ? s : ""));
}
/* In-place rstrip of any char in `chars`; NUL written at the new length. */
static void um_rstrip_chars(char *s, const char *chars) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    s[n] = '\0';   /* critical: terminate at shortened length */
}
static void um_replace_all(um_sb_t *b, const char *src, const char *from, const char *to) {
    size_t fl = strlen(from);
    const char *p = src;
    const char *q;
    while ((q = strstr(p, from)) != NULL) {
        um_addn(b, p, (size_t)(q - p));
        um_adds(b, to);
        p = q + fl;
    }
    um_adds(b, p);
}

/* ---- live copy tables (verbatim) ---------------------------------------- */
typedef struct { const char *key, *title, *hint; } um_copy_t;

static const um_copy_t UM_CODE_COPY[] = {
    {"auth", "The model provider rejected the API key", "Fix the key with /model, then /retry."},
    {"auth_permanent", "The model provider rejected the API key", "Fix the key with /model, then /retry."},
    {"billing", "The model provider reports no credit left", "Top up the account or switch with /model."},
    {"billing_unverified", "The model provider reports no credit left", "Top up the account or switch with /model."},
    {"rate_limit", "The model provider is rate-limiting requests", "Wait a moment, then /retry."},
    {"upstream_rate_limit", "The model provider is rate-limiting requests", "Wait a moment, then /retry."},
    {"upstream_blocked", "A firewall/CDN in front of the model provider blocked the request",
     "Set a User-Agent via the provider's extra_headers, or switch with /model."},
    {"overloaded", "The model provider is overloaded", "Wait a moment, then /retry."},
    {"server_error", "The model provider had an internal error", "Wait a moment, then /retry."},
    {"timeout", "The model provider did not answer in time", "Try /retry; if it keeps happening, switch with /model."},
    {"context_overflow", "The conversation is too long for this model", "Run /compress, then /retry."},
    {"payload_too_large", "The request was too large for this model", "Run /compress, then /retry."},
    {"model_not_found", "The model provider does not know this model", "Pick another model with /model."},
    {"content_policy_blocked", "The model provider refused this request (content policy)", "Rephrase and send again."},
    {"provider_policy_blocked", "The model provider refused this request (account policy)", "Switch with /model."},
    {"guardrail_blocked", "The model provider's prompt-injection guardrail blocked the request (not your key)",
     "Change the guardrail to Redact or Flag in the provider's settings, or switch with /model."},
    {"format_error", "The model provider rejected the request format", "Try /retry; if it persists, switch with /model."},
    {"local_runtime_unavailable", "The local model server is not running",
     "Try /retry once it is up, or switch with /model."},
    {"ssl_cert_verification", "The connection to the model provider could not be verified (TLS)",
     "Check the endpoint's certificate, then /retry."},
};
static const um_copy_t UM_LAYER_COPY[] = {
    {"auth", "The model provider rejected the credentials", "Fix them with /model, then /retry."},
    {"billing", "The model provider reports no credit left", "Top up the account or switch with /model."},
    {"endpoint", "Your custom model endpoint did not answer", "Check the endpoint is running, then /retry."},
    {"streaming", "The connection to the model provider dropped mid-reply", "Send /retry."},
    {"disk", "The disk is full, so Hermes could not save the turn", "Free some space, then /retry."},
    {"gateway", "Hermes hit an internal error while running this turn", "Send /retry; type /logs for the trace."},
    {"provider", "The model provider returned an error", "Send /retry, or switch with /model."},
};
#define UM_DEFAULT_TITLE "The request failed"
#define UM_DEFAULT_HINT  "Send /retry, or switch with /model."
#define UM_NO_RETRY_HINT ("Start the local model server (Hermes desktop app, or `hermes model` \xE2\x86\x92 " \
                          "Local models), or switch with /model.")

static void um_lookup(const char *code, const char *layer, const char **title, const char **hint) {
    for (size_t i = 0; i < sizeof(UM_CODE_COPY) / sizeof(UM_CODE_COPY[0]); i++)
        if (!strcmp(code, UM_CODE_COPY[i].key)) {
            *title = UM_CODE_COPY[i].title; *hint = UM_CODE_COPY[i].hint; return;
        }
    for (size_t i = 0; i < sizeof(UM_LAYER_COPY) / sizeof(UM_LAYER_COPY[0]); i++)
        if (!strcmp(layer, UM_LAYER_COPY[i].key)) {
            *title = UM_LAYER_COPY[i].title; *hint = UM_LAYER_COPY[i].hint; return;
        }
    *title = UM_DEFAULT_TITLE; *hint = UM_DEFAULT_HINT;
}
static const json_t *um_obj(const json_t *surface) {
    return (surface && surface->type == JSON_OBJECT) ? surface : NULL;
}

/* ---- exported functions ------------------------------------------------- */

char *w13_usermessag_547811_identity(const json_t *surface) {
    const json_t *s = um_obj(surface);
    char *pv = um_str_or_empty(s ? json_obj_get(s, "provider") : NULL);
    char *provider = um_strip(pv);
    free(pv);
    um_sb_t b; um_init(&b);
    if (provider[0]) um_addf(&b, " (%s)", provider);
    free(provider);
    return um_done(&b);
}

char *w13_usermessag_547811_turn_error_title(const json_t *surface) {
    const json_t *s = um_obj(surface);
    char *code = um_str_or_empty(s ? json_obj_get(s, "code") : NULL);
    char *layer = um_str_or_empty(s ? json_obj_get(s, "layer") : NULL);
    const char *title, *hint;
    um_lookup(code, layer, &title, &hint);
    char *id = w13_usermessag_547811_identity(s);
    um_sb_t b; um_init(&b);
    um_addf(&b, "%s%s", title, id);
    free(id); free(code); free(layer);
    return um_done(&b);
}

char *w13_usermessag_547811_turn_error_hint(const json_t *surface, int recoverable) {
    const json_t *s = um_obj(surface);
    char *code = um_str_or_empty(s ? json_obj_get(s, "code") : NULL);
    char *layer = um_str_or_empty(s ? json_obj_get(s, "layer") : NULL);
    const char *title, *hint;
    um_lookup(code, layer, &title, &hint);
    um_sb_t b; um_init(&b);
    if (!strcmp(code, "local_runtime_unavailable") &&
        (!recoverable || (s && um_is_false_literal(json_obj_get(s, "retryable"))))) {
        um_adds(&b, UM_NO_RETRY_HINT);
    } else if (recoverable) {
        um_adds(&b, hint);
    } else {
        um_replace_all(&b, hint, "Send /retry", "Pick another model with /model");
    }
    free(code); free(layer);
    return um_done(&b);
}

char *w13_usermessag_547811_turn_error_text(const char *error, const json_t *surface, int recoverable) {
    char *raw = um_collapse_ws(error);
    char *detail;
    if (um_utf8_count(raw) > 400) {
        size_t nb = um_utf8_prefix(raw, 399);
        um_sb_t d; um_init(&d);
        um_addn(&d, raw, nb);
        um_adds(&d, "\xE2\x80\xA6");  /* U+2026 */
        detail = um_done(&d);
    } else {
        detail = um_dup(raw);
    }
    free(raw);
    char *title = w13_usermessag_547811_turn_error_title(surface);
    char *hint = w13_usermessag_547811_turn_error_hint(surface, recoverable);
    um_sb_t out; um_init(&out);
    um_addf(&out, "%s. Your message was not answered.", title);
    if (detail[0]) um_addf(&out, "\nDetails: %s", detail);
    um_addf(&out, "\n%s", hint);
    free(title); free(hint); free(detail);
    return um_done(&out);
}

char *w13_usermessag_547811_busy_message(const char *command, int compressing) {
    const char *c = command ? command : "";
    while (*c == '/') c++;   /* command.lstrip('/') */
    um_sb_t b; um_init(&b);
    if (compressing) {
        um_addf(&b, "session busy \xE2\x80\x94 a /compress is running; run /%s when it finishes.", c);
    } else {
        um_addf(&b, "session busy \xE2\x80\x94 Hermes is still replying. Stop the current reply first "
                    "(Stop button, or Ctrl+C in a terminal), then run /%s.", c);
    }
    return um_done(&b);
}

char *w13_usermessag_547811_agent_init_failed_message(const char *exc) {
    char *detail = um_str_plain(exc);
    um_rstrip_chars(detail, ". ");
    int is_lock = strstr(detail, "Timed out waiting for auth store lock") != NULL ||
                  strstr(detail, "Timed out waiting for shared Nous auth lock") != NULL;
    um_sb_t b; um_init(&b);
    um_addf(&b, "Hermes could not start the assistant for this session. Details: %s. ", detail);
    if (is_lock)
        um_adds(&b, "Wait for the other process to release the lock (or exit it \xE2\x80\x94 check for "
                    "a running dashboard or background hermes process), then retry.");
    else
        um_adds(&b, "Check the model and provider with /model, or run `hermes setup` in a terminal to reconfigure.");
    free(detail);
    return um_done(&b);
}

char *w13_usermessag_547811_resume_failed_message(const char *exc) {
    um_sb_t b; um_init(&b);
    um_addf(&b, "Could not reopen that session (its transcript could not be read). Details: %s. "
                "Start a new session (/new), or pick another from /sessions.",
            exc ? exc : "None");
    return um_done(&b);
}

char *w13_usermessag_547811_agent_still_starting(void) {
    return um_dup("Hermes is still starting this session (loading tools), so this command could not run yet. "
                  "Wait for the status bar to show ready and try again.");
}
char *w13_usermessag_547811_agent_build_abandoned(void) {
    return um_dup("agent build aborted: the session was closed or replaced before the build finished");
}
char *w13_usermessag_547811_agent_missing_for_turn(void) {
    return um_dup("Hermes could not start the assistant for this session, so your message was not run. "
                  "Reopen the session (or start a new one with /new) and send it again.");
}
