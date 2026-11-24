/*
 * subagents.c - real C11 port of upstream Hermes persistence/authority/subagents.py
 * helper functions (the dataclass/Protocol surface stays declarative).
 * PoP: w19_subagents_8f6344_parent_superseded_note @ persistence/authority/subagents.py:parent_superseded_note
 * PoP: w19_subagents_8f6344_owed @ persistence/authority/subagents.py:SubagentRecord.owed
 * The automatic-end vocabulary is mirrored from persistence/conversation/sessions.py
 * (RECOVERABLE_END_REASONS | {tui_shutdown, ws_disconnect, idle_timeout, lru_evict}).
 */
#include "noc_w19_subagents_8f6344.h"

#include <stdio.h>
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
static const long long AUTO_RESUME_MAX_ATTEMPTS = 2;
static const char *const GATEWAY_SHUTDOWN = "gateway_shutdown";
static const char *const AUTO_RESUME_CANCELLED =
    "Not continued: its parent cancelled the automatic resume";

/* End stamps automatic cleanup writes (sessions.py AUTOMATIC_END_REASONS). */
static const char *const AUTOMATIC_END_REASONS[] = {
    "agent_close", "ws_orphan_reap", "superseded_by_resume", "startup_orphan_reap",
    "tui_shutdown", "ws_disconnect", "idle_timeout", "lru_evict",
    NULL
};

int w19_subagents_8f6344_is_automatic_end_reason(const char *reason) {
    if (!reason) return 0;
    for (int i = 0; AUTOMATIC_END_REASONS[i]; i++)
        if (!strcmp(reason, AUTOMATIC_END_REASONS[i])) return 1;
    return 0;
}

/* Python truthiness of a JSON scalar/container. */
static int jobj_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Compose one string with one %s argument into a json string node (no strdup). */
static json_t *json_printf_string(const char *fmt, const char *arg) {
    size_t need = strlen(fmt) + (arg ? strlen(arg) : 6) + 2;
    char *buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need, fmt, arg ? arg : "(null)");
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

json_t *w19_subagents_8f6344_parent_superseded_note(const char *tip_end_reason, int tip_exists) {
    if (!tip_exists)
        return json_string("its parent session no longer exists");
    /* `not tip_end_reason`: None and the empty string both answer None. */
    if (!tip_end_reason || !tip_end_reason[0])
        return json_null();
    if (w19_subagents_8f6344_is_automatic_end_reason(tip_end_reason))
        return json_null();
    (void)AUTO_RESUME_MAX_ATTEMPTS;
    (void)GATEWAY_SHUTDOWN;
    (void)AUTO_RESUME_CANCELLED;
    return json_printf_string("its parent session ended (%s)", tip_end_reason);
}

json_t *w19_subagents_8f6344_owed(json_t *document) {
    json_t *promised = (document && document->type == JSON_OBJECT)
                     ? json_obj_get(document, "auto_promised") : NULL;
    if (!jobj_truthy(promised)) return json_bool(0);
    json_t *gave_up = json_obj_get(document, "auto_gave_up");
    return json_bool(!gave_up || gave_up->type == JSON_NULL);
}
