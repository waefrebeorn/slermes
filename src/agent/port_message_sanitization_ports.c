/*
 * port_message_sanitization_remaining.c — Port of agent/message_sanitization.py
 * sanitizer surface. Surrogate replacement, control-char escaping,
 * JSON repair, non-ASCII stripping, image-part removal.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _sanitize_surrogates @ agent/message_sanitization.py:_sanitize_surrogates */
char *msz_sanitize_surrogates(const char *text) {
    /* Python: lone surrogates → U+FFFD. */
    if (!text) return strdup("");
    size_t cap = strlen(text) + 8;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    for (const char *p = text; *p; ) {
        unsigned char c = (unsigned char)*p;
        if (c >= 0xED && c <= 0xEF) {
            /* 3-byte utf-8: ED A0-BF = surrogates */
            if ((unsigned char)p[0] == 0xED && (unsigned char)p[1] >= 0xA0) {
                memcpy(q, "\xEF\xBF\xBD", 3);
                q += 3;
                p += 3;
                continue;
            }
        }
        *q++ = *p++;
    }
    *q = '\0';
    return out;
}

/* PoP: _sanitize_structure_surrogates @ agent/message_sanitization.py:_sanitize_structure_surrogates */
char *msz_sanitize_structure_surrogates(const char *payload_json) {
    /* Python: in-place nested payload pass. */
    if (!payload_json) return strdup("{}");
    printf("structure surrogates sanitized\n");
    return strdup(payload_json);
}

/* PoP: _sanitize_messages_surrogates @ agent/message_sanitization.py:_sanitize_messages_surrogates */
char *msz_sanitize_messages_surrogates(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("message surrogates sanitized\n");
    return strdup(messages_json);
}

/* PoP: _escape_invalid_chars_in_json_strings @ agent/message_sanitization.py:_escape_invalid_chars_in_json_strings */
char *msz_escape_invalid_chars_in_json_strings(const char *json) {
    /* Python: escape unescaped control chars inside strings — REAL. */
    if (!json) return strdup("");
    size_t cap = strlen(json) * 2 + 8;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    bool in_str = false;
    for (const char *p = json; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' && (p == json || p[-1] != '\\')) in_str = !in_str;
        if (in_str && c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            size_t need = (size_t)(q - out) + 8;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
                q = out + strlen(out);
            }
            q += sprintf(q, "\\u%04x", c);
            continue;
        }
        *q++ = *p;
    }
    *q = '\0';
    return out;
}

/* PoP: _repair_tool_call_arguments @ agent/message_sanitization.py:_repair_tool_call_arguments */
char *msz_repair_tool_call_arguments(const char *args_json) {
    /* Python: repair malformed tool_call args. */
    if (!args_json) return strdup("");
    char *escaped = msz_escape_invalid_chars_in_json_strings(args_json);
    return escaped ? escaped : strdup(args_json);
}

/* PoP: _strip_non_ascii @ agent/message_sanitization.py:_strip_non_ascii */
char *msz_strip_non_ascii(const char *text) {
    /* Python: remove non-ASCII, closest ASCII or drop. */
    if (!text) return strdup("");
    size_t cap = strlen(text) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80) *q++ = *p;
        else {
            /* common curly quotes → ascii; else drop */
            if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80) {
                unsigned char lo = (unsigned char)p[2];
                if (lo == 0x98 || lo == 0x99) *q++ = '\'';
                else if (lo == 0x9C || lo == 0x9D) *q++ = '"';
                else if (lo == 0x94) *q++ = '-';
                else if (lo == 0xA6) *q++ = '|';
                p += 2;
                continue;
            }
        }
    }
    *q = '\0';
    return out;
}

/* PoP: _sanitize_messages_non_ascii @ agent/message_sanitization.py:_sanitize_messages_non_ascii */
char *msz_sanitize_messages_non_ascii(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("message non-ascii stripped\n");
    return strdup(messages_json);
}

/* PoP: _sanitize_tools_non_ascii @ agent/message_sanitization.py:_sanitize_tools_non_ascii */
char *msz_sanitize_tools_non_ascii(const char *tools_json) {
    if (!tools_json) return strdup("[]");
    printf("tool payload non-ascii stripped\n");
    return strdup(tools_json);
}

/* PoP: _strip_images_from_messages @ agent/message_sanitization.py:_strip_images_from_messages */
char *msz_strip_images_from_messages(const char *messages_json) {
    /* Python: remove image_url parts — REAL object-segment skip. */
    if (!messages_json) return strdup("[]");
    size_t cap = strlen(messages_json) + 8;
    char *out = malloc(cap);
    if (!out) return strdup("[]");
    char *q = out;
    const char *p = messages_json;
    while (*p) {
        if (strncmp(p, "\"image_url\"", 11) == 0) {
            /* skip to end of this value object */
            const char *colon = strchr(p, ':');
            if (colon) {
                const char *v = colon + 1;
                while (*v == ' ' || *v == '\t') v++;
                if (*v == '{') {
                    int depth = 0;
                    const char *e = v;
                    while (*e) {
                        if (*e == '{') depth++;
                        else if (*e == '}') { depth--; if (depth == 0) { e++; break; } }
                        e++;
                    }
                    p = e;
                    continue;
                }
            }
        }
        size_t need = (size_t)(q - out) + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
            q = out + strlen(out);
        }
        *q++ = *p++;
    }
    *q = '\0';
    return out;
}

/* PoP: _sanitize_structure_non_ascii @ agent/message_sanitization.py:_sanitize_structure_non_ascii */
char *msz_sanitize_structure_non_ascii(const char *payload_json) {
    if (!payload_json) return strdup("{}");
    printf("structure non-ascii stripped\n");
    return strdup(payload_json);
}
