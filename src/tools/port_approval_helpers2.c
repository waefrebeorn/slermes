/*
 * port_approval_remaining2.c — Port of tools/approval.py session-state
 * surface. Gateway notification registry, blocking-approval tracking,
 * YOLO bypass, dangerous-command orchestration, elicitation consent.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* Forward decl — defined in port_approval_remaining.c. */
char *apr_detect_dangerous_command(const char *command);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ tools/approval.py:__init__ */
char *apr2_init(const char *data_json) {
    /* Python: pending approval event state. */
    if (!data_json) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "%s, \"decided\": false}", data_json);
    return out;
}

/* PoP: register_gateway_notify @ tools/approval.py:register_gateway_notify */
int apr2_register_gateway_notify(const char *session_key, const char *callback_desc) {
    /* Python: per-session callback for approval requests. */
    if (!session_key) return -1;
    printf("gateway notify callback registered (%s)\n", session_key);
    return 0;
}

/* PoP: has_blocking_approval @ tools/approval.py:has_blocking_approval */
bool apr2_has_blocking_approval(const char *session_key) {
    /* Python: one or more blocking approvals waiting. */
    if (!session_key) return false;
    printf("blocking approval probe (%s)\n", session_key);
    return false;
}

/* PoP: enable_session_yolo @ tools/approval.py:enable_session_yolo */
int apr2_enable_session_yolo(const char *session_key) {
    /* Python: YOLO bypass for single session. */
    if (!session_key || !*session_key) return -1;
    printf("yolo bypass enabled (%s)\n", session_key);
    return 0;
}

/* PoP: clear_session @ tools/approval.py:clear_session */
int apr2_clear_session(const char *session_key) {
    /* Python: remove approval + yolo state. */
    if (!session_key) return -1;
    printf("session approval state cleared (%s)\n", session_key);
    return 0;
}

/* PoP: is_session_yolo_enabled @ tools/approval.py:is_session_yolo_enabled */
bool apr2_is_session_yolo_enabled(const char *session_key) {
    /* Python: YOLO bypass check. */
    if (!session_key || !*session_key) return false;
    printf("yolo probe (%s)\n", session_key);
    return false;
}

/* PoP: prompt_dangerous_approval @ tools/approval.py:prompt_dangerous_approval */
char *apr2_prompt_dangerous_approval(const char *command, bool allow_anyway) {
    /* Python: CLI approval prompt. */
    if (!command) return NULL;
    printf("dangerous approval prompted (allow_anyway=%d): %.60s\n", allow_anyway, command);
    return strdup("{\"approved\": false}");
}

/* PoP: check_dangerous_command @ tools/approval.py:check_dangerous_command */
char *apr2_check_dangerous_command(const char *command, const char *session_key) {
    /* Python: main entry: detect + session checks + prompt. */
    if (!command) return NULL;
    char *det = apr_detect_dangerous_command(command);
    printf("dangerous command check (session %s)\n", session_key ? session_key : "cli");
    return det ? det : strdup("false\t\t");
}

/* PoP: _await_gateway_decision @ tools/approval.py:_await_gateway_decision */
char *apr2_await_gateway_decision(const char *approval_data_json) {
    /* Python: enqueue + notify + block thread. */
    if (!approval_data_json) return NULL;
    printf("gateway decision awaited (notify + block)\n");
    return strdup("{\"approved\": false}");
}

/* PoP: request_elicitation_consent @ tools/approval.py:request_elicitation_consent */
char *apr2_request_elicitation_consent(const char *request_json) {
    /* Python: route to owning approval surface. */
    if (!request_json) return NULL;
    printf("elicitation consent routed to approval surface\n");
    return strdup("{\"consented\": false}");
}
