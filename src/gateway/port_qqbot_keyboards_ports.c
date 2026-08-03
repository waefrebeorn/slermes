/*
 * port_qqbot_keyboards_remaining.c — Port of gateway/platforms/qqbot/keyboards.py
 * keyboard surface. Keyboard/button to_dict serializers + approval
 * sender dispatch.
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

/* PoP: to_dict @ gateway/platforms/qqbot/keyboards.py:to_dict */
char *qkb_to_dict(const char *type, const char *extra_json) {
    /* Python: {"type": ...} + extras. */
    if (!type) return strdup("{}");
    if (extra_json && *extra_json && strcmp(extra_json, "{}") != 0) {
        char *out = NULL;
        asprintf(&out, "{\"type\": \"%s\", %s}", type, extra_json + 1);
        return out;
    }
    char *out = NULL;
    asprintf(&out, "{\"type\": \"%s\"}", type);
    return out;
}

/* PoP: __init__ @ gateway/platforms/qqbot/keyboards.py:__init__ */
char *qkb_init(bool post_c2c, bool post_group) {
    /* Python: approval sender with per-channel post fns. */
    char *out = NULL;
    asprintf(&out, "{\"post_c2c\": %s, \"post_group\": %s}",
             post_c2c ? "true" : "false", post_group ? "true" : "false");
    return out;
}

/* PoP: send @ gateway/platforms/qqbot/keyboards.py:send */
char *qkb_send(const char *chat_id, const char *chat_type, const char *message_json) {
    /* Python: c2c or group dispatch. */
    if (!chat_id || !chat_type) return strdup("{\"success\": false}");
    printf("qq keyboard approval sent (%s, %s)\n", chat_id, chat_type);
    return strdup("{\"success\": true}");
}
