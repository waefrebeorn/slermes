/*
 * terminal_approval_batch.c — real C11 port of selected pure decisions of
 * upstream Hermes agent/terminal_approval_batch.py.
 *
 * PoP: w17_terminalap_428119_terminal_approval_runs @ agent/terminal_approval_batch.py:terminal_approval_runs
 * PoP: w17_terminalap_428119_consume_prepared_guard @ agent/terminal_approval_batch.py:consume_prepared_guard
 * PoP: w17_terminalap_428119_preparing_terminal_approval @ agent/terminal_approval_batch.py:preparing_terminal_approval
 */
#include "noc_w17_terminalap_428119.h"
#include <stddef.h>
#include <string.h>

static int json_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

json_t *w17_terminalap_428119_terminal_approval_runs(json_t *calls) {
    json_t *out = json_array();
    json_t *cur = NULL;
    int cur_key = -1, have_group = 0;
    size_t i, n = 0;

    if (calls && calls->type == JSON_ARRAY) n = calls->c.count;
    for (i = 0; i < n; i++) {
        json_t *call = calls->c.items[i];
        const char *name, *tag;
        json_t *err;
        int isterm;
        if (!call || call->type != JSON_OBJECT) continue;
        name = json_get_str(call, "name", "");
        err = json_obj_get(call, "parse_error");
        /* pc.name == "terminal" and pc.parse_error is None */
        isterm = (!strcmp(name, "terminal") && (!err || err->type == JSON_NULL)) ? 1 : 0;
        if (!have_group || isterm != cur_key) {
            if (cur) json_append(out, cur);
            cur = json_array();
            cur_key = isterm;
            have_group = 1;
        }
        tag = json_get_str(call, "tag", "");
        json_append(cur, json_string(tag));
    }
    if (cur) json_append(out, cur);
    return out;
}

json_t *w17_terminalap_428119_consume_prepared_guard(
    int slot_exists, int preparing, int failure_seen,
    const char *ctx_call_id, const char *expected_call_id,
    const char *command, const char *env_type, int has_host_access,
    const char *guard_command, const char *guard_env_type, int guard_host_access,
    json_t *decision, const char *approval_mode)
{
    if (!slot_exists || preparing) return json_null();
    if (!ctx_call_id) ctx_call_id = "";
    if (!expected_call_id) expected_call_id = "";
    if (!command) command = "";
    if (!env_type) env_type = "";
    if (!guard_command) guard_command = "";
    if (!guard_env_type) guard_env_type = "";

    /* Re-gate after an earlier slot in the batch failed. */
    if (failure_seen && decision && decision->type != JSON_NULL) return json_null();

    if (strcmp(ctx_call_id, expected_call_id) != 0) return json_null();
    if (strcmp(command, guard_command) != 0 ||
        strcmp(env_type, guard_env_type) != 0 ||
        has_host_access != guard_host_access) return json_null();

    if (!decision || decision->type == JSON_NULL) return json_null();
    if (decision->type == JSON_OBJECT) {
        json_t *appr = json_obj_get(decision, "approved");
        json_t *user = json_obj_get(decision, "user_approved");
        if (json_truthy(appr) && !json_truthy(user)) {
            json_t *smart = json_obj_get(decision, "smart_approved");
            int smart_ok = json_truthy(smart) &&
                           approval_mode && !strcmp(approval_mode, "smart");
            if (!smart_ok) return json_null();
        }
    }
    return json_copy(decision);
}

int w17_terminalap_428119_preparing_terminal_approval(int slot_exists, int preparing) {
    return (slot_exists && preparing) ? 1 : 0;
}
