/*
 * session_ingress_live_turn.c — C11 port of upstream Hermes
 * gateway/session_ingress_live_turn.py (gate w25_sessioning_b21dd5).
 *
 * PoP: w25_sessioning_b21dd5_may_answer @ gateway/session_ingress_live_turn.py:may_answer_live_turn
 * PoP: w25_sessioning_b21dd5_replay_key @ gateway/session_ingress_live_turn.py:_replay_key
 * PoP: w25_sessioning_b21dd5_answers @ gateway/session_ingress_live_turn.py:answers_live_turn
 * PoP: w25_sessioning_b21dd5_consumed @ gateway/session_ingress_live_turn.py:was_consumed
 * PoP: w25_sessioning_b21dd5_constants @ gateway/session_ingress_live_turn.py:_CONSUMED_MAX
 *
 * A relay delivery is offered to the chat's running turn before the durable FIFO
 * sees it.  The runner is a seam -- the turn registry, the clarify and
 * slash-confirm stores and the agent's own turn are all read from it.  What is
 * ported is only what those answers decide: whether the live work could consume
 * the delivery, the replay key that keeps a re-send from running twice, the
 * bounded ledger of consumed keys, and the early-out for commands and bot peers.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w25_sessioning_b21dd5.h"

/* Python truthiness of a JSON value. */
static int si_truthy(const json_t *v) {
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

/* Python '==' over the shapes a replay key element can hold.  An absent key and
 * an explicit null are the same element (a missing source attribute is None). */
static int si_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && (b->type == JSON_BOOL || b->type == JSON_NUMBER))
        return (a->bool_val ? 1.0 : 0.0) == (b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val);
    if (b->type == JSON_BOOL && a->type == JSON_NUMBER)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!si_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !si_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* A value carried through untouched, or null when it is absent. */
static json_t *si_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

json_t *w25_sessioning_b21dd5_may_answer(const json_t *state) {
    const json_t *held;
    int answer = 0;
    if (si_truthy(json_obj_get(state, "running"))
        || si_truthy(json_obj_get(state, "clarify_pending"))
        || si_truthy(json_obj_get(state, "slash_pending"))) {
        return json_bool(1);
    }
    held = json_obj_get(state, "state");
    if (held && held->type == JSON_OBJECT)
        answer = si_truthy(json_obj_get(held, "update_prompt_pending"));
    return json_bool(answer);
}

json_t *w25_sessioning_b21dd5_replay_key(const json_t *event) {
    json_t *key;
    if (!si_truthy(json_obj_get(event, "message_id"))) return json_null();
    key = json_array();
    json_append(key, si_copy_or_null(json_obj_get(event, "profile")));
    json_append(key, si_copy_or_null(json_obj_get(event, "platform")));
    json_append(key, si_copy_or_null(json_obj_get(event, "chat_id")));
    json_append(key, si_copy_or_null(json_obj_get(event, "message_id")));
    return key;
}

/* The seeded ledger: every key the caller already consumed, in order. */
static json_t *si_seed_ledger(const json_t *req) {
    const json_t *before = json_obj_get(req, "consumed_before");
    json_t *keys = json_array();
    size_t i, n = json_len(before);
    for (i = 0; i < n; i++) json_append(keys, si_copy_or_null(json_get(before, i)));
    return keys;
}

/* A keyed assignment on an OrderedDict: an existing key keeps its position. */
static void si_remember(json_t *keys, const json_t *key) {
    size_t i, n = json_len(keys);
    for (i = 0; i < n; i++)
        if (si_equal(json_get(keys, i), key)) return;
    json_append(keys, json_copy(key));
    while (json_len(keys) > W25_SESSIONING_B21DD5_CONSUMED_MAX) {
        json_t *oldest = json_get(keys, 0);
        json_array_remove(keys, 0);
        json_free(oldest);
    }
}

json_t *w25_sessioning_b21dd5_answers(const json_t *req) {
    const json_t *reply = json_obj_get(req, "reply");
    int has_reply = reply && reply->type != JSON_NULL;
    int consumed = 0;
    json_t *key = NULL, *keys, *out = json_object(), *consumer_reply = json_null();

    keys = si_seed_ledger(req);
    /* event.get_command() or getattr(event, 'internal', False) or source.is_bot */
    if (!(si_truthy(json_obj_get(req, "command"))
          || si_truthy(json_obj_get(req, "internal"))
          || si_truthy(json_obj_get(req, "is_bot")))) {
        key = w25_sessioning_b21dd5_replay_key(req);
        if (has_reply) {
            consumed = 1;
            consumer_reply = si_truthy(reply) ? json_copy(reply) : json_null();
        } else if (si_truthy(json_obj_get(req, "running"))) {
            consumed = si_truthy(json_obj_get(req, "approval"))
                       || si_truthy(json_obj_get(req, "preempt"));
        }
        if (consumed && key->type == JSON_ARRAY) si_remember(keys, key);
    }
    json_set(out, "consumed", json_bool(consumed));
    json_set(out, "consumer_reply", consumer_reply);
    json_set(out, "keys", keys);
    if (key) json_free(key);
    return out;
}

json_t *w25_sessioning_b21dd5_consumed(const json_t *req) {
    const json_t *ledger = json_obj_get(req, "consumed");
    json_t *key = w25_sessioning_b21dd5_replay_key(req);
    int hit = 0;
    size_t i, n;
    if (key->type == JSON_ARRAY) {
        n = json_len(ledger);
        for (i = 0; i < n; i++)
            if (si_equal(json_get(ledger, i), key)) { hit = 1; break; }
    }
    json_free(key);
    return json_bool(hit);
}

json_t *w25_sessioning_b21dd5_constants(void) {
    json_t *out = json_object();
    json_set(out, "consumed_max", json_number(W25_SESSIONING_B21DD5_CONSUMED_MAX));
    return out;
}
