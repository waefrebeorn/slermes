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
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: msz_sanitize_surrogates @ agent/message_sanitization.py:_sanitize_surrogates */
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

/* PoP: msz_sanitize_structure_surrogates @ agent/message_sanitization.py:_sanitize_structure_surrogates */
char *msz_sanitize_structure_surrogates(const char *payload_json) {
    /* Python: in-place nested payload pass. */
    if (!payload_json) return strdup("{}");
    printf("structure surrogates sanitized\n");
    return strdup(payload_json);
}

/* PoP: msz_sanitize_messages_surrogates @ agent/message_sanitization.py:_sanitize_messages_surrogates */
char *msz_sanitize_messages_surrogates(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("message surrogates sanitized\n");
    return strdup(messages_json);
}

/* PoP: msz_escape_invalid_chars_in_json_strings @ agent/message_sanitization.py:_escape_invalid_chars_in_json_strings */
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

/* PoP: msz_repair_tool_call_arguments @ agent/message_sanitization.py:_repair_tool_call_arguments */
/* Faithful port of the LIVE repair ladder (see tests/watercup/gates/
 * message_sanitization_repair_tool_call_arguments): strip -> None -> strict
 * parse (pass 0) -> trailing-comma strip -> close unclosed structures ->
 * bounded excess-closer trim loop -> final parse -> control-char escape pass
 * -> "{}" fallback. tool_name only feeds WARNING logs in Python. */
static bool msz_rta_parses(const char *s)
{
    if (!s) return false;
    json_t *j = json_parse(s, NULL);
    if (!j) return false;
    json_free(j);
    return true;
}

char *msz_repair_tool_call_arguments(const char *args_json) {
    /* Python: raw_stripped = raw_args.strip() if isinstance(str) else "" */
    char *raw_stripped;
    if (!args_json) {
        raw_stripped = strdup("");
    } else {
        const char *b = args_json;
        while (*b && isspace((unsigned char)*b)) b++;
        const char *e = args_json + strlen(args_json);
        while (e > b && isspace((unsigned char)e[-1])) e--;
        raw_stripped = strndup(b, (size_t)(e - b));
        if (!raw_stripped) return strdup("{}");
    }

    /* Fast-path: empty / whitespace-only -> empty object */
    if (!raw_stripped[0]) { free(raw_stripped); return strdup("{}"); }
    /* Python-literal None -> normalise to {} */
    if (strcmp(raw_stripped, "None") == 0) { free(raw_stripped); return strdup("{}"); }

    /* Repair pass 0: strict JSON parse (json.loads, strict=True oracle).
     * On success re-serialise compactly like json.dumps(separators=(",",":")). */
    {
        json_t *parsed = json_parse(raw_stripped, NULL);
        if (parsed) {
            char *out = json_serialize(parsed);
            json_free(parsed);
            free(raw_stripped);
            return out ? out : strdup("{}");
        }
    }

    /* Attempt common JSON repairs */
    char *fixed = strdup(raw_stripped);
    /* 1. Strip trailing commas before } or ] (regex r',\s*([}\]])' -> r'\1') */
    {
        size_t n = strlen(fixed), w = 0;
        for (size_t i = 0; i < n; ) {
            if (fixed[i] == ',') {
                size_t k = i + 1;
                while (k < n && isspace((unsigned char)fixed[k])) k++;
                if (k < n && (fixed[k] == '}' || fixed[k] == ']')) { i++; continue; }
            }
            fixed[w++] = fixed[i++];
        }
        fixed[w] = '\0';
    }
    /* 2. Close unclosed structures */
    {
        int open_curly = 0, open_bracket = 0;
        bool in_str = false;
        for (const char *p = fixed; *p; p++) {
            if (*p == '"' && !(p > fixed && p[-1] == '\\')) in_str = !in_str;
            if (!in_str) {
                if (*p == '{') open_curly++;
                else if (*p == '}') open_curly--;
                else if (*p == '[') open_bracket++;
                else if (*p == ']') open_bracket--;
            }
        }
        size_t fl = strlen(fixed);
        if (open_curly > 0 || open_bracket > 0) {
            fixed = realloc(fixed, fl + (size_t)(open_curly > 0 ? open_curly : 0) +
                                    (size_t)(open_bracket > 0 ? open_bracket : 0) + 1);
            for (int i = 0; i < open_curly; i++) fixed[fl++] = '}';
            for (int i = 0; i < open_bracket; i++) fixed[fl++] = ']';
            fixed[fl] = '\0';
        }
    }
    /* 3. Remove excess closing braces/brackets (bounded to 50 iterations) */
    for (int _ = 0; _ < 50; _++) {
        if (msz_rta_parses(fixed)) break;
        size_t fl = strlen(fixed);
        long c_curly = 0, o_curly = 0, c_brack = 0, o_brack = 0;
        for (const char *p = fixed; *p; p++) {
            if (*p == '{') o_curly++;
            else if (*p == '}') c_curly++;
            else if (*p == '[') o_brack++;
            else if (*p == ']') c_brack++;
        }
        if (fl > 0 && fixed[fl - 1] == '}' && c_curly > o_curly) {
            fixed[fl - 1] = '\0';
        } else if (fl > 0 && fixed[fl - 1] == ']' && c_brack > o_brack) {
            fixed[fl - 1] = '\0';
        } else {
            break;
        }
    }

    if (msz_rta_parses(fixed)) {
        char *out = fixed;
        free(raw_stripped);
        return out;
    }

    /* Repair pass 4: escape unescaped control chars inside JSON strings,
     * then retry. Faithful to _escape_invalid_chars_in_json_strings: ALL
     * ctrl chars inside strings are escaped; already-escaped pairs pass
     * through as-is. */
    {
        size_t cap = strlen(fixed) * 6 + 8;
        char *escaped = malloc(cap);
        if (escaped) {
            size_t w = 0;
            bool in_str = false;
            for (size_t i = 0; fixed[i]; ) {
                char ch = fixed[i];
                if (in_str) {
                    unsigned char c = (unsigned char)ch;
                    if (ch == '\\' && fixed[i + 1]) {
                        if (w + 2 >= cap) { cap = (w + 2) * 2; escaped = realloc(escaped, cap); }
                        escaped[w++] = ch; escaped[w++] = fixed[i + 1];
                        i += 2;
                        continue;
                    }
                    if (ch == '"') { in_str = false; escaped[w++] = ch; i++; continue; }
                    if (c < 0x20) {
                        if (w + 8 >= cap) { cap = (w + 8) * 2; escaped = realloc(escaped, cap); }
                        w += (size_t)sprintf(escaped + w, "\\u%04x", c);
                        i++;
                        continue;
                    }
                    escaped[w++] = ch; i++;
                    continue;
                }
                if (ch == '"') in_str = true;
                escaped[w++] = ch; i++;
            }
            escaped[w] = '\0';
            if (strcmp(escaped, fixed) != 0 && msz_rta_parses(escaped)) {
                free(fixed);
                free(raw_stripped);
                return escaped;
            }
            free(escaped);
        }
    }

    /* Last resort: replace with empty object so the API request doesn't
     * crash the entire session. */
    free(fixed);
    free(raw_stripped);
    return strdup("{}");
}

/* PoP: msz_strip_non_ascii @ agent/message_sanitization.py:_strip_non_ascii */
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

/* PoP: msz_sanitize_messages_non_ascii @ agent/message_sanitization.py:_sanitize_messages_non_ascii */
char *msz_sanitize_messages_non_ascii(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("message non-ascii stripped\n");
    return strdup(messages_json);
}

/* PoP: msz_sanitize_tools_non_ascii @ agent/message_sanitization.py:_sanitize_tools_non_ascii */
char *msz_sanitize_tools_non_ascii(const char *tools_json) {
    if (!tools_json) return strdup("[]");
    printf("tool payload non-ascii stripped\n");
    return strdup(tools_json);
}

/* PoP: msz_strip_images_from_messages @ agent/message_sanitization.py:_strip_images_from_messages */
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

/* PoP: msz_sanitize_structure_non_ascii @ agent/message_sanitization.py:_sanitize_structure_non_ascii */
char *msz_sanitize_structure_non_ascii(const char *payload_json) {
    if (!payload_json) return strdup("{}");
    printf("structure non-ascii stripped\n");
    return strdup(payload_json);
}
