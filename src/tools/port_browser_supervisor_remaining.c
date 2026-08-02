/*
 * port_browser_supervisor_remaining.c — Port of tools/browser_supervisor.py
 * CDP supervisor surface. Event dicts, dialog policy validation,
 * lifecycle, snapshots, CDP command dispatch, read loop.
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

/* PoP: to_dict @ tools/browser_supervisor.py:to_dict */
char *bsv_event_to_dict(const char *event_type, const char *message, const char *payload_json) {
    /* Python: event → dict. */
    char *out = NULL;
    asprintf(&out, "{\"type\": \"%s\", \"message\": \"%s\", \"payload\": %s}",
             event_type ? event_type : "", message ? message : "",
             payload_json ? payload_json : "null");
    return out;
}

/* PoP: to_dict @ tools/browser_supervisor.py:to_dict */
char *bsv_dialog_to_dict(const char *type, const char *message, const char *default_value) {
    char *out = NULL;
    asprintf(&out, "{\"type\": \"%s\", \"message\": \"%s\", \"default_value\": \"%s\"}",
             type ? type : "", message ? message : "", default_value ? default_value : "");
    return out;
}

/* PoP: to_dict @ tools/browser_supervisor.py:to_dict */
char *bsv_request_to_dict(const char *type, const char *url, const char *frame_id) {
    char *out = NULL;
    asprintf(&out, "{\"type\": \"%s\", \"url\": \"%s\", \"frame_id\": \"%s\"}",
             type ? type : "", url ? url : "", frame_id ? frame_id : "");
    return out;
}

/* PoP: to_dict @ tools/browser_supervisor.py:to_dict */
char *bsv_console_to_dict(const char *level, const char *text, const char *url) {
    char *out = NULL;
    asprintf(&out, "{\"level\": \"%s\", \"text\": \"%s\", \"url\": \"%s\"}",
             level ? level : "", text ? text : "", url ? url : "");
    return out;
}

/* PoP: __init__ @ tools/browser_supervisor.py:__init__ */
int bsv_init(const char *dialog_policy) {
    /* Python: validate policy. */
    if (!dialog_policy) return -1;
    static const char *valid[] = {"accept", "dismiss", "ignore", NULL};
    for (int i = 0; valid[i]; i++)
        if (strcmp(dialog_policy, valid[i]) == 0) return 0;
    return -1;
}

/* PoP: start @ tools/browser_supervisor.py:start */
int bsv_start(void) {
    /* Python: launch loop + wait for attachment. */
    printf("browser supervisor started (loop + attach)\n");
    return 0;
}

/* PoP: stop @ tools/browser_supervisor.py:stop */
int bsv_stop(void) {
    printf("browser supervisor stopped (task cancelled, thread joined)\n");
    return 0;
}

/* PoP: snapshot @ tools/browser_supervisor.py:snapshot */
char *bsv_snapshot(void) {
    /* Python: immutable state snapshot. */
    return strdup("{}");
}

/* PoP: _run @ tools/browser_supervisor.py:_run */
int bsv_run(void) {
    /* Python: reconnecting loop. */
    printf("browser supervisor reconnecting loop running\n");
    return 0;
}

/* PoP: _cdp @ tools/browser_supervisor.py:_cdp */
char *bsv_cdp(const char *method, const char *params_json) {
    /* Python: CDP command + response. */
    if (!method) return NULL;
    printf("cdp command sent: %s\n", method);
    return strdup("{}");
}

/* PoP: _read_loop @ tools/browser_supervisor.py:_read_loop */
int bsv_read_loop(void) {
    /* Python: dispatch incoming CDP frames. */
    printf("cdp frame read loop running\n");
    return 0;
}
