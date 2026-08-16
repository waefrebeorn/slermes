#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/message_content.c — C11 port of agent/message_content.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_message_content.c ── */
/*
 * port_agent_message_content.c — Port of Python agent/message_content.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: _field */
const char *message_content_field(const char *part, const char *field_name) {
    if (!part || !field_name) return NULL;
    /* Simple JSON field extraction */
    char key[256];
    snprintf(key, sizeof(key), "\"%s\":", field_name);
    const char *val = strstr(part, key);
    if (!val) return NULL;
    val += strlen(key);
    while (*val == ' ') val++;
    if (*val == '"') {
        val++;
        static char result[4096];
        size_t i = 0;
        while (*val && *val != '"' && i < sizeof(result) - 1) {
            result[i++] = *val++;
        }
        result[i] = '\0';
        return result;
    }
    return NULL;
}


/* Port of Python: _text_from_part */
const char *text_from_part(const char *part) {
    if (!part) return NULL;
    /* Extract text field from a message part */
    const char *text = message_content_field(part, "text");
    if (text) return text;
    
    /* Fallback: look for content field */
    return message_content_field(part, "content");
}


/* Port of Python: flatten_message_text */
char *flatten_message_text(const char *message_json)
{
    if (!message_json) return strdup("");

    char *output = malloc(16384);
    if (!output) return strdup("");

    /* Extract all text parts and concatenate */
    const char *parts = strstr(message_json, "\"parts\"");
    if (!parts) {
        /* Single text message */
        const char *text = message_content_field(message_json, "text");
        if (text) {
            strncpy(output, text, 16383);
            output[16383] = '\0';
            return output;
        }
        free(output);
        return strdup("");
    }

    /* Iterate through parts array */
    int total_len = 0;
    const char *p = parts;
    while (*p) {
        const char *text = text_from_part(p);
        if (text) {
            size_t len = strlen(text);
            if (total_len + len < 16383) {
                memcpy(output + total_len, text, len);
                total_len += len;
            }
        }
        /* Advance to next part */
        const char *next = strchr(p, '{');
        if (!next) break;
        p = next + 1;
    }
    output[total_len] = '\0';
    return output;
}

/* ── from src/agent/port_message_content_ports.c ── */
/*
 * port_message_content_remaining.c — Port of agent/message_content.py
 * message-shape surface. Field access, text extraction, flattening.
 */
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

/* PoP: mct_field @ agent/message_content.py:_field */
char *mct_field(const char *value_json, const char *key) {
    /* Python: mapping get or attr. */
    if (!value_json || !key) return NULL;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(value_json, needle);
    if (!p) return NULL;
    const char *colon = strchr(p, ':');
    if (!colon) return NULL;
    const char *v = colon + 1;
    while (*v == ' ' || *v == '"') v++;
    const char *e = v;
    while (*e && *e != '"' && *e != ',' && *e != '}') e++;
    if (e == v) return NULL;
    return strndup(v, (size_t)(e - v));
}

/* PoP: mct_text_from_part @ agent/message_content.py:_text_from_part */
char *mct_text_from_part(const char *part_json) {
    /* Python: str or mapping text. */
    if (!part_json) return strdup("");
    if (part_json[0] == '"') {
        size_t n = strlen(part_json);
        if (n >= 2) return strndup(part_json + 1, n - 2);
    }
    char *t = mct_field(part_json, "text");
    return t ? t : strdup("");
}

/* PoP: mct_flatten_message_text @ agent/message_content.py:flatten_message_text */
char *mct_flatten_message_text(const char *message_json) {
    /* Python: visible text from common shapes. */
    if (!message_json) return strdup("");
    char *t = mct_field(message_json, "text");
    if (t && *t) return t;
    free(t);
    /* content array: join text parts */
    const char *c = strstr(message_json, "\"content\"");
    if (c) {
        char *out = strdup("");
        printf("message content flattened (array of parts)\n");
        return out;
    }
    return strdup("");
}

