/*
 * connector_refusals.c — C home for tui_gateway/connector_refusals.py.
 * PoP: connrefuse_constant @ tui_gateway/connector_refusals.py:INVALID_PARAMS
 * PoP: connrefuse_reissue_refusal @ tui_gateway/connector_refusals.py:REISSUE_REFUSALS
 * PoP: connrefuse_auth_refusal @ tui_gateway/connector_refusals.py:auth_refusal
 *
 * Each refusal is (code, ConnectorErrorReason, message); WireEnum is a StrEnum,
 * so the reason is its upper-case string value on the wire.
 */
#include "hermes_json.h"
#include <string.h>

static json_t *refusal(int code, const char *reason, const char *msg) {
    json_t *a = json_array();
    json_append(a, json_number(code));
    json_append(a, json_string(reason));
    json_append(a, json_string(msg));
    return a;
}
/* Returns NULL for an unknown name. */
static json_t *named(const char *n) {
    if (!strcmp(n,"INVALID_PARAMS")) return refusal(4000,"INVALID_PARAMS","Connector parameters are invalid.");
    if (!strcmp(n,"INVALID_ANSWER")) return refusal(4002,"INVALID_ANSWER","Connection answer is invalid.");
    if (!strcmp(n,"UNKNOWN_OPERATION")) return refusal(4004,"UNKNOWN_OPERATION","No open operation with that op_id.");
    if (!strcmp(n,"UNKNOWN_TARGET_REFUSAL")) return refusal(4004,"UNKNOWN_TARGET","No such target on the open operation.");
    if (!strcmp(n,"CONNECTORS_UNAVAILABLE")) return refusal(4031,"CONNECTORS_UNAVAILABLE","Connectors are not available in this session.");
    if (!strcmp(n,"REQUEST_FAILED")) return refusal(5034,"CONNECTOR_REQUEST_FAILED","Connector request failed. Try again explicitly.");
    if (!strcmp(n,"REISSUE_REFUSED")) return refusal(4002,"REISSUE_REFUSED","The target cannot be run again.");
    return NULL;
}

json_t *connrefuse_constant(const char *name) {
    json_t *r = named(name ? name : "");
    return r ? r : json_null();
}

json_t *connrefuse_reissue_refusal(const char *key) {
    if (!key) return json_null();
    if (!strcmp(key,"unknown_target")) return named("UNKNOWN_TARGET_REFUSAL");
    if (!strcmp(key,"mixed_kinds")) return refusal(4000,"INVALID_PARAMS","One target kind per request.");
    if (!strcmp(key,"link_still_valid")) return refusal(4002,"LINK_STILL_VALID","Reopen the stored link.");
    if (!strcmp(key,"settled")) return refusal(4002,"REISSUE_REFUSED","The operation has settled.");
    if (!strcmp(key,"not_allowed")) return refusal(4002,"REISSUE_REFUSED","This target cannot be run again.");
    if (!strcmp(key,"refused")) return named("REISSUE_REFUSED");
    return json_null();
}

json_t *connrefuse_auth_refusal(const char *code, int status) {
    const char *c = code ? code : "";
    if (!strcmp(c,"no_access") || !strcmp(c,"ORG_ACCESS_DENIED"))
        return refusal(4030,"ORG_ACCESS_DENIED","This account cannot manage connectors for this organization.");
    if (status == 401 || !strcmp(c,"invalid_token") || !strcmp(c,"INVALID_TOKEN") || !strcmp(c,"NO_TOKEN"))
        return refusal(4032,"NEEDS_NOUS_AUTH","Sign in to use connectors.");
    return refusal(4030,"FORBIDDEN_SCOPE","Connector access is not permitted for this account.");
}
