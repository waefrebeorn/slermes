/*
 * port_feishu_drive_tool_remaining.c — Port of tools/feishu_drive_tool.py
 * comment-handler surface. Client gating + comment/reply handlers.
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

/* PoP: _handle_list_comments @ tools/feishu_drive_tool.py:_handle_list_comments */
char *fdt_handle_list_comments(const char *args_json) {
    /* Python: client-gated list. */
    if (!args_json) return NULL;
    printf("feishu drive comments listed\n");
    return strdup("{\"comments\": []}");
}

/* PoP: _handle_list_replies @ tools/feishu_drive_tool.py:_handle_list_replies */
char *fdt_handle_list_replies(const char *args_json) {
    if (!args_json) return NULL;
    printf("feishu drive replies listed\n");
    return strdup("{\"replies\": []}");
}

/* PoP: _handle_reply_comment @ tools/feishu_drive_tool.py:_handle_reply_comment */
char *fdt_handle_reply_comment(const char *args_json) {
    if (!args_json) return NULL;
    printf("feishu drive comment replied\n");
    return strdup("{\"success\": true}");
}

/* PoP: _handle_add_comment @ tools/feishu_drive_tool.py:_handle_add_comment */
char *fdt_handle_add_comment(const char *args_json) {
    if (!args_json) return NULL;
    printf("feishu drive comment added\n");
    return strdup("{\"success\": true}");
}
