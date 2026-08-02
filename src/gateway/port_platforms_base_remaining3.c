/*
 * port_platforms_base_remaining3.c — Port of gateway/platforms/base.py
 * adapter overrides. Optional edit/delete/typing/processing hooks with
 * safe base defaults.
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

/* PoP: edit_message @ gateway/platforms/base.py:edit_message */
char *pbs3_edit_message(const char *chat_id, const char *message_id, const char *new_text) {
    /* Python: optional; unsupported default. */
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: delete_message @ gateway/platforms/base.py:delete_message */
char *pbs3_delete_message(const char *chat_id, const char *message_id) {
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: send_typing @ gateway/platforms/base.py:send_typing */
int pbs3_send_typing(const char *chat_id) {
    if (!chat_id) return -1;
    return 0;
}

/* PoP: stop_typing @ gateway/platforms/base.py:stop_typing */
int pbs3_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    return 0;
}

/* PoP: on_processing_start @ gateway/platforms/base.py:on_processing_start */
int pbs3_on_processing_start(void) {
    return 0;
}

/* PoP: on_processing_complete @ gateway/platforms/base.py:on_processing_complete */
int pbs3_on_processing_complete(void) {
    return 0;
}
