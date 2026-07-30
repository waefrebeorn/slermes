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