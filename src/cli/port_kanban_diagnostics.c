/*
 * port_kanban_diagnostics.c — Faithful C11 port of pure helpers from
 * hermes_cli/kanban_diagnostics.py
 *
 * Pure, IO-free rule helpers. Events/tasks are modeled as libjson objects
 * (the Python helpers accept sqlite3.Row / dataclass / dict; in C we use
 * JSON objects which cover the dict case).
 *
 * Ported: severity_at_or_above, _task_field, _parse_payload, _event_kind,
 * _event_ts, _active_hallucination_events, _generic_recovery_actions.
 *
 * Angel-coder note: the full module has 20 functions; the remaining 13 are
 * rule implementations / dataclass to_dict that depend on the Diagnostic
 * dataclass graph and are a separate, larger port. Only the pure helpers
 * above are ported here — no façade.
 */

#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "kanban_diagnostics.h"

/* Severity rungs, ordered least -> most urgent. */
static const char *G_SEVERITY_ORDER[] = {"warning", "error", "critical"};
static const int G_SEVERITY_N = 3;

static int severity_index(const char *s) {
    if (!s) return -1;
    for (int i = 0; i < G_SEVERITY_N; i++)
        if (strcmp(s, G_SEVERITY_ORDER[i]) == 0) return i;
    return -1;
}

/* PoP: kanban_severity_at_or_above @ hermes_cli/kanban_diagnostics.py:severity_at_or_above */
int kanban_severity_at_or_above(const char *severity, const char *threshold)
{
    if (threshold == NULL) return 1;
    if (severity == NULL) return 0;  /* threshold set but severity missing */
    int si = severity_index(severity);
    int ti = severity_index(threshold);
    if (si < 0 || ti < 0) return 0;
    return si >= ti;
}

/* PoP: kanban_task_field @ hermes_cli/kanban_diagnostics.py:_task_field */
/* Reads a field from a task modeled as a JSON object. Returns the json_t*
 * (owned by the task) or NULL if absent. For scalar extraction use
 * kanban_task_field_str. */
const json_t *kanban_task_field(const json_t *task, const char *name)
{
    if (!task) return NULL;
    if (task->type == JSON_OBJECT) return json_obj_get(task, name);
    return NULL;
}

const char *kanban_task_field_str(const json_t *task, const char *name, const char *default_val)
{
    const json_t *v = kanban_task_field(task, name);
    if (v && v->type == JSON_STRING) return v->str_val;
    return default_val;
}

/* PoP: kanban_parse_payload @ hermes_cli/kanban_diagnostics.py:_parse_payload */
json_t *kanban_parse_payload(const json_t *ev)
{
    json_t *out = json_object();
    const json_t *p = kanban_task_field(ev, "payload");
    if (!p) return out;
    if (p->type == JSON_OBJECT) {
        /* copy */
        json_t *c = json_copy(p);
        json_free(out);
        return c ? c : json_object();
    }
    if (p->type == JSON_STRING) {
        json_t *parsed = json_parse(p->str_val, NULL);
        if (parsed) {
            json_t *c = (parsed->type == JSON_OBJECT) ? parsed : json_object();
            if (parsed != c) json_free(parsed);
            json_free(out);
            return c;
        }
        return out;
    }
    return out;
}

/* PoP: kanban_event_kind @ hermes_cli/kanban_diagnostics.py:_event_kind */
const char *kanban_event_kind(const json_t *ev)
{
    const char *k = kanban_task_field_str(ev, "kind", "");
    return k ? k : "";
}

/* PoP: kanban_event_ts @ hermes_cli/kanban_diagnostics.py:_event_ts */
long kanban_event_ts(const json_t *ev)
{
    const json_t *t = kanban_task_field(ev, "created_at");
    if (!t) return 0;
    if (t->type == JSON_NUMBER) return (long)t->num_val;
    if (t->type == JSON_STRING) {
        /* int(t or 0) — parse leading integer */
        char *end = NULL;
        long v = strtol(t->str_val ? t->str_val : "0", &end, 10);
        (void)end;
        return v;
    }
    return 0;
}

/* PoP: kanban_active_hallucination_events @ hermes_cli/kanban_diagnostics.py:_active_hallucination_events */
/* Returns a JSON array of events of `kind` that have no completed/edited
 * event strictly after them. events is a JSON array. Caller frees. */
json_t *kanban_active_hallucination_events(const json_t *events, const char *kind)
{
    json_t *active = json_array();
    if (!events || events->type != JSON_ARRAY || !kind) return active;
    size_t n = json_len(events);
    for (size_t i = 0; i < n; i++) {
        json_t *ev = json_get(events, i);
        if (!ev) continue;
        const char *k = kanban_event_kind(ev);
        if (strcmp(k, "completed") == 0 || strcmp(k, "edited") == 0) {
            /* active.clear() — drop all accumulated events */
            json_t *tmp = json_array();
            json_free(active);
            active = tmp;
        } else if (strcmp(k, kind) == 0) {
            json_append(active, json_copy(ev));
        }
    }
    return active;
}

/* DiagnosticAction to_dict (PoP: DiagnosticAction.to_dict) */
json_t *kanban_diagnostic_action_to_dict(const char *kind, const char *label,
                                          const json_t *payload, int suggested)
{
    json_t *d = json_object();
    json_set(d, "kind", json_string(kind ? kind : ""));
    json_set(d, "label", json_string(label ? label : ""));
    json_set(d, "payload", payload ? json_copy(payload) : json_object());
    json_set(d, "suggested", json_bool(suggested != 0));
    return d;
}

/* PoP: kanban_generic_recovery_actions @ hermes_cli/kanban_diagnostics.py:_generic_recovery_actions */
json_t *kanban_generic_recovery_actions(const json_t *task, int running)
{
    json_t *out = json_array();
    (void)task;
    if (running) {
        json_t *a = json_object();
        json_set(a, "kind", json_string("reclaim"));
        json_set(a, "label", json_string("Reclaim task"));
        json_set(a, "payload", json_object());
        json_set(a, "suggested", json_bool(0));
        json_append(out, a);
    }
    json_t *b = json_object();
    json_set(b, "kind", json_string("reassign"));
    json_set(b, "label", json_string("Reassign to different profile"));
    json_t *p = json_object();
    json_set(p, "reclaim_first", json_bool(running != 0));
    json_set(b, "payload", p);
    json_set(b, "suggested", json_bool(0));
    json_append(out, b);
    return out;
}
