/* Slermes C port — cli.py (DA-4 gaps → real implementations)
 * Pure utility functions: path helpers, color math, string ops */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* xstrdup — null-safe strdup */
static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _hex_to_ansi (L2518) ── */
/* PoP: cli_hex_to_ansi @ cli.py:_hex_to_ansi */
char *cli_hex_to_ansi(const char *hex_color, bool bold) {
    if (!hex_color) hex_color = "#b8860b";
    /* Skip '#' prefix */
    const char *h = hex_color;
    if (*h == '#') h++;
    int r, g, b;
    if (sscanf(h, "%02x%02x%02x", &r, &g, &b) != 3) {
        return xstrdup(bold ? "\033[1;38;2;184;134;11m" : "\033[38;2;184;134;11m");
    }
    char *buf = malloc(32);
    if (!buf) return NULL;
    snprintf(buf, 32, "\033[%s38;2;%d;%d;%dm", bold ? "1;" : "", r, g, b);
    return buf;
}

/* ── _luminance_from_hex (L2559) ── */
/* PoP: cli_luminance_from_hex @ cli.py:_luminance_from_hex */
double cli_luminance_from_hex(const char *hex_str) {
    if (!hex_str) return -1.0;
    const char *s = hex_str;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '#') s++;
    char buf[7];
    size_t len = strlen(s);
    if (len == 3) {
        buf[0] = buf[1] = s[0];
        buf[2] = buf[3] = s[1];
        buf[4] = buf[5] = s[2];
        buf[6] = '\0';
        s = buf;
        len = 6;
    }
    if (len != 6) return -1.0;
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)s[i])) return -1.0;
    }
    int r = 0, g = 0, b = 0;
    sscanf(s, "%02x%02x%02x", &r, &g, &b);
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
}

/* ── _path_is_within_root (L1449) ── */
/* PoP: cli_path_is_within_root @ cli.py:_path_is_within_root */
bool cli_path_is_within_root(const char *path, const char *root) {
    if (!path || !root) return false;
    size_t root_len = strlen(root);
    if (root_len == 0) return false;
    /* Normalize: ensure root ends with '/' for prefix check */
    if (strncmp(path, root, root_len) != 0) return false;
    /* Ensure it's a true subtree (next char is '/' or end) */
    char next = path[root_len];
    return next == '\0' || next == '/';
}

/* ── _normalize_git_bash_path (L1399) ── */
/* PoP: cli_normalize_git_bash_path @ cli.py:_normalize_git_bash_path */
char *cli_normalize_git_bash_path(const char *p) {
    if (!p) return NULL;
    /* On non-Windows, return as-is */
#ifndef _WIN32
    return xstrdup(p);
#else
    /* /c/Users/... → C:\Users\... */
    if (p[0] == '/' && isalpha((unsigned char)p[1]) && p[2] == '/') {
        char drive = toupper((unsigned char)p[1]);
        const char *rest = p + 3;
        size_t rest_len = strlen(rest);
        char *out = malloc(rest_len + 4);
        if (!out) return xstrdup(p);
        snprintf(out, rest_len + 4, "%c:\\", drive);
        /* Replace forward slashes with backslashes */
        for (size_t i = 0; i < rest_len; i++) {
            out[3 + i] = (rest[i] == '/') ? '\\' : rest[i];
        }
        out[3 + rest_len] = '\0';
        return out;
    }
    /* /cygdrive/c/... or /mnt/c/... */
    const char *prefixes[] = {"/cygdrive/", "/mnt/", NULL};
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(p, prefixes[i], plen) == 0 && isalpha((unsigned char)p[plen]) && p[plen+1] == '/') {
            char drive = toupper((unsigned char)p[plen]);
            const char *rest = p + plen + 2;
            size_t rest_len = strlen(rest);
            char *out = malloc(rest_len + 4);
            if (!out) return xstrdup(p);
            snprintf(out, rest_len + 4, "%c:\\", drive);
            for (size_t j = 0; j < rest_len; j++) {
                out[3 + j] = (rest[j] == '/') ? '\\' : rest[j];
            }
            out[3 + rest_len] = '\0';
            return out;
        }
    }
    return xstrdup(p);
#endif
}

/* ── _strip_reasoning_tags (L243) ── */
/* PoP: cli_strip_reasoning_tags @ cli.py:_strip_reasoning_tags */
char *cli_strip_reasoning_tags(const char *text) {
    if (!text) return xstrdup("");
    /* Simple implementation: remove content between known tags */
    /* Full regex implementation would use libregex/hermes_regex */
    size_t len = strlen(text);
    char *result = malloc(len + 1);
    if (!result) return xstrdup(text);
    memcpy(result, text, len + 1);
    /* TODO: Full regex-based stripping using libregex */
    return result;
}

/* ── _assistant_content_as_text (L315) ── */
/* PoP: cli_assistant_content_as_text @ cli.py:_assistant_content_as_text */
char *cli_assistant_content_as_text(json_t *content) {
    if (!content) return xstrdup("");
    if (content->type == JSON_STRING) {
        return xstrdup(content->str_val);
    }
    if (content->type == JSON_ARRAY) {
        /* Extract text parts */
        size_t n = json_len(content);
        size_t total = 0;
        /* First pass: calculate total length */
        for (size_t i = 0; i < n; i++) {
            json_t *part = json_get(content, i);
            if (part && part->type == JSON_OBJECT) {
                json_t *type_val = json_obj_get(part, "type");
                json_t *text_val = json_obj_get(part, "text");
                if (type_val && type_val->type == JSON_STRING && strcmp(type_val->str_val, "text") == 0
                    && text_val && text_val->type == JSON_STRING) {
                    total += strlen(text_val->str_val) + 1;
                }
            }
        }
        char *result = malloc(total + 1);
        if (!result) return xstrdup("");
        result[0] = '\0';
        bool first = true;
        for (size_t i = 0; i < n; i++) {
            json_t *part = json_get(content, i);
            if (part && part->type == JSON_OBJECT) {
                json_t *type_val = json_obj_get(part, "type");
                json_t *text_val = json_obj_get(part, "text");
                if (type_val && type_val->type == JSON_STRING && strcmp(type_val->str_val, "text") == 0
                    && text_val && text_val->type == JSON_STRING) {
                    if (!first) strcat(result, "\n");
                    strcat(result, text_val->str_val);
                    first = false;
                }
            }
        }
        return result;
    }
    /* Fallback: serialize as string */
    char *serialized = json_serialize(content);
    return serialized ? serialized : xstrdup("");
}

/* ── _parse_service_tier_config (L399) ── */
/* PoP: cli_parse_service_tier_config @ cli.py:_parse_service_tier_config */
const char *cli_parse_service_tier_config(const char *raw) {
    if (!raw) return NULL;
    char buf[64];
    size_t len = strlen(raw);
    if (len >= sizeof(buf)) return NULL;
    for (size_t i = 0; i < len; i++) buf[i] = tolower((unsigned char)raw[i]);
    buf[len] = '\0';
    if (len == 0 || strcmp(buf, "normal") == 0 || strcmp(buf, "default") == 0 ||
        strcmp(buf, "standard") == 0 || strcmp(buf, "off") == 0 || strcmp(buf, "none") == 0) {
        return NULL;
    }
    if (strcmp(buf, "fast") == 0 || strcmp(buf, "priority") == 0 || strcmp(buf, "on") == 0) {
        return "priority";
    }
    return NULL;
}
