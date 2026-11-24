/*
 * telegram_held_inbound.c - real C11 port of upstream Hermes
 *   plugins/platforms/telegram/telegram_held_inbound.py.
 *
 * The hold queue is the module's state machine: arrival ordering, the cap that drops
 * the oldest, identity dedupe, forwarding to the successor adapter and the redispatch
 * schedule decision. The adapter itself (its fatal/drop flags, its scheduler) is the
 * seam, so a "held event" here is a JSON object with an id, its arrival stamp and its
 * text length.
 *
 * PoP: w19_telegramhe_c40f34_insert_by_arrival @ plugins/platforms/telegram/telegram_held_inbound.py:_insert_by_arrival
 * PoP: w19_telegramhe_c40f34_note_arrival @ plugins/platforms/telegram/telegram_held_inbound.py:_note_arrival
 * PoP: w19_telegramhe_c40f34_hold_event @ plugins/platforms/telegram/telegram_held_inbound.py:_hold_inbound_event
 */
#include "hermes_json.h"
#include "noc_w19_telegramhe_c40f34.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_HELD_MAX 64

static int jisnum(json_t *v) { return v && v->type == JSON_NUMBER; }

/* getattr(event, "_telegram_arrival_seq", None) */
static json_t *event_seq(json_t *ev) {
    return (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "_seq") : NULL;
}

/* The queue entry as the module keeps it: the event object itself (id + stamp). */
static json_t *event_copy(json_t *ev) {
    json_t *out = json_object();
    json_set(out, "id", ev ? json_copy(json_obj_get(ev, "id")) : json_null());
    json_set(out, "_seq", ev ? json_copy(event_seq(ev)) : json_null());
    return out;
}

/* Stamps as the ordering loop reads them: getattr(other, attr, -1). */
static double other_stamp(json_t *ev) {
    json_t *s = event_seq(ev);
    return jisnum(s) ? s->num_val : -1.0;
}

static double seq_of(json_t *s) { return jisnum(s) ? s->num_val : 0.0; }

/* copy `arr` and place `ev` behind every entry that arrived before it */
static json_t *push_arrival(json_t *arr, json_t *ev) {
    json_t *out = json_array();
    json_t *seq = event_seq(ev);
    int placed = 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        if (!placed && jisnum(seq) && other_stamp(json_get(arr, i)) > seq_of(seq)) {
            json_append(out, event_copy(ev));
            placed = 1;
        }
        json_append(out, json_copy(json_get(arr, i)));
    }
    if (!placed) json_append(out, event_copy(ev));
    return out;
}

/* _insert_by_arrival over bare stamps: same rule, stamps in / stamps out. */
json_t *w19_telegramhe_c40f34_insert_by_arrival(json_t *held_stamps, json_t *seq) {
    json_t *out = json_array();
    size_t n = (held_stamps && held_stamps->type == JSON_ARRAY) ? json_len(held_stamps) : 0;
    int placed = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_get(held_stamps, i);
        double stamp = jisnum(s) ? s->num_val : -1.0;   /* getattr(other, attr, -1) */
        if (!placed && jisnum(seq) && stamp > seq_of(seq)) {
            json_append(out, json_copy(seq));
            placed = 1;
        }
        json_append(out, json_copy(s));
    }
    if (!placed) json_append(out, json_copy(seq));
    return out;
}

/* _note_arrival: stamp only an unstamped event; a batch keeps its first stamp. */
json_t *w19_telegramhe_c40f34_note_arrival(json_t *existing_stamp, int counter) {
    json_t *out = json_object();
    if (!existing_stamp || existing_stamp->type == JSON_NULL) {
        json_set(out, "stamp", json_number((double)counter));
        json_set(out, "next", json_number((double)(counter + 1)));
    } else {
        json_set(out, "stamp", json_copy(existing_stamp));
        json_set(out, "next", json_number((double)counter));
    }
    return out;
}

/* _hold_inbound_event */
json_t *w19_telegramhe_c40f34_hold_event(json_t *queue, json_t *event, int fatal, int drop_delayed,
                                         int max_n, int schedule, json_t *successor_queue) {
    json_t *out = json_object();
    json_t *held = json_array();
    json_t *target = NULL;
    int accepted = 0, scheduled = 0, forwarded = 0;
    size_t n = (queue && queue->type == JSON_ARRAY) ? json_len(queue) : 0;

    if (!fatal) {
        if (successor_queue && successor_queue->type == JSON_ARRAY) {
            /* retired by a runner rebuild: the successor adopts it (it owns the queue again) */
            json_t *grown;
            size_t tn = (successor_queue->type == JSON_ARRAY) ? json_len(successor_queue) : 0;
            target = json_array();
            for (size_t i = 0; i < tn; i++) json_append(target, event_copy(json_get(successor_queue, i)));
            grown = push_arrival(target, event);
            json_free(target);
            target = grown;
            /* this adapter's queue is untouched; it only claims the update and forwards */
            for (size_t i = 0; i < n; i++) json_append(held, event_copy(json_get(queue, i)));
            accepted = 1;
            forwarded = 1;
            scheduled = 0;
        } else {
            /* identity dedupe: "same_as" names the queue slot holding THIS object */
            int same_as = (event && event->type == JSON_OBJECT)
                        ? (int)json_get_num(event, "same_as", -1) : -1;
            int dup = (same_as >= 0 && (size_t)same_as < n) ? 1 : 0;
            for (size_t i = 0; i < n; i++) json_append(held, event_copy(json_get(queue, i)));
            if (!dup) {
                int cap = max_n > 0 ? max_n : DEFAULT_HELD_MAX;
                while ((int)json_len(held) >= cap) {
                    json_t *rest = json_array();          /* the cap drops the oldest */
                    for (size_t i = 1; i < json_len(held); i++)
                        json_append(rest, json_copy(json_get(held, i)));
                    json_free(held);
                    held = rest;
                }
                {
                    json_t *grown = push_arrival(held, event);
                    json_free(held);
                    held = grown;
                }
                accepted = 1;
                scheduled = (schedule && !drop_delayed) ? 1 : 0;
            }
        }
    } else {
        for (size_t i = 0; i < n; i++) json_append(held, event_copy(json_get(queue, i)));
    }

    json_set(out, "queue", held);
    json_set(out, "accepted", json_number((double)accepted));
    json_set(out, "scheduled", json_number((double)scheduled));
    json_set(out, "forwarded", json_bool(forwarded ? true : false));
    json_set(out, "target_queue", target ? target : json_null());
    return out;
}
