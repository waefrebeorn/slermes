/*
 * hosted_room_work_lineage.c - C11 port of upstream
 * gateway/hosted_room_work_lineage.py (gate w24_hostedroom_d92036).
 *
 * PoP: w24_hostedroom_d92036_at_sequence @ gateway/hosted_room_work_lineage.py:at_sequence
 * PoP: w24_hostedroom_d92036_prefix @ gateway/hosted_room_work_lineage.py:source_prefix_locked
 * PoP: w24_hostedroom_d92036_provenance @ gateway/hosted_room_work_lineage.py:validate_provenance
 * PoP: w24_hostedroom_d92036_task_origins @ gateway/hosted_room_work_lineage.py:task_origins
 * PoP: w24_hostedroom_d92036_constants @ gateway/hosted_room_work_lineage.py:TRANSFER_KINDS
 *
 * Passive evidence is never election proof, so this port is entirely a set of
 * decisions over values: the reversed span lookup, the prefix check (sequence
 * continuity, epoch agreement, the gateway actor's identity and the
 * transfer-kind boundary rule), the receipt-to-original-span rule, and the
 * task-origin map.  Opening the store, reading the authority history and
 * decoding the event rows are seams; the caller supplies what it read.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w24_hostedroom_d92036.h"

/* TRANSFER_KINDS: committed authority transfers, each the first event of its span. */
static const char *const HL_TRANSFER_KINDS[] = {"authority.claimed", "authority.lost", NULL};
#define HL_MAX_AUTHORITY_SPANS 1024

/* Numeric view of a value (bool folds to 0/1). */
static int hl_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==': numbers (bool folded) compare numerically, strings by text,
 * a null/missing key equals only null. */
static int hl_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (hl_number(a, &x) && hl_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* value != an int constant, without materialising a node. */
static int hl_ne_num(const json_t *v, double n) {
    double x = 0.0;
    if (!hl_number(v, &x)) return 1;
    return x != n;
}

static double hl_field_num(const json_t *obj, const char *key) {
    double v = 0.0;
    if (!obj || obj->type != JSON_OBJECT) return 0.0;
    hl_number(json_obj_get(obj, key), &v);
    return v;
}

/* at_sequence: the last span whose from_seq <= seq, else NULL (the Python raise). */
static const json_t *hl_span_at(const json_t *spans, double seq) {
    size_t n = json_len(spans), i;
    for (i = n; i > 0; i--) {
        const json_t *span = json_get(spans, i - 1);
        if (!span || span->type != JSON_OBJECT) continue;
        if (seq >= hl_field_num(span, "from_seq")) return span;
    }
    return NULL;
}

static json_t *hl_span_view(const json_t *span) {
    json_t *o = json_object();
    json_set(o, "gateway_id", json_copy(json_obj_get(span, "gateway_id")));
    json_set(o, "epoch", json_copy(json_obj_get(span, "epoch")));
    json_set(o, "from_seq", json_copy(json_obj_get(span, "from_seq")));
    return o;
}

json_t *w24_hostedroom_d92036_at_sequence(const json_t *req) {
    const json_t *span = hl_span_at(json_obj_get(req, "spans"), hl_field_num(req, "seq"));
    if (!span) {
        json_t *o = json_object();
        json_set(o, "ok", json_bool(0));
        json_set(o, "error", json_string("precedes"));
        return o;
    }
    return hl_span_view(span);
}

json_t *w24_hostedroom_d92036_prefix(const json_t *req) {
    const json_t *spans = json_obj_get(req, "spans");
    const json_t *events = json_obj_get(req, "events");
    double seq = hl_field_num(req, "seq");
    size_t n, i, count = 0;
    json_t *out;

    /* read_history_locked -> None: successor work evidence has no lineage. */
    if (!spans || spans->type != JSON_ARRAY) {
        out = json_object();
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("no_lineage"));
        return out;
    }
    n = events && events->type == JSON_ARRAY ? json_len(events) : 0;
    for (i = 0; i < n; i++) {
        const json_t *event = json_get(events, i);
        const json_t *actor_json, *actor_kind, *actor_id, *span, *kind;
        json_t *actor;
        int boundary, actor_is_gateway, actor_mismatch, transfer, bad = 0;

        actor_json = event && event->type == JSON_OBJECT ? json_obj_get(event, "actor_json") : NULL;
        actor = actor_json && actor_json->type == JSON_STRING
                    ? json_parse(actor_json->str_val ? actor_json->str_val : "", NULL) : NULL;
        if (!actor || actor->type != JSON_OBJECT) {
            json_free(actor);
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string("decode"));
            return out;
        }
        count++;
        span = hl_span_at(spans, (double)count);
        if (!span) {
            json_free(actor);
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string("precedes"));
            return out;
        }
        actor_kind = json_obj_get(actor, "kind");
        actor_id = json_obj_get(actor, "id");
        actor_is_gateway = actor_kind && actor_kind->type == JSON_STRING && actor_kind->str_val &&
                           !strcmp(actor_kind->str_val, "gateway");
        actor_mismatch = actor_is_gateway && !hl_equal(actor_id, json_obj_get(span, "gateway_id"));
        /* boundary = span.epoch > 1 and count == span.from_seq */
        boundary = hl_field_num(span, "epoch") > 1.0 &&
                   (double)count == hl_field_num(span, "from_seq");
        kind = json_obj_get(event, "kind");
        transfer = kind && kind->type == JSON_STRING && kind->str_val &&
                   (!strcmp(kind->str_val, "authority.claimed") ||
                    !strcmp(kind->str_val, "authority.lost"));
        if (hl_ne_num(json_obj_get(event, "seq"), (double)count) ||
            !hl_equal(json_obj_get(event, "authority_epoch"), json_obj_get(span, "epoch")) ||
            actor_mismatch || (transfer != boundary)) {
            bad = 1;
        }
        json_free(actor);
        if (bad) {
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string("unverified"));
            return out;
        }
    }
    if ((double)count != seq) {
        out = json_object();
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("unavailable"));
        return out;
    }
    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "spans", json_copy(spans));
    return out;
}

json_t *w24_hostedroom_d92036_provenance(const json_t *req) {
    const json_t *record = json_obj_get(req, "record");
    const json_t *spans = json_obj_get(req, "spans");
    const json_t *tasks, *receipts;
    size_t n, i, j;
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));

    if (!record || record->type != JSON_OBJECT) goto key_error;
    tasks = json_obj_get(record, "tasks");
    receipts = json_obj_get(record, "receipts");
    if (!tasks || !receipts) goto key_error;
    /* Execution generation remains an independent per-task counter. */
    n = json_len(tasks);
    for (i = 0; i < n; i++) {
        const json_t *task = json_get(tasks, i);
        if (!task || task->type != JSON_OBJECT) goto key_error;
        if (!json_obj_get(task, "source_event_seq")) goto key_error;
        if (!hl_span_at(spans, hl_field_num(task, "source_event_seq"))) {
            json_free(out);
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string("precedes"));
            return out;
        }
    }
    n = json_len(receipts);
    for (i = 0; i < n; i++) {
        const json_t *receipt = json_get(receipts, i);
        const json_t *home, *authority_id, *authority_epoch;
        int found = 0;
        if (!receipt || receipt->type != JSON_OBJECT) goto key_error;
        home = json_obj_get(receipt, "home_install_id");
        authority_id = json_obj_get(receipt, "authority_gateway_id");
        authority_epoch = json_obj_get(receipt, "authority_epoch");
        if (!home || !authority_id || !authority_epoch) goto key_error;
        for (j = 0; j < json_len(spans); j++) {
            const json_t *span = json_get(spans, j);
            if (!span || span->type != JSON_OBJECT) continue;
            /* s.gateway_id == receipt["home_install_id"] == receipt["authority_gateway_id"] */
            if (hl_equal(json_obj_get(span, "gateway_id"), home) &&
                hl_equal(home, authority_id) &&
                hl_equal(json_obj_get(span, "epoch"), authority_epoch)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            json_free(out);
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string("no_authority_span"));
            return out;
        }
    }
    json_set(out, "ok", json_bool(1));
    return out;

key_error:
    json_set(out, "error", json_string("key"));
    return out;
}

json_t *w24_hostedroom_d92036_task_origins(const json_t *req) {
    const json_t *record = json_obj_get(req, "record");
    const json_t *spans = json_obj_get(req, "spans");
    const json_t *tasks;
    size_t n, i;
    json_t *out;

    if (!record || record->type != JSON_OBJECT) goto key_error;
    tasks = json_obj_get(record, "tasks");
    if (!tasks) goto key_error;
    out = json_object();
    n = json_len(tasks);
    for (i = 0; i < n; i++) {
        const json_t *task = json_get(tasks, i);
        const json_t *task_id, *span;
        json_t *origin;
        if (!task || task->type != JSON_OBJECT) { json_free(out); goto key_error; }
        task_id = json_obj_get(task, "task_id");
        if (!task_id || !json_obj_get(task, "source_event_seq") ||
            task_id->type != JSON_STRING || !task_id->str_val) {
            json_free(out);
            goto key_error;
        }
        span = hl_span_at(spans, hl_field_num(task, "source_event_seq"));
        if (!span) {
            json_free(out);
            out = json_object();
            json_set(out, "ok", json_bool(0));
            json_set(out, "error", json_string("precedes"));
            return out;
        }
        origin = json_object();
        json_set(origin, "gateway_id", json_copy(json_obj_get(span, "gateway_id")));
        json_set(origin, "epoch", json_copy(json_obj_get(span, "epoch")));
        json_set(out, task_id->str_val, origin);
    }
    return out;

key_error:
    out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string("key"));
    return out;
}

json_t *w24_hostedroom_d92036_constants(void) {
    json_t *o = json_object();
    json_t *kinds = json_array();
    int i;
    for (i = 0; HL_TRANSFER_KINDS[i]; i++) json_append(kinds, json_string(HL_TRANSFER_KINDS[i]));
    json_set(o, "transfer_kinds", kinds);
    json_set(o, "max_authority_spans", json_number(HL_MAX_AUTHORITY_SPANS));
    return o;
}

