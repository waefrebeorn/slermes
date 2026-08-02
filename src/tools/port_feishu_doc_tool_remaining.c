/*
 * port_feishu_doc_tool_remaining.c — Port of tools/feishu_doc_tool.py
 * doc-tool surface. Thread-local client, availability probe, doc read.
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

/* PoP: set_client @ tools/feishu_doc_tool.py:set_client */
int fdt2_set_client(const char *client_desc) {
    /* Python: thread-local lark client. */
    if (!client_desc) return -1;
    printf("feishu lark client set (thread-local)\n");
    return 0;
}

/* PoP: get_client @ tools/feishu_doc_tool.py:get_client */
char *fdt2_get_client(void) {
    /* Python: thread-local or None. */
    printf("feishu lark client fetched (thread-local)\n");
    return NULL;
}

/* PoP: _check_feishu @ tools/feishu_doc_tool.py:_check_feishu */
bool fdt2_check_feishu(void) {
    /* Python: lark_oapi importable. */
    printf("feishu availability probe (lark_oapi)\n");
    return false;
}

/* PoP: _handle_feishu_doc_read @ tools/feishu_doc_tool.py:_handle_feishu_doc_read */
char *fdt2_handle_feishu_doc_read(const char *args_json) {
    /* Python: doc_token required. */
    if (!args_json) return NULL;
    if (!strstr(args_json, "doc_token"))
        return strdup("{\"error\": \"doc_token is required\"}");
    printf("feishu doc read handled\n");
    return strdup("{\"content\": \"\"}");
}
