/*
 * session_sudo_prompt.c — real C11 port of upstream Hermes gateway/session_sudo_prompt.py.
 * PoP: w14_sessionsud_e00b7b_ask_sudo_request @ gateway/session_sudo_prompt.py:ask_sudo
 * PoP: w14_sessionsud_e00b7b_sudo_timeout_s @ gateway/session_sudo_prompt.py:SUDO_TIMEOUT_S
 * Ported: the deterministic "sudo" request construction and the timeout
 * constant. ask_viewer()/the callback installer are side-effecting seams and
 * are excluded; the live function is driven with a capturing ask_viewer in the
 * oracle so the request it builds is compared exactly.
 */
#include "hermes_json.h"
#include "noc_w14_sessionsud_e00b7b.h"
#include <stdlib.h>

#define SUDO_TIMEOUT_S 120.0

struct json_t *w14_sessionsud_e00b7b_ask_sudo_request(const struct json_t *session_id,
                                                     const struct json_t *command,
                                                     const struct json_t *timeout) {
    struct json_t *out = json_object();
    struct json_t *params = json_object();
    double t = SUDO_TIMEOUT_S;
    if (timeout && timeout->type == JSON_NUMBER) t = timeout->num_val;
    json_set(out, "session_id", session_id ? json_copy(session_id) : json_null());
    json_set(out, "method", json_string("sudo"));
    json_set(params, "command", command ? json_copy(command) : json_null());
    json_set(out, "params", params);
    json_set(out, "timeout", json_number(t));
    return out;
}

double w14_sessionsud_e00b7b_sudo_timeout_s(void) {
    return SUDO_TIMEOUT_S;
}
