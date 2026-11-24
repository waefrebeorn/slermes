/*
 * run_turn_runner_approval_transport.c -- real C11 port of the surface decision
 * of upstream Hermes gateway/run_turn_runner_approval_transport.py.
 *
 * PoP: w21_runturnrun_ac2ad3_surfaces @ gateway/run_turn_runner_approval_transport.py:NON_MESSAGING_SESSION_SURFACES
 * PoP: w21_runturnrun_ac2ad3_platform_value @ gateway/run_turn_runner_approval_transport.py:approval_transport
 * PoP: w21_runturnrun_ac2ad3_transport @ gateway/run_turn_runner_approval_transport.py:approval_transport
 *
 * `approval_transport` clones the turn's `TurnContext`, builds a detached
 * `TurnRunner` presenter and returns the `notify` closure; all of that needs the
 * live turn, its adapter resolver and the run-generation clock.  The decision
 * the module exists for -- whether a background child's approval can still reach
 * the chat it came from -- is what is ported, together with the surface table it
 * reads from `gateway.session_context`.
 *
 * The table is matched exactly as the module does: no strip, no lower-case.  A
 * gateway platform `" Telegram "` is therefore a messaging surface (it is not in
 * the table), which is the opposite of what
 * `session_context.session_is_messaging_surface` would answer for the same text.
 */
#include "hermes_json.h"
#include "noc_w21_runturnrun_ac2ad3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* gateway/session_context.py: NON_MESSAGING_SESSION_SURFACES, sorted. */
static const char *const RTAT_SURFACES[] = {
    "", "api_server", "cli", "codex", "desktop", "gateway", "kanban", "local",
    "msgraph_webhook", "tool", "tui", "webhook",
};

static char *rtat_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

json_t *w21_runturnrun_ac2ad3_surfaces(void) {
    json_t *arr = json_array();
    size_t i;
    if (!arr) return NULL;
    for (i = 0; i < sizeof(RTAT_SURFACES) / sizeof(RTAT_SURFACES[0]); i++)
        json_append(arr, json_string(RTAT_SURFACES[i]));
    return arr;
}

/* `str(x or "")`: falsiness first (None, "", 0, False read as ""), then str(). */
static char *rtat_str(const json_t *v) {
    char buf[64];
    if (!v) return rtat_dup("");
    switch (v->type) {
    case JSON_NULL:   return rtat_dup("");
    case JSON_BOOL:   return rtat_dup(v->bool_val ? "True" : "");
    case JSON_NUMBER: {
        if (v->num_val == 0.0) return rtat_dup("");
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return rtat_dup(buf);
    }
    case JSON_STRING: return rtat_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:
    case JSON_OBJECT: return rtat_dup("");
    }
    return rtat_dup("");
}

char *w21_runturnrun_ac2ad3_platform_value(const json_t *source) {
    /* getattr(source, "platform", None): only a JSON object carries attributes. */
    json_t *platform;
    if (!source || source->type != JSON_OBJECT) return rtat_dup("");
    platform = json_obj_get(source, "platform");
    /* getattr(platform, "value", ""): a str/int/None has no `.value`. */
    if (!platform || platform->type != JSON_OBJECT) return rtat_dup("");
    return rtat_str(json_obj_get(platform, "value"));
}

static int rtat_non_messaging(const char *platform) {
    size_t i;
    for (i = 0; i < sizeof(RTAT_SURFACES) / sizeof(RTAT_SURFACES[0]); i++)
        if (!strcmp(platform, RTAT_SURFACES[i])) return 1;
    return 0;
}

json_t *w21_runturnrun_ac2ad3_transport(const json_t *source, int local_viewer) {
    json_t *out = json_object();
    char *platform = w21_runturnrun_ac2ad3_platform_value(source);
    int attached;
    if (!out) {
        free(platform);
        return NULL;
    }
    attached = (!rtat_non_messaging(platform ? platform : "")) && !local_viewer;
    json_set(out, "platform", json_string(platform ? platform : ""));
    json_set(out, "attached", json_bool(attached));
    free(platform);
    return out;
}
