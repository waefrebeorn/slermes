/*
 * turn_request_assembly.c — C port of upstream Hermes agent/turn_request_assembly.py.
 *
 * PoP: w14_turnreques_8ccd5c_prepare_moa_request @ agent/turn_request_assembly.py:_prepare_moa_request
 *
 * The persistent-MoA prepared-request rebase/prepare logic is self-contained:
 * it reads agent.client.chat.completions capabilities, rebases a pending
 * prepared request when the client supports it, otherwise prepares a fresh
 * one, and finally uses prepared["messages"] as the wire messages.
 */
#include "noc_w14_turnreques_8ccd5c.h"
#include "hermes_json.h"

struct json_t *w14_turnreques_8ccd5c_prepare_moa_request(
    const struct json_t *api_messages, const struct json_t *pending,
    int has_rebase, int has_prepare,
    const struct json_t *rebase_out, const struct json_t *prepare_out) {
    struct json_t *result = json_array();
    struct json_t *prepared = NULL;
    struct json_t *messages = api_messages ? json_copy(api_messages) : json_null();
    int pending_is_set = pending && pending->type != JSON_NULL;

    if (!result) return NULL;

    if (pending_is_set) {
        if (has_rebase && rebase_out && rebase_out->type != JSON_NULL)
            prepared = json_copy(rebase_out);
        pending_is_set = 0;  /* Python clears pending unconditionally in this branch */
    }
    if (!prepared && has_prepare && prepare_out && prepare_out->type != JSON_NULL)
        prepared = json_copy(prepare_out);

    if (prepared && prepared->type == JSON_OBJECT) {
        const struct json_t *pm = json_obj_get(prepared, "messages");
        if (pm) {
            json_free(messages);
            messages = json_copy(pm);
        }
    }

    json_append(result, prepared ? prepared : json_null());
    json_append(result, messages ? messages : json_null());
    json_append(result, pending_is_set ? json_copy(pending) : json_null());
    return result;
}
