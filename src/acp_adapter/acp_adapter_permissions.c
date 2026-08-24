/*
 * acp_adapter_permissions.c — canonical C11 port of acp_adapter/permissions.py
 *
 * ACP permission-option construction + outcome mapping into Hermes approval
 * strings. The Python side builds SDK objects; the C port builds the same
 * option rows as JSON (ACP transport is JSON-RPC, so the wire shape is what
 * matters).
 */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long g_permission_request_counter = 0;

/*
 * The C implementation owns its ACP encoding — every kind it emits is
 * PoP: acpp_permission_option_supports_kind @ acp_adapter/permissions.py:_permission_option_supports_kind
 */
int acpp_permission_option_supports_kind(const char *kind) {
    static const char *const supported[] = {
        "allow_once", "allow_always", "reject_once", "reject_always", NULL
    };
    if (!kind) return 0;
    for (int i = 0; supported[i]; i++)
        if (strcmp(kind, supported[i]) == 0) return 1;
    return 0;
}

/*
 * Returns a JSON array of {option_id, kind, name} rows matching Hermes
 * PoP: acpp_build_permission_options @ acp_adapter/permissions.py:_build_permission_options
 */
json_t *acpp_build_permission_options(int allow_permanent, int smart_denied) {
    json_t *arr = json_array();
    json_t *row;

    row = json_object();
    json_set(row, "option_id", json_string("allow_once"));
    json_set(row, "kind",      json_string("allow_once"));
    json_set(row, "name",      json_string("Allow once"));
    json_append(arr, row);

    if (!smart_denied) {
        /* ACP has no session-scoped kind: use the closest persistent hint
         * while keeping Hermes semantics in the option id. */
        row = json_object();
        json_set(row, "option_id", json_string("allow_session"));
        json_set(row, "kind",      json_string("allow_always"));
        json_set(row, "name",      json_string("Allow for session"));
        json_append(arr, row);
    }
    if (allow_permanent && !smart_denied) {
        row = json_object();
        json_set(row, "option_id", json_string("allow_always"));
        json_set(row, "kind",      json_string("allow_always"));
        json_set(row, "name",      json_string("Allow always"));
        json_append(arr, row);
    }
    row = json_object();
    json_set(row, "option_id", json_string("deny"));
    json_set(row, "kind",      json_string("reject_once"));
    json_set(row, "name",      json_string("Deny"));
    json_append(arr, row);

    if (!smart_denied && acpp_permission_option_supports_kind("reject_always")) {
        row = json_object();
        json_set(row, "option_id", json_string("deny_always"));
        json_set(row, "kind",      json_string("reject_always"));
        json_set(row, "name",      json_string("Deny always"));
        json_append(arr, row);
    }
    return arr;
}

/*
 * Tool-call update attached to a permission request. Unique perm-check-N id
 * PoP: acpp_build_permission_tool_call @ acp_adapter/permissions.py:_build_permission_tool_call
 */
json_t *acpp_build_permission_tool_call(const char *command, const char *description) {
    char tool_call_id[48];
    snprintf(tool_call_id, sizeof tool_call_id, "perm-check-%ld",
             ++g_permission_request_counter);

    char title[512], content_text[768];
    if (description && *description) {
        snprintf(title, sizeof title, "%s: %s", description, command);
        snprintf(content_text, sizeof content_text, "%s\n$ %s", description, command);
    } else {
        snprintf(title, sizeof title, "%s", command ? command : "");
        snprintf(content_text, sizeof content_text, "$ %s", command ? command : "");
    }

    json_t *update = json_object();
    json_set(update, "tool_call_id", json_string(tool_call_id));
    json_set(update, "title",        json_string(title));
    json_set(update, "kind",         json_string("execute"));
    json_set(update, "status",       json_string("pending"));
    json_t *content = json_array();
    json_t *block = json_object();
    json_set(block, "type",  json_string("text"));
    json_set(block, "text",  json_string(content_text));
    json_append(content, block);
    json_set(update, "content", content);
    json_t *raw = json_object();
    json_set(raw, "command",     json_string(command ? command : ""));
    json_set(raw, "description", json_string(description ? description : ""));
    json_set(update, "raw_input", raw);
    return update;
}

/*
 * Map an ACP permission outcome option-id into a Hermes approval string.
 * PoP: acpp_map_outcome_to_hermes @ acp_adapter/permissions.py:_map_outcome_to_hermes
 */
void acpp_map_outcome_to_hermes(const char *outcome_json,
                                char *const *allowed_ids,
                                char *out, size_t out_sz) {
    snprintf(out, out_sz, "deny"); /* default deny */
    json_t *o = json_parse(outcome_json ? outcome_json : "", NULL);
    if (!o || o->type != JSON_OBJECT) { if (o) json_free(o); return; }
    json_t *idv = json_obj_get(o, "option_id");
    const char *option_id = (idv && json_is_string(idv)) ? json_string_value(idv) : NULL;
    if (!option_id) { json_free(o); return; }
    for (int i = 0; allowed_ids && allowed_ids[i]; i++) {
        if (strcmp(option_id, allowed_ids[i]) == 0) {
            /* _OPTION_ID_TO_HERMES mapping */
            if (strcmp(option_id, "allow_once") == 0 ||
                strcmp(option_id, "allow_session") == 0)
                snprintf(out, out_sz, "allow");
            else if (strcmp(option_id, "allow_always") == 0)
                snprintf(out, out_sz, "always");
            else
                snprintf(out, out_sz, "deny");
            json_free(o);
            return;
        }
    }
    json_free(o);
}
