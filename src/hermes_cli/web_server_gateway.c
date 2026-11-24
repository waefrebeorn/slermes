/*
 * web_server_gateway.c — C home for upstream Hermes hermes_cli/web_server_gateway.py.
 *
 * PoP: w17_webserver_4c1444_windows_build_number @ hermes_cli/web_server_gateway.py:_windows_build_number
 * PoP: w17_webserver_4c1444_display_system_platform @ hermes_cli/web_server_gateway.py:_display_system_platform
 * PoP: w17_webserver_4c1444_named_profile_from_action @ hermes_cli/web_server_gateway.py:_named_profile_from_action
 * PoP: w17_webserver_4c1444_is_host_gateway_spawn @ hermes_cli/web_server_gateway.py:_is_host_gateway_spawn
 * PoP: w17_webserver_4c1444_split_text_for_speak_stream @ hermes_cli/web_server_gateway.py:_split_text_for_speak_stream
 * PoP: w17_webserver_4c1444_strip_session_list_rows @ hermes_cli/web_server_gateway.py:_strip_session_list_rows
 *
 * Faithful ports of the six pure helpers. The TTS split mirrors
 * tools/tts_streaming.SENTENCE_BOUNDARY_RE == (?<=[.!?])(?:\s|\n)|(?:\n\n),
 * and slices by Unicode code point (UTF-8 offsets always start at lead bytes).
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_webserver_4c1444.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* ---- _windows_build_number / _display_system_platform ---- */
static bool is_digit(char c) { return c >= '0' && c <= '9'; }

/* match (?:^|[^\d])10\.0\.(\d{5,})(?=[^\d]|$) */
static bool match_build(const char *value, long long *out) {
    if (!value) return false;
    size_t n = strlen(value);
    for (size_t i = 0; i + 5 < n; i++) {
        if (!memcmp(value + i, "10.0.", 5) &&
            (i == 0 || !is_digit(value[i - 1]))) {
            size_t j = i + 5;
            size_t digits = 0;
            while (j < n && is_digit(value[j])) { j++; digits++; }
            if (digits >= 5 && (j == n || !is_digit(value[j]))) {
                char buf[64];
                size_t take = (j - (i + 5) < 63) ? (j - (i + 5)) : 63;
                memcpy(buf, value + i + 5, take);
                buf[take] = '\0';
                *out = strtoll(buf, NULL, 10);
                return true;
            }
        }
    }
    return false;
}

bool w17_webserver_4c1444_windows_build_number(const char *version, const char *platform_label,
                                               long long *out_build) {
    if (match_build(version ? version : "", out_build)) return true;
    return match_build(platform_label ? platform_label : "", out_build);
}

json_t *w17_webserver_4c1444_display_system_platform(const char *system, const char *release,
                                                     const char *version, const char *platform_label) {
    const char *sys = system ? system : "";
    const char *rel = release ? release : "";
    const char *ver = version ? version : "";
    const char *plat = platform_label ? platform_label : "";
    long long build = 0;
    char *tmp_plat = NULL;
    if (!strcmp(sys, "Windows") && !strcmp(rel, "10") &&
        w17_webserver_4c1444_windows_build_number(ver, plat, &build) && build >= 22000) {
        if (!memcmp(plat, "Windows-10-", 11)) {
            char *np = (char *)malloc(strlen(plat) + 2);   /* 11 bytes prefix + "Windows-11" */
            if (np) {
                strcpy(np, "Windows-11");
                strcat(np, plat + 10);                     /* keep the trailing '-' */
                plat = tmp_plat = np;
            }
        }
        rel = "11";
    }
    json_t *out = json_object();
    json_set(out, "os", json_string(sys));
    json_set(out, "os_release", json_string(rel));
    json_set(out, "os_version", json_string(ver));
    json_set(out, "platform", json_string(plat));
    free(tmp_plat);   /* json_string already copied it */
    return out;
}

/* ---- _named_profile_from_action / _is_host_gateway_spawn ---- */
static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t a = 0, b = strlen(s);
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r' ||
                     s[b-1] == '\v' || s[b-1] == '\f')) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';   /* NUL at the new length */
    return out;
}

static const char *arg_at(json_t *sub, size_t i) {
    if (!json_is_array(sub) || i >= json_len(sub)) return NULL;
    json_t *v = json_get(sub, i);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

char *w17_webserver_4c1444_named_profile_from_action(json_t *subcommand) {
    size_t n = json_is_array(subcommand) ? json_len(subcommand) : 0;
    if (n >= 2) {
        const char *a0 = arg_at(subcommand, 0);
        if (!strcmp(a0, "-p") || !strcmp(a0, "--profile")) {
            char *s = py_strip(arg_at(subcommand, 1));
            if (s && !*s) { free(s); return NULL; }
            return s;
        }
    }
    if (n >= 1) {
        const char *a0 = arg_at(subcommand, 0);
        if (!memcmp(a0, "--profile=", 10)) {
            char *s = py_strip(a0 + 10);   /* split("=",1)[1] */
            if (s && !*s) { free(s); return NULL; }
            return s;
        }
    }
    return NULL;
}

bool w17_webserver_4c1444_is_host_gateway_spawn(json_t *subcommand) {
    char *profile = w17_webserver_4c1444_named_profile_from_action(subcommand);
    if (profile && strcmp(profile, "default") != 0) { free(profile); return false; }
    size_t n = json_is_array(subcommand) ? json_len(subcommand) : 0;
    size_t first = 0;
    if (profile) {
        const char *a0 = arg_at(subcommand, 0);
        if (!strcmp(a0, "-p") || !strcmp(a0, "--profile")) first = 2;
        else if (!memcmp(a0, "--profile=", 10)) first = 1;
    }
    free(profile);
    return (first < n) && !strcmp(arg_at(subcommand, first), "gateway");
}

/* ---- _split_text_for_speak_stream ---- */
typedef struct { char **items; size_t count, cap; } strlist;
static void sl_push(strlist *l, char *s) {
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char **p = (char **)realloc(l->items, cap * sizeof(char *));
        if (!p) { free(s); return; }
        l->items = p; l->cap = cap;
    }
    l->items[l->count++] = s;
}
static char *substr(const char *s, size_t a, size_t b) {
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static bool only_space(const char *s) {
    if (!s || !*s) return true;
    for (const char *p = s; *p; p++) if (!is_space(*p)) return false;
    return true;
}
static void split_sentences(const char *text, strlist *out) {
    size_t n = strlen(text), start = 0, i = 0;
    while (i < n) {
        size_t sep = 0;
        char prev = (i > 0) ? text[i - 1] : 0;
        if ((prev == '.' || prev == '!' || prev == '?') && is_space(text[i])) sep = 1;
        else if (i + 1 < n && text[i] == '\n' && text[i + 1] == '\n') sep = 2;
        if (sep) {
            sl_push(out, substr(text, start, i));
            i += sep; start = i;
        } else i++;
    }
    sl_push(out, substr(text, start, n));
}
/* byte offset of code-point index cp; always at a UTF-8 lead byte (or end). */
static size_t cp_offset(const char *s, size_t cp) {
    size_t count = 0;
    for (size_t at = 0; s[at]; at++) {
        /* Only UTF-8 lead bytes start a code point; a tail offset must land
         * on one, never on a continuation byte. */
        if (((unsigned char)s[at] & 0xC0) != 0x80) {
            if (count == cp) return at;
            count++;
        }
    }
    return strlen(s);
}
static size_t cp_len(const char *s) {
    size_t count = 0;
    for (const char *p = s; *p; p++) if (((unsigned char)*p & 0xC0) != 0x80) count++;
    return count;
}

json_t *w17_webserver_4c1444_split_text_for_speak_stream(const char *text, long long cap) {
    if (!cap || cap <= 0) cap = 4000;
    strlist sentences = {0};
    split_sentences(text ? text : "", &sentences);
    strlist pieces = {0};
    char *buf = NULL;   /* owned, or NULL */
    for (size_t i = 0; i < sentences.count; i++) {
        char *sentence = sentences.items[i];
        if (only_space(sentence)) { free(sentence); continue; }
        size_t slen = cp_len(sentence);
        size_t ccap = (size_t)cap;
        while (slen > ccap) {
            size_t cut = cp_offset(sentence, ccap);
            sl_push(&pieces, substr(sentence, 0, cut));
            char *rest = substr(sentence, cut, strlen(sentence));
            free(sentence);
            sentence = rest;
            slen -= ccap;
        }
        size_t blen = buf ? cp_len(buf) : 0;
        if (buf && blen + slen + 1 > ccap) {
            sl_push(&pieces, buf);
            buf = sentence;
        } else if (buf) {
            char *joined = (char *)malloc(strlen(buf) + 1 + strlen(sentence) + 1);
            if (joined) {
                strcpy(joined, buf);
                strcat(joined, " ");
                strcat(joined, sentence);
            }
            free(buf); free(sentence);
            buf = joined;
        } else {
            buf = sentence;
        }
    }
    if (buf) sl_push(&pieces, buf);
    json_t *out = json_array();
    for (size_t i = 0; i < pieces.count; i++) {
        json_append(out, json_string(pieces.items[i] ? pieces.items[i] : ""));
        free(pieces.items[i]);
    }
    free(pieces.items);
    free(sentences.items);
    return out;
}

/* ---- _strip_session_list_rows ---- */
json_t *w17_webserver_4c1444_strip_session_list_rows(json_t *sessions) {
    json_t *out = json_array();
    if (!json_is_array(sessions)) return out;
    for (size_t i = 0; i < json_len(sessions); i++) {
        json_t *row = json_copy(json_get(sessions, i));
        if (row) {
            json_obj_del(row, "system_prompt");
            json_obj_del(row, "model_config");
            json_append(out, row);
        }
    }
    return out;
}
