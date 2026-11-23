/*
 * interrupt_scope.c — C port of upstream Hermes agent/interrupt_scope.py.
 *
 * PoP: interruptsc_default_tool_reason @ agent/interrupt_scope.py:_TOOL_REASON_HOST_CANCELLED
 * PoP: interruptsc_replay @ agent/interrupt_scope.py:InterruptScope.cancel
 * PoP: interruptsc_replay @ agent/interrupt_scope.py:InterruptScope.track
 * PoP: interruptsc_replay @ agent/interrupt_scope.py:track_in_interrupt_scope
 *
 * Host-owned cancellation latch: cancel(reason, tool_reason) latches both values
 * and hard-interrupts every registered agent; track(agent) registers an agent and
 * interrupts it immediately when a cancel already latched (no lost race).
 * request_hard_interrupt is driven as an explicit recorded seam via replay ops.
 */
#include "noc_w7_interruptsc_96fc03.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

const char *interruptsc_default_tool_reason(void) {
    return "host cancelled the command";
}

static void add_interrupt(json_t *interrupts, const char *agent,
                          const char *reason, int has_tool, const char *tool_reason) {
    json_t *rec = json_object();
    json_set(rec, "agent", json_string(agent ? agent : ""));
    json_set(rec, "reason", json_string(reason ? reason : ""));
    json_set(rec, "tool_reason", has_tool && tool_reason
             ? json_string(tool_reason) : json_null());
    json_append(interrupts, rec);
}

json_t *interruptsc_replay(const struct json_t *ops) {
    json_t *agents = json_array();          /* list[str] */
    json_t *interrupts = json_array();      /* list[{agent,reason,tool_reason}] */
    /* Python __init__: reason=None, _tool_reason=_TOOL_REASON_HOST_CANCELLED */
    char *reason = NULL;                    /* NULL == Python None */
    int has_reason = 0;
    char *tool_reason = strdup(interruptsc_default_tool_reason());
    int has_tool_reason = 1;

    size_t n = json_is_array(ops) ? json_len(ops) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *op = json_get(ops, i);
        if (!json_is_object(op)) continue;
        const char *name = json_get_str(op, "op", "");
        const char *agent = json_get_str(op, "agent", "");

        if (!strcmp(name, "track") || !strcmp(name, "track_in")) {
            if (!strcmp(name, "track_in")) {
                json_t *b = json_obj_get(op, "bound");
                if (!b || b->type != JSON_BOOL || !b->bool_val) continue; /* nullcontext */
            }
            json_append(agents, json_string(agent ? agent : ""));
            if (has_reason)
                add_interrupt(interrupts, agent, reason, has_tool_reason, tool_reason);
        } else if (!strcmp(name, "cancel")) {
            free(reason); free(tool_reason);
            reason = NULL; tool_reason = NULL;
            has_reason = 1;
            const char *r = json_get_str(op, "reason", "");
            reason = r ? strdup(r) : strdup("");
            json_t *tr = json_obj_get(op, "tool_reason");
            if (!tr) {
                has_tool_reason = 1;
                tool_reason = strdup(interruptsc_default_tool_reason());
            } else if (tr->type == JSON_NULL) {
                has_tool_reason = 0;         /* Python None */
            } else if (tr->type == JSON_STRING) {
                has_tool_reason = 1;
                tool_reason = tr->str_val ? strdup(tr->str_val) : strdup("");
            } else {
                has_tool_reason = 0;
            }
            for (size_t j = 0; j < json_len(agents); j++) {
                json_t *a = json_get(agents, j);
                add_interrupt(interrupts, a && a->str_val ? a->str_val : "",
                              reason, has_tool_reason, tool_reason);
            }
        }
    }

    json_t *out = json_object();
    json_set(out, "reason", has_reason && reason ? json_string(reason) : json_null());
    json_set(out, "tool_reason", has_tool_reason && tool_reason
             ? json_string(tool_reason) : json_null());
    json_set(out, "interrupts", interrupts);
    json_free(agents);
    free(reason); free(tool_reason);
    return out;
}
