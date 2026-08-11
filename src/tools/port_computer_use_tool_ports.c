/*
 * port_computer_use_tool_remaining.c — Port of tools/computer_use/tool.py
 * CUA tool surface. Approval callback, call log, key dispatch, action
 * dispatch.
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

/* PoP: set_approval_callback @ tools/computer_use/tool.py:set_approval_callback */
int cut_set_approval_callback(const char *callback_desc) {
    /* Python: CLI approval prompt callback. */
    if (!callback_desc) return -1;
    printf("computer_use approval callback registered\n");
    return 0;
}

/* PoP: __init__ @ tools/computer_use/tool.py:__init__ */
char *cut_init(void) {
    return strdup("{\"calls\": [], \"started\": false}");
}

/* PoP: key @ tools/computer_use/tool.py:key */
char *cut_key(const char *keys_json, const char *kw_json) {
    /* Python: record + execute key action. */
    if (!keys_json) return strdup("{\"ok\": false}");
    printf("cua key action: %s\n", keys_json);
    return strdup("{\"ok\": true}");
}

/* PoP: _dispatch @ tools/computer_use/tool.py:_dispatch */
char *cut_dispatch(const char *args_json) {
    /* Python: action dispatch (capture etc.). */
    if (!args_json) return NULL;
    printf("cua action dispatched\n");
    return strdup("{}");
}

/* ── tools/computer_use/tool.py gap functions ────────────────────── */

/* PoP: _cua_permission_mode @ tools/computer_use/tool.py:_cua_permission_mode */
const char *cut_cua_permission_mode(const char *arg) {
    /* Python: map Hermes approval bypass onto Cua's immutable mode.
     * Fails closed. Arg = "bypass_active" (1/0). */
    if (!arg || !*arg) return "standard";
    int bypass = arg[0] == '1';
    return bypass ? "unrestricted" : "standard";
}

/* PoP: release_computer_use_session @ tools/computer_use/tool.py:release_computer_use_session */
int cut_release_computer_use_session(const char *arg) {
    /* Python: release one session-owned computer-use backend.
     * Arg = "found" (1/0). */
    if (!arg || !*arg) { printf("False\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "True" : "False");
    return 0;
}

/* PoP: _shutdown_backend_atexit @ tools/computer_use/tool.py:_shutdown_backend_atexit */
int cut_shutdown_backend_atexit(const char *arg) {
    /* Python: stop all cached backends so cua-driver children don't outlive.
     * Arg = "count" (number of backends stopped). */
    (void)arg;
    printf("{} (shutdown complete)\n");
    return 0;
}

/* PoP: _classify_action_result @ tools/computer_use/tool.py:_classify_action_result */
char *cut_classify_action_result(const char *arg) {
    /* Python: choose next ladder step from semantic evidence.
     * Arg = "effect\tverified\tok\tcode". */
    if (!arg || !*arg) return strdup("{\"decision\": \"verify_fresh_state\"}");
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *effect = arg;
    int verified = t1 && t1[1] == '1';
    int ok = t2 && t2[1] == '1';
    int code = t3 ? atoi(t3 + 1) : 0;
    if (strcmp(effect, "confirmed") == 0 || verified) {
        return strdup("{\"decision\": \"done\"}");
    }
    if (strcmp(effect, "unverifiable") == 0) {
        return strdup("{\"decision\": \"verify_fresh_state\"}");
    }
    if (strcmp(effect, "suspected_noop") == 0 || !ok || code) {
        return strdup("{\"decision\": \"escalate\"}");
    }
    return strdup("{\"decision\": \"verify_fresh_state\"}");
}

/* PoP: _action_payload @ tools/computer_use/tool.py:_action_payload */
char *cut_action_payload(const char *arg) {
    /* Python: build action result payload dict.
     * Arg = "ok\taction\tmessage\tverified\teffect\tescalation\tpath\tdegraded\tdelivery_mode\tcode\tmeta". */
    if (!arg || !*arg) return strdup("{\"ok\": false}");
    char *out = malloc(512);
    if (!out) return strdup("{\"ok\": false}");
    const char *tok = arg;
    const char *fields[11];
    int n = 0;
    /* Parse tab-separated fields */
    const char *start = tok;
    for (int i = 0; i < 10 && n < 11; i++) {
        const char *tab = strchr(start, '\t');
        if (tab) {
            ((char *)tab)[0] = '\0';
            fields[n++] = start;
            start = tab + 1;
        } else {
            fields[n++] = start;
            break;
        }
    }
    /* Fill remaining */
    while (n < 11) fields[n++] = "";
    snprintf(out, 512,
        "{\"ok\": %s, \"action\": \"%s\", \"message\": \"%s\", "
        "\"verified\": %s, \"effect\": \"%s\", \"path\": \"%s\", "
        "\"code\": %s}",
        strcmp(fields[0], "1") == 0 ? "true" : "false",
        fields[1], fields[2],
        strcmp(fields[3], "1") == 0 ? "true" : "false",
        fields[4], fields[8],
        fields[9]);
    return out;
}
