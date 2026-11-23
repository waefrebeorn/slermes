/*
 * tts_tool_providers.c - faithful C11 port of selected pure helpers from
 * upstream Hermes tools/tts_tool_providers.py (gate w11_ttstoolpr_0ad803).
 *
 * PoP: w11_ttstoolpr_0ad803_config_bool @ tools/tts_tool_providers.py:_config_bool
 * PoP: w11_ttstoolpr_0ad803_response_format @ tools/tts_tool_providers.py:_tts_response_format_from_path
 * PoP: w11_ttstoolpr_0ad803_clamped_number @ tools/tts_tool_providers.py:_clamped_number
 * PoP: w11_ttstoolpr_0ad803_gemini_audio_tags @ tools/tts_tool_providers.py:_gemini_audio_tags_enabled
 * PoP: w11_ttstoolpr_0ad803_compose_gemini_prompt @ tools/tts_tool_providers.py:_compose_gemini_tts_prompt
 *
 * Network/SDK generation paths and auxiliary-model rewrites are intentionally not
 * ported (external I/O, non-deterministic).
 */
#include "noc_w11_ttstoolpr_0ad803.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *s; size_t n, cap; } SB;
static void sb_init(SB *b) { b->cap = 64; b->n = 0; b->s = (char *)malloc(b->cap); if (b->s) b->s[0] = 0; }
static void sb_addn(SB *b, const char *p, size_t n) {
    if (!p || !n || !b->s) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap * 2;
        while (nc < b->n + n + 1) nc *= 2;
        char *q = (char *)realloc(b->s, nc);
        if (!q) return;
        b->s = q; b->cap = nc;
    }
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = 0;
}
static void sb_add(SB *b, const char *p) { if (p) sb_addn(b, p, strlen(p)); }
static char *sb_take(SB *b) { char *s = b->s ? b->s : (char *)malloc(1); if (s && !b->s) s[0] = 0; b->s = NULL; b->n = b->cap = 0; return s; }
static void sb_free(SB *b) { free(b->s); b->s = NULL; b->n = b->cap = 0; }

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *strip_dup(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = 0; }
    return o;
}

static int in_words(const char *s, const char *const *words, size_t count) {
    for (size_t i = 0; i < count; i++) if (!strcmp(s, words[i])) return 1;
    return 0;
}

static int config_bool_raw(const json_t *v, int def) {
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) {
        char *t = strip_dup(v->str_val ? v->str_val : "");
        for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
        static const char *const trues[] = {"1", "true", "yes", "on", "enabled"};
        static const char *const falses[] = {"0", "false", "no", "off", "disabled"};
        int r = def;
        if (in_words(t, trues, sizeof(trues) / sizeof(trues[0]))) r = 1;
        else if (in_words(t, falses, sizeof(falses) / sizeof(falses[0]))) r = 0;
        free(t);
        return r;
    }
    return def;
}

json_t *w11_ttstoolpr_0ad803_config_bool(const json_t *value, int default_val) {
    return json_bool(config_bool_raw(value, default_val) ? true : false);
}

json_t *w11_ttstoolpr_0ad803_response_format(const char *output_path) {
    if (!output_path) return json_string("mp3");
    size_t n = strlen(output_path);
    if (n >= 4 && !strcmp(output_path + n - 4, ".ogg")) return json_string("opus");
    if (n >= 4 && !strcmp(output_path + n - 4, ".wav")) return json_string("wav");
    if (n >= 5 && !strcmp(output_path + n - 5, ".flac")) return json_string("flac");
    return json_string("mp3");
}

static int parse_int_strict(const char *s, long long *out) {
    while (*s && py_isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    char *end = NULL;
    long long x = strtoll(s, &end, 10);
    if (end == s) return 0;
    while (*end && py_isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = x;
    return 1;
}

json_t *w11_ttstoolpr_0ad803_clamped_number(const json_t *raw, const char *cast,
                                            double lo, double hi) {
    if (!raw || raw->type == JSON_NULL) return json_null();
    int is_int = cast && !strcmp(cast, "int");
    double x = 0.0;
    if (raw->type == JSON_STRING) {
        if (is_int) {
            long long t = 0;
            if (!parse_int_strict(raw->str_val ? raw->str_val : "", &t)) return json_null();
            x = (double)t;
        } else {
            char *end = NULL;
            const char *s = raw->str_val ? raw->str_val : "";
            x = strtod(s, &end);
            if (end == s || *end != '\0') return json_null();
        }
    } else if (raw->type == JSON_NUMBER) {
        x = raw->num_val;
        if (is_int) x = (double)(long long)x;
    } else if (raw->type == JSON_BOOL) {
        x = raw->bool_val ? 1.0 : 0.0;
    } else {
        return json_null();
    }
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    if (is_int) x = (double)(long long)x;
    return json_number(x);
}

json_t *w11_ttstoolpr_0ad803_gemini_audio_tags(const json_t *gemini_config, const char *model) {
    const json_t *raw = json_obj_get(gemini_config, "audio_tags");
    if (raw && raw->type == JSON_OBJECT) raw = json_obj_get(raw, "enabled");
    if (!config_bool_raw(raw, 0)) return json_bool(false);
    const char *m = model ? model : "";
    char *t = strip_dup(m);
    for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
    const char *last = strrchr(t, '/');
    const char *tail = last ? last + 1 : t;
    int ok = strstr(tail, "gemini-3.1") && strstr(tail, "tts");
    free(t);
    return json_bool(ok ? true : false);
}

/* Match { ws transcript ws } or {{ ws transcript ws }} case-insensitively. */
static int match_placeholder(const char *s, size_t n, size_t i, size_t *end) {
    size_t p = i, opens = 0;
    if (p >= n || s[p] != '{') return 0;
    while (p < n && s[p] == '{' && opens < 2) { opens++; p++; }
    while (p < n && py_isspace((unsigned char)s[p])) p++;
    static const char word[] = "transcript";
    if (n - p < sizeof(word) - 1) return 0;
    for (size_t k = 0; k < sizeof(word) - 1; k++)
        if (tolower((unsigned char)s[p + k]) != word[k]) return 0;
    p += sizeof(word) - 1;
    while (p < n && py_isspace((unsigned char)s[p])) p++;
    size_t need = opens;
    if (n - p < need) return 0;
    for (size_t k = 0; k < need; k++) if (s[p + k] != '}') return 0;
    p += need;
    *end = p;
    return 1;
}

static char *substitute_transcript(const char *persona, const char *transcript) {
    size_t n = strlen(persona);
    SB out; sb_init(&out);
    size_t i = 0;
    while (i < n) {
        size_t e;
        if (match_placeholder(persona, n, i, &e)) { sb_add(&out, transcript); i = e; continue; }
        sb_addn(&out, persona + i, 1); i++;
    }
    char *r = sb_take(&out); sb_free(&out); return r;
}

static const char PREAMBLE[] =
    "Synthesize speech from the TRANSCRIPT only. Treat AUDIO PROFILE, "
    "SCENE, DIRECTOR'S NOTES, and SAMPLE CONTEXT as performance direction; "
    "do not speak those sections aloud.";

json_t *w11_ttstoolpr_0ad803_compose_gemini_prompt(const char *text, const char *persona_prompt) {
    char *transcript = strip_dup(text ? text : "");
    if (!persona_prompt || !*persona_prompt) {
        json_t *r = json_string(transcript ? transcript : "");
        free(transcript);
        return r;
    }
    /* Probe for a placeholder with either brace width. */
    size_t n = strlen(persona_prompt), found = 0;
    for (size_t i = 0; i < n && !found; i++) { size_t e; if (match_placeholder(persona_prompt, n, i, &e)) found = 1; }
    SB full; sb_init(&full);
    sb_add(&full, PREAMBLE);
    sb_add(&full, "\n\n");
    if (found) {
        char *sub = substitute_transcript(persona_prompt, transcript);
        sb_add(&full, sub);
        free(sub);
    } else {
        sb_add(&full, persona_prompt);
        sb_add(&full, "\n\n#### TRANSCRIPT\n");
        sb_add(&full, transcript);
    }
    char *raw = sb_take(&full); sb_free(&full);
    char *clean = strip_dup(raw ? raw : "");
    json_t *r = json_string(clean ? clean : "");
    free(raw); free(clean); free(transcript);
    return r;
}
