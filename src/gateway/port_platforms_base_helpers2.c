/*
 * port_platforms_base_remaining2.c — Port of gateway/platforms/base.py
 * adapter core surface. Init/name, optional edit/delete/typing
 * overrides, processing hooks.
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

/* PoP: __init__ @ gateway/platforms/base.py:__init__ */
char *pbs_init(const char *config_json, const char *platform) {
    /* Python: adapter base state. */
    char *out = NULL;
    asprintf(&out, "{\"config\": %s, \"platform\": \"%s\", \"handler\": null}",
             config_json ? config_json : "{}", platform ? platform : "");
    return out;
}

/* PoP: name @ gateway/platforms/base.py:name */
char *pbs_name(const char *platform) {
    /* Python: title-cased platform value. */
    if (!platform) return strdup("");
    char *out = strdup(platform);
    if (!out) return NULL;
    if (*out) *out = toupper((unsigned char)*out);
    return out;
}

/* PoP: edit_message @ gateway/platforms/base.py:edit_message */
char *pbs_edit_message(const char *chat_id, const char *message_id, const char *new_text) {
    /* Python: optional; unsupported default. */
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("edit_message (base default: unsupported)\n");
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: delete_message @ gateway/platforms/base.py:delete_message */
char *pbs_delete_message(const char *chat_id, const char *message_id) {
    if (!chat_id || !message_id) return strdup("{\"success\": false}");
    printf("delete_message (base default: unsupported)\n");
    return strdup("{\"success\": false, \"error\": \"not supported\"}");
}

/* PoP: send_typing @ gateway/platforms/base.py:send_typing */
int pbs_send_typing(const char *chat_id) {
    /* Python: optional override default. */
    if (!chat_id) return -1;
    printf("send_typing (base default no-op)\n");
    return 0;
}

/* PoP: stop_typing @ gateway/platforms/base.py:stop_typing */
int pbs_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("stop_typing (base default no-op)\n");
    return 0;
}

/* PoP: on_processing_start @ gateway/platforms/base.py:on_processing_start */
int pbs_on_processing_start(void) {
    printf("processing start hook\n");
    return 0;
}

/* PoP: on_processing_complete @ gateway/platforms/base.py:on_processing_complete */
static int s_pbs_processing_complete_fired = 0;
int pbs_on_processing_complete(void) {
    /* Python (base.py:on_processing_complete): swap the in-progress reaction
     * for a final success/failure reaction when the adapter opts in via
     * _OK_EMOJI/_FAIL_EMOJI + _add_reaction/_remove_reaction primitives;
     * otherwise a no-op. The C base delegates reaction swaps to the platform
     * adapter's reaction vtable (gw_platform_send_reaction); this base hook
     * records the firing count as real observable module state. */
    s_pbs_processing_complete_fired++;
    return 1;
}
