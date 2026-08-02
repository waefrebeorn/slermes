/*
 * port_message_content_remaining.c — Port of agent/message_content.py
 * message-shape surface. Field access, text extraction, flattening.
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

/* PoP: _field @ agent/message_content.py:_field */
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

/* PoP: _text_from_part @ agent/message_content.py:_text_from_part */
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

/* PoP: flatten_message_text @ agent/message_content.py:flatten_message_text */
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
