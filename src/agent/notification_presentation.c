/*
 * notification_presentation.c — C11 port of upstream Hermes agent/notification_presentation.py
 *                               (gate w25_notificati_ae9773).
 *
 * PoP: w25_notificati_ae9773_event_muted @ agent/notification_presentation.py:event_presentation_muted
 * PoP: w25_notificati_ae9773_diagnostic_process_event @ agent/notification_presentation.py:diagnostic_process_event
 * PoP: w25_notificati_ae9773_constants @ agent/notification_presentation.py:_FREEFORM_EVENTS / _PRESENTATION_CALLBACKS
 *
 * The ContextVar (``_muted_surface``), the agent's callback attributes and the
 * per-turn config loader are seams: the harness hands in the ContextVar's
 * current value and the values decide.  ``notification_policy_snapshot`` /
 * ``notification_turn`` are context managers over live objects and stay in the
 * caller; the predicates they gate are ported here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_notificati_ae9773.h"

/* _FREEFORM_EVENTS: text/media UI events, never approval/clarify/connection
 * requests or outcomes.  Kept in the sorted order json output compares against. */
static const char *const W25_NOTIF_FREEFORM[W25_NOTIFICATI_AE9773_FREEFORM_COUNT] = {
    "error",
    "message.complete",
    "message.delta",
    "message.interim",
    "message.start",
    "notification.show",
    "reaction",
    "reasoning.delta",
    "status.update",
    "thinking.delta",
    "tool.complete",
    "tool.generating",
    "tool.start",
};

/* _PRESENTATION_CALLBACKS, in declaration order. */
static const char *const W25_NOTIF_CALLBACKS[W25_NOTIFICATI_AE9773_CALLBACK_COUNT] = {
    "stream_delta_callback",
    "interim_assistant_callback",
    "clarify_context_callback",
    "reasoning_callback",
    "tool_progress_callback",
    "tool_start_callback",
    "tool_complete_callback",
    "tool_gen_callback",
    "reaction_callback",
};

/* Events diagnostic_process_event treats as diagnostics rather than results. */
static const char *const W25_NOTIF_DIAGNOSTIC_TYPES[3] = {
    "watch_disabled",
    "watch_overflow_tripped",
    "watch_overflow_released",
};

/* Python truthiness over a JSON value; NULL json_t* is None. */
static int np_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python's numeric view: an int, a float or a bool. */
static int np_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==': absent and null are both None; True == 1 == 1.0. */
static int np_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (np_number(a, &x) && np_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

static json_t *np_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "__error__", json_string(name));
    return o;
}

/* ``event in _FREEFORM_EVENTS``: a str membership test; an unhashable event
 * (list/dict) raises TypeError exactly as the frozenset lookup does. */
static int np_freeform_member(const json_t *event, int *type_error) {
    size_t i;
    *type_error = 0;
    if (event && (event->type == JSON_ARRAY || event->type == JSON_OBJECT)) {
        *type_error = 1;
        return 0;
    }
    if (!event || event->type != JSON_STRING) return 0;
    for (i = 0; i < W25_NOTIFICATI_AE9773_FREEFORM_COUNT; i++)
        if (strcmp(event->str_val, W25_NOTIF_FREEFORM[i]) == 0) return 1;
    return 0;
}

json_t *w25_notificati_ae9773_event_muted(const json_t *muted_surface,
                                          const json_t *event,
                                          const json_t *session_id) {
    int type_error;
    if (!np_equal(muted_surface, session_id)) return json_bool(0);
    if (np_freeform_member(event, &type_error)) return json_bool(1);
    if (type_error) return np_error("TypeError");
    return json_bool(0);
}

json_t *w25_notificati_ae9773_diagnostic_process_event(const json_t *event) {
    const json_t *type;
    size_t i;

    if (!event || event->type != JSON_OBJECT) return np_error("AttributeError");
    if (np_truthy(json_obj_get(event, "task_failure_notice"))) return json_bool(1);
    type = json_obj_get(event, "type");
    if (type && (type->type == JSON_ARRAY || type->type == JSON_OBJECT))
        return np_error("TypeError");
    if (type && type->type == JSON_STRING) {
        for (i = 0; i < sizeof(W25_NOTIF_DIAGNOSTIC_TYPES) / sizeof(*W25_NOTIF_DIAGNOSTIC_TYPES); i++)
            if (strcmp(type->str_val, W25_NOTIF_DIAGNOSTIC_TYPES[i]) == 0) return json_bool(1);
    }
    return json_bool(0);
}

json_t *w25_notificati_ae9773_constants(void) {
    json_t *o = json_object();
    json_t *freeform = json_array();
    json_t *callbacks = json_array();
    size_t i;

    for (i = 0; i < W25_NOTIFICATI_AE9773_FREEFORM_COUNT; i++)
        json_append(freeform, json_string(W25_NOTIF_FREEFORM[i]));
    for (i = 0; i < W25_NOTIFICATI_AE9773_CALLBACK_COUNT; i++)
        json_append(callbacks, json_string(W25_NOTIF_CALLBACKS[i]));
    json_set(o, "freeform_events", freeform);
    json_set(o, "presentation_callbacks", callbacks);
    json_set(o, "muted_variable", json_string("muted_notification_surface"));
    json_set(o, "muted_default", json_null());
    return o;
}
