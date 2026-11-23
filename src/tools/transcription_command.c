/*
 * transcription_command.c - C home for upstream Hermes tools/transcription_command.py.
 * PoP: w15_transcripti_57b86b_enforce_prompt @ tools/transcription_command.py:_enforce_prompt_length_limit
 * PoP: w15_transcripti_57b86b_unregistered_error @ tools/transcription_command.py:_unregistered_stt_provider_error
 *
 * Deterministic prompt-budget and error-envelope helpers for command STT providers.
 * Process spawning, temp dirs and filesystem reads stay in Python; only the pure
 * truncation/formatting rules are mirrored here.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_transcripti_57b86b.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL)   return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}
static char *strip(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1); memcpy(out, s, n); out[n] = 0; return out;
}

/* UTF-8 code-point counting, used because Python len()/slice count characters. */
static size_t u8step(const unsigned char *p) {
    unsigned char c = *p;
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}
static size_t u8_count(const char *s) {
    size_t n = 0; const unsigned char *p = (const unsigned char *)s;
    while (*p) { p += u8step(p); n++; }
    return n;
}
static char *u8_tail(const char *s, size_t keep) {
    size_t total = u8_count(s);
    if (keep >= total) return dupstr(s);
    size_t skip = total - keep; const unsigned char *p = (const unsigned char *)s;
    while (skip--) p += u8step(p);
    return dupstr((const char *)p);
}

static int capped_provider(const char *p) {
    return !strcmp(p, "local") || !strcmp(p, "openai") || !strcmp(p, "groq") || !strcmp(p, "deepinfra");
}

struct json_t *w15_transcripti_57b86b_enforce_prompt(const struct json_t *prompt, const char *provider) {
    if (!prompt || prompt->type == JSON_NULL) return json_null();
    char *s = py_str(prompt);            /* str(prompt) */
    const size_t max_chars = 224u * 4u;  /* 224 tokens * CHARS_PER_TOKEN(4) */
    json_t *out;
    if (!s[0] || !capped_provider(provider ? provider : "") || u8_count(s) <= max_chars)
        out = json_string(s);
    else {
        char *tail = u8_tail(s, max_chars);
        out = json_string(tail);
        free(tail);
    }
    free(s);
    return out;
}

struct json_t *w15_transcripti_57b86b_unregistered_error(const struct json_t *provider) {
    char *raw = py_str(provider);
    char *key = strip(raw);
    size_t n = strlen(key) + 320;
    char *msg = (char *)malloc(n);
    snprintf(msg, n,
             "stt.provider='%s' is set but no built-in, command, or plugin provider registered that name. "
             "Run `hermes plugins list` to see installed STT plugins, or configure a command provider under "
             "`stt.providers.%s.command`.", key, key);
    json_t *r = json_object();
    json_set(r, "success", json_bool(false));
    json_set(r, "transcript", json_string(""));
    json_set(r, "error", json_string(msg));
    json_set(r, "provider", json_string(key));
    json_set(r, "error_type", json_string("provider_not_registered"));
    free(msg); free(key); free(raw);
    return r;
}

