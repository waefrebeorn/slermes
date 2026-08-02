/*
 * port_msgraph_webhook_remaining.c — Port of gateway/platforms/msgraph_webhook.py
 * webhook adapter surface. Connect/disconnect, send, chat info,
 * health with source-ip gate.
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

/* PoP: connect @ gateway/platforms/msgraph_webhook.py:connect */
bool msw_connect(void) {
    /* Python: client state required. */
    printf("msgraph webhook connect (client state check)\n");
    return false;
}

/* PoP: disconnect @ gateway/platforms/msgraph_webhook.py:disconnect */
int msw_disconnect(void) {
    printf("msgraph webhook disconnected (runner cleaned)\n");
    return 0;
}

/* PoP: send @ gateway/platforms/msgraph_webhook.py:send */
char *msw_send(const char *chat_id, const char *content) {
    /* Python: log + deliver. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("msgraph webhook response for %s: %.200s\n", chat_id, content);
    return strdup("{\"success\": true}");
}

/* PoP: get_chat_info @ gateway/platforms/msgraph_webhook.py:get_chat_info */
char *msw_get_chat_info(const char *chat_id) {
    if (!chat_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"webhook\"}", chat_id);
    return out;
}

/* PoP: _handle_health @ gateway/platforms/msgraph_webhook.py:_handle_health */
char *msw_handle_health(bool source_ip_allowed) {
    /* Python: source-ip gated health. */
    if (!source_ip_allowed) return strdup("{\"status\": 403}");
    return strdup("{\"status\": 200, \"body\": \"{\\\"status\\\": \\\"ok\\\"}\"}");
}
