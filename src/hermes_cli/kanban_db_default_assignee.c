/*
 * kanban_db_default_assignee.c -- real C11 port of the `kanban.default_assignee`
 * fill-in for unassigned ready cards, upstream
 * hermes_cli/kanban_db_default_assignee.py.
 *
 * PoP: w21_kanbandbde_905b79_apply @ hermes_cli/kanban_db_default_assignee.py:apply_default_assignee
 * PoP: w21_kanbandbde_905b79_fill @ hermes_cli/kanban_db_default_assignee.py:fill_default_assignee
 *
 * Seam: `_kb.write_txn(conn)` / `conn.execute(...)` / `_kb._append_event(...)`.
 * The caller reports what the write did (rowcount, or that it raised) and what
 * the post-failure SELECT returned; this port owns every decision the module
 * makes around those facts, including the exact event payload and both
 * DispatchResult buckets.
 *
 * ASCII source: the module carries no literal text.
 */
#include "hermes_json.h"
#include "noc_w21_kanbandbde_905b79.h"

#include <stdlib.h>
#include <string.h>

#define KBD_SOURCE "kanban.default_assignee"

/* Python truthiness of a decoded value; absent (NULL) reads as None. */
static int kbd_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static json_t *kbd_event(const char *task_id, const char *assignee) {
    json_t *ev = json_object();
    json_t *payload = json_object();
    json_set(payload, "assignee", json_string(assignee ? assignee : ""));
    json_set(payload, "source", json_string(KBD_SOURCE));
    json_set(ev, "task_id", json_string(task_id ? task_id : ""));
    json_set(ev, "kind", json_string("assigned"));
    json_set(ev, "payload", payload);
    return ev;
}

/* `updated.rowcount != 1` after the conditional UPDATE; dry_run never writes
 * and an exception is swallowed into False. */
static int kbd_apply(const char *task_id, const char *assignee, int dry_run,
                     int rowcount, int raises, json_t *events) {
    if (dry_run) return 1;
    if (raises) return 0;
    if (rowcount != 1) return 0;   /* a concurrent assign won the row */
    if (events) json_append(events, kbd_event(task_id, assignee));
    return 1;
}

json_t *w21_kanbandbde_905b79_apply(const char *task_id, const char *assignee,
                                    int dry_run, int rowcount, int raises) {
    json_t *out = json_object();
    json_t *events = json_array();
    int applied = kbd_apply(task_id, assignee, dry_run, rowcount, raises, events);
    json_set(out, "applied", json_bool(applied != 0));
    json_set(out, "events", events);
    return out;
}

static json_t *kbd_list_copy(const json_t *result, const char *key) {
    const json_t *v = (result && result->type == JSON_OBJECT) ? json_obj_get(result, key) : NULL;
    return (v && v->type == JSON_ARRAY) ? json_copy(v) : json_array();
}

static void kbd_list_push(json_t *list, const char *task_id) {
    json_append(list, json_string(task_id ? task_id : ""));
}

json_t *w21_kanbandbde_905b79_fill(const char *task_id, const json_t *default_assignee,
                                   int dry_run, int rowcount, int raises,
                                   const json_t *current_row, const json_t *result) {
    json_t *out = json_object();
    json_t *skipped = kbd_list_copy(result, "skipped_unassigned");
    json_t *auto_assigned = kbd_list_copy(result, "auto_assigned_default");
    json_t *events = json_array();
    json_t *assignee = json_null();

    if (!kbd_truthy(default_assignee)) {
        /* No default configured: an unassigned card is parked for this tick. */
        kbd_list_push(skipped, task_id);
    } else if (!kbd_apply(task_id, default_assignee->str_val, dry_run, rowcount, raises, events)) {
        const json_t *current = (current_row && current_row->type == JSON_OBJECT)
                                    ? json_obj_get(current_row, "assignee") : NULL;
        /* `current is None or not current["assignee"]`: still unassigned, so it
         * is bucketed; an assignee that appeared concurrently is deferred. */
        if (!current_row || current_row->type == JSON_NULL || !kbd_truthy(current))
            kbd_list_push(skipped, task_id);
    } else {
        kbd_list_push(auto_assigned, task_id);
        json_free(assignee);
        assignee = json_copy(default_assignee);
    }

    json_set(out, "assignee", assignee);
    json_set(out, "skipped_unassigned", skipped);
    json_set(out, "auto_assigned_default", auto_assigned);
    json_set(out, "events", events);
    return out;
}
