/*
 * port_session_server_requests.c — Port of Python:
 * gateway/session_server_requests.py (frame predicate).
 */

#include "port_session_server_requests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: ssr_is_response_frame @ gateway/session_server_requests.py:is_response_frame */
int ssr_is_response_frame(const char *frame_json)
{
    json_t *frame = frame_json ? json_parse(frame_json, NULL) : NULL;
    if (!frame || frame->type != JSON_OBJECT) {
        if (frame) json_free(frame);
        return 0;
    }

    int ok = 0;
    json_t *method = json_obj_get(frame, "method");
    json_t *id = json_obj_get(frame, "id");
    int has_result = json_obj_get(frame, "result") != NULL;
    int has_error = json_obj_get(frame, "error") != NULL;

    if (!method && id && id->type == JSON_STRING && id->str_val &&
        strncmp(id->str_val, "srq-", 4) == 0 && (has_result || has_error))
        ok = 1;

    json_free(frame);
    return ok;
}
