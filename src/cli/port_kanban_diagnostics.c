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
#include <stdio.h>
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

/* DiagnosticAction list builder matching Python's Diagnostic.actions:
 * each action → {"kind","label","payload","suggested"}. */
static json_t *build_action(const char *kind, const char *label,
                            json_t *payload, int suggested)
{
    json_t *a = json_object();
    json_set(a, "kind", json_string(kind ? kind : ""));
    json_set(a, "label", json_string(label ? label : ""));
    json_set(a, "payload", payload ? payload : json_object());
    json_set(a, "suggested", json_bool(suggested != 0));
    return a;
}

/* Diagnostic.to_dict mirror. */
static json_t *build_diagnostic(const char *kind, const char *severity,
                                const char *title, const char *detail,
                                json_t *actions, long first_seen, long last_seen,
                                int count, json_t *data)
{
    json_t *d = json_object();
    json_set(d, "kind", json_string(kind ? kind : ""));
    json_set(d, "severity", json_string(severity ? severity : "warning"));
    json_set(d, "title", json_string(title ? title : ""));
    json_set(d, "detail", json_string(detail ? detail : ""));
    json_set(d, "actions", actions ? actions : json_array());
    json_set(d, "first_seen_at", json_number(first_seen));
    json_set(d, "last_seen_at", json_number(last_seen));
    json_set(d, "count", json_number(count));
    json_set(d, "data", data ? data : json_object());
    return d;
}

/* PoP: kanban_diagnostic_rule_review_dependency_deadlock @ hermes_cli/kanban_diagnostics.py:_rule_review_dependency_deadlock */
/* Detect a legacy review handoff that starves downstream children.
 * task/events/runs/cfg are JSON-objects/arrays; now is a unix epoch (int).
 * Returns a JSON array of Diagnostic dicts (empty when not applicable). */
json_t *kanban_diagnostic_rule_review_dependency_deadlock(
    const json_t *task, const json_t *events, const json_t *runs,
    long now, const json_t *cfg)
{
    (void)runs; (void)cfg;
    json_t *out = json_array();
    const char *status = kanban_task_field_str(task, "status", "");
    if (strcmp(status, "blocked") != 0) return out;
    /* find the latest "blocked" event */
    json_t *latest_block = NULL;
    if (events && events->type == JSON_ARRAY) {
        size_t n = json_len(events);
        for (size_t i = 0; i < n; i++) {
            json_t *ev = json_get(events, i);
            if (!ev) continue;
            if (strcmp(kanban_event_kind(ev), "blocked") == 0)
                latest_block = ev;
        }
    }
    if (!latest_block) return out;
    json_t *payload = kanban_parse_payload(latest_block);
    const char *reason = "";
    const json_t *reason_node = json_obj_get(payload, "reason");
    if (reason_node && reason_node->type == JSON_STRING)
        reason = reason_node->str_val;
    /* must start with "review-required:" (case-insensitive) */
    const char *r = reason;
    while (*r && (*r == ' ' || *r == '\t')) r++;
    if (strncasecmp(r, "review-required:", 16) != 0) {
        json_free(payload);
        return out;
    }
    json_free(payload);
    /* graph.children with status == "todo" */
    json_t *graph = NULL;
    if (cfg && cfg->type == JSON_OBJECT)
        graph = json_obj_get(cfg, "_graph");
    if (!graph || graph->type != JSON_OBJECT) return out;
    json_t *children = json_obj_get(graph, "children");
    if (!children || children->type != JSON_ARRAY) return out;
    json_t *waiting = json_array();
    size_t n = json_len(children);
    for (size_t i = 0; i < n; i++) {
        json_t *child = json_get(children, i);
        if (!child || child->type != JSON_OBJECT) continue;
        const char *cs = kanban_task_field_str(child, "status", "");
        if (strcmp(cs, "todo") == 0)
            json_append(waiting, json_copy(child));
    }
    if (json_len(waiting) == 0) {
        json_free(waiting);
        return out;
    }
    /* actions + child_ids */
    json_t *actions = json_array();
    const char *task_id = kanban_task_field_str(task, "id", "");
    json_t *child_ids = json_array();
    size_t wc = json_len(waiting);
    for (size_t i = 0; i < wc; i++) {
        json_t *child = json_get(waiting, i);
        const char *cid = kanban_task_field_str(child, "id", "");
        json_t *cj = json_string(cid ? cid : "");
        json_append(child_ids, cj);
        (void)cj;
    }
    if (task_id && *task_id) {
        json_t *payload1 = json_object();
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "hermes kanban complete %s", task_id);
        json_set(payload1, "command", json_string(cmd));
        json_append(actions, build_action("cli_hint",
            "Complete the finished implementation phase", payload1, 1));
    }
    if (task_id && wc > 0) {
        json_t *payload2 = json_object();
        char cmd[256];
        const char *first_child = NULL;
        if (wc > 0) {
            json_t *fc = json_get(waiting, 0);
            first_child = kanban_task_field_str(fc, "id", "");
        }
        snprintf(cmd, sizeof(cmd), "hermes kanban unlink %s %s",
                 task_id, first_child ? first_child : "");
        json_set(payload2, "command", json_string(cmd));
        json_append(actions, build_action("cli_hint",
            "Or unlink the incorrectly gated reviewer", payload2, 0));
    }
    long blocked_at = kanban_event_ts(latest_block);
    if (!blocked_at) blocked_at = now;
    char title[256];
    snprintf(title, sizeof(title), "Review handoff blocks %zu dependent task(s)", wc);
    const char *detail = "This implementation is sticky-blocked for review while its "
        "downstream task(s) require the implementation to be done or archived before "
        "they can run. Complete the finished phase, unlink the incorrect dependency, "
        "or migrate this workflow to the first-class review lifecycle.";
    json_t *data = json_object();
    json_set(data, "blocked_parent_id", json_string(task_id ? task_id : ""));
    json_set(data, "waiting_child_ids", child_ids);
    json_set(data, "block_reason", json_string(reason ? reason : ""));
    json_append(out, build_diagnostic("review_dependency_deadlock", "error",
        title, detail, actions, blocked_at, blocked_at, (int)wc, data));
    json_free(waiting);
    return out;
}
