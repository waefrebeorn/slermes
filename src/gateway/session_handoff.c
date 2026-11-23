/*
 * session_handoff.c — C home for upstream Hermes gateway/session_handoff.py.
 *
 * PoP: w9_sessionhand_e737d5_unsettled   @ gateway/session_handoff.py:_unsettled
 * PoP: w9_sessionhand_e737d5_destination @ gateway/session_handoff.py:_destination
 *
 * The two pure refusal-mappers of the shared-owner handoff protocol.  The
 * upstream _destination imports gateway.handoff_target lazily and calls
 * resolve_handoff_home(); the C port takes the HandoffTargetError.reason as an
 * injected input (the seam) and mirrors the _REFUSALS formatting exactly.
 */
#include "noc_w9_sessionhand_e737d5.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *refusal(const char *reason, const char *detail, int has_field, const char *field) {
    json_t *o = json_object();
    json_set(o, "reason", json_string(reason ? reason : ""));
    json_set(o, "detail", json_string(detail ? detail : ""));
    if (has_field) json_set(o, "field", json_string(field ? field : ""));
    else json_set(o, "field", json_null());
    return o;
}

static json_t *key_error(void) {
    json_t *o = json_object();
    json_set(o, "error", json_string("KeyError"));
    return o;
}

struct json_t *w9_sessionhand_e737d5_unsettled(const char *reason) {
    const char *r = reason ? reason : "";
    if (!strcmp(r, "session_busy"))
        return refusal("session_busy",
                       "wait for the current turn to finish, then retry the handoff", 0, NULL);
    if (!strcmp(r, "unknown_execution"))
        return refusal("unknown_execution",
                       "a turn of this session did not finish when Hermes restarted; "
                       "send /discard in it (or resume it) first, then retry the handoff", 0, NULL);
    return key_error();
}

struct json_t *w9_sessionhand_e737d5_destination(const char *reason, const char *platform) {
    const char *r = reason ? reason : "";
    const char *p = platform ? platform : "";
    char detail[1024];
    if (!strcmp(r, "unknown_platform")) {
        snprintf(detail, sizeof detail, "unknown platform '%s'", p);
    } else if (!strcmp(r, "platform_not_enabled")) {
        snprintf(detail, sizeof detail, "platform '%s' is not configured/enabled in the gateway", p);
    } else if (!strcmp(r, "no_home_channel")) {
        snprintf(detail, sizeof detail,
                 "no home channel configured for %s; set one with /sethome on the "
                 "destination chat first", p);
    } else {
        return key_error();
    }
    return refusal("invalid_params", detail, 1, "platform");
}
