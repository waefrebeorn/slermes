/*
 * session_control_actions.c - C11 port of upstream
 * gateway/session_control_actions.py (gate w26_sessioncon_578be4).
 *
 * PoP: w26_sessioncon_578be4_validate @ gateway/session_control_actions.py:_validate
 * PoP: w26_sessioncon_578be4_dispatch @ gateway/session_control_actions.py:<action dispatch>
 * PoP: w26_sessioncon_578be4_envelope @ gateway/session_control_actions.py:control
 * PoP: w26_sessioncon_578be4_publish_decision @ gateway/session_control_actions.py:_publish_snapshot
 * PoP: w26_sessioncon_578be4_constants @ gateway/session_control_actions.py:<button tables>
 *
 * The authority rows, the goal manager and the event stream are seams: an
 * action's admission rules, the command it stands for, the envelope its result
 * is trimmed to and the publish ordering guard are pure functions of values.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "noc_w26_sessioncon_578be4.h"

typedef struct { const char *action; const char *name; const char *arg; } sc_cmd;

/* _ACTION_COMMANDS, in source order. */
static const sc_cmd W26_SC_ACTIONS[] = {
    {"goal.pause", "goal", "pause"},
    {"goal.resume", "goal", "resume"},
    {"goal.clear", "goal", "clear"},
    {"goal.unwait", "goal", "unwait"},
    {"loop.pause", "loop", "pause"},
    {"loop.resume", "loop", "resume"},
    {"loop.stop", "loop", "stop"},
    {"heartbeat.pause", "heartbeat", "pause"},
    {"heartbeat.resume", "heartbeat", "resume"},
    {"heartbeat.clear", "heartbeat", "clear"},
};

/* _SUBGOAL_ACTIONS */
static const char *const W26_SC_SUBGOALS[] = {"subgoal.add", "subgoal.clear", "subgoal.remove"};

/* _ENVELOPE */
static const char *const W26_SC_ENVELOPE[] = {"type", "output", "notice", "message", "display"};

#define W26_SC_ACTION_N (sizeof(W26_SC_ACTIONS) / sizeof(*W26_SC_ACTIONS))
#define W26_SC_SUBGOAL_N (sizeof(W26_SC_SUBGOALS) / sizeof(*W26_SC_SUBGOALS))
#define W26_SC_ENVELOPE_N (sizeof(W26_SC_ENVELOPE) / sizeof(*W26_SC_ENVELOPE))

static int sc_truthy(const json_t *v) {
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

static int sc_is_subgoal(const char *action) {
    size_t i;
    if (!action) return 0;
    for (i = 0; i < W26_SC_SUBGOAL_N; i++)
        if (!strcmp(action, W26_SC_SUBGOALS[i])) return 1;
    return 0;
}

static const sc_cmd *sc_find(const char *action) {
    size_t i;
    if (!action) return NULL;
    for (i = 0; i < W26_SC_ACTION_N; i++)
        if (!strcmp(action, W26_SC_ACTIONS[i].action)) return &W26_SC_ACTIONS[i];
    return NULL;
}

/* Python str.strip() over the whitespace a button's text field can carry. */
static char *sc_strip(const char *s) {
    const char *a = s ? s : "";
    const char *b;
    size_t n;
    char *out;
    while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\f' || *a == '\v') a++;
    b = a + strlen(a);
    while (b > a) {
        char c = b[-1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') b--;
        else break;
    }
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* type(x) is int: an integral number that is not a bool. */
static int sc_is_int(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return 0;
    return v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e18;
}

json_t *w26_sessioncon_578be4_validate(const json_t *req) {
    const json_t *args = req && req->type == JSON_OBJECT ? json_obj_get(req, "args") : NULL;
    const char *action = json_get_str(req, "action", NULL);
    size_t i;

    /* set(args) - {'text', 'index'} */
    if (!args || args->type != JSON_OBJECT) return json_string("invalid_params");
    for (i = 0; i < json_object_size(args); i++) {
        const char *k = json_object_get_key_at(args, i);
        if (!k) continue;
        if (strcmp(k, "text") && strcmp(k, "index")) return json_string("invalid_params");
    }
    if (action && !strcmp(action, "subgoal.remove")) {
        const json_t *index = json_obj_get(args, "index");
        if (!sc_is_int(index) || index->num_val < 1) return json_string("invalid_params");
    }
    if (action && !strcmp(action, "subgoal.add")) {
        const json_t *text = json_obj_get(args, "text");
        char *stripped;
        int empty;
        if (!text || text->type != JSON_STRING) return json_string("invalid_params");
        stripped = sc_strip(text->str_val);
        empty = stripped[0] == '\0';
        free(stripped);
        if (empty) return json_string("invalid_params");
    }
    if (!sc_find(action) && !sc_is_subgoal(action)) return json_string("invalid_params");
    return json_string("ok");
}

json_t *w26_sessioncon_578be4_dispatch(const json_t *action) {
    const sc_cmd *cmd;
    json_t *out;
    if (!action || action->type != JSON_STRING) return json_null();
    if (sc_is_subgoal(action->str_val)) {
        out = json_array();
        json_append(out, json_string("subgoal"));
        return out;
    }
    cmd = sc_find(action->str_val);
    if (!cmd) return json_null();
    out = json_array();
    json_append(out, json_string(cmd->name));
    json_append(out, json_string(cmd->arg));
    return out;
}

json_t *w26_sessioncon_578be4_envelope(const json_t *result) {
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < W26_SC_ENVELOPE_N; i++) {
        const json_t *v = result && result->type == JSON_OBJECT
                              ? json_obj_get(result, W26_SC_ENVELOPE[i]) : NULL;
        json_set(out, W26_SC_ENVELOPE[i], v ? json_copy(v) : json_null());
    }
    return out;
}

/* Python '==' between two snapshot revisions; a missing key is None. */
static int sc_py_eq(const json_t *a, const json_t *b) {
    int an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING || b->type == JSON_STRING) {
        return a->type == JSON_STRING && b->type == JSON_STRING
               && !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    }
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL)
        && (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) return json_len(a) == json_len(b);
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) return json_object_size(a) == json_object_size(b);
    return 0;
}

json_t *w26_sessioncon_578be4_publish_decision(const json_t *req) {
    long long published_seq = 0, ticket = 0;
    const json_t *snapshot_revision, *seq_v, *ticket_v, *rev_v;
    int published = 0;

    seq_v = req && req->type == JSON_OBJECT ? json_obj_get(req, "control_published_seq") : NULL;
    rev_v = req && req->type == JSON_OBJECT ? json_obj_get(req, "control_revision") : NULL;
    ticket_v = req && req->type == JSON_OBJECT ? json_obj_get(req, "ticket") : NULL;
    snapshot_revision = req && req->type == JSON_OBJECT ? json_obj_get(req, "snapshot_revision") : NULL;
    if (seq_v && seq_v->type == JSON_NUMBER) published_seq = (long long)seq_v->num_val;
    if (ticket_v && ticket_v->type == JSON_NUMBER) ticket = (long long)ticket_v->num_val;

    if (published_seq <= ticket) {
        int blocked = 0;
        if (sc_truthy(json_obj_get(req, "only_if_present")) && !sc_truthy(snapshot_revision)) blocked = 1;
        if (sc_truthy(json_obj_get(req, "only_if_changed")) && sc_py_eq(snapshot_revision, rev_v)) blocked = 1;
        if (!blocked) published = 1;
    }
    {
        json_t *out = json_object();
        json_set(out, "published", json_bool(published ? 1 : 0));
        json_set(out, "control_published_seq", json_number(published ? (double)ticket : (double)published_seq));
        json_set(out, "control_revision",
                 published ? (snapshot_revision ? json_copy(snapshot_revision) : json_null())
                           : (rev_v ? json_copy(rev_v) : json_null()));
        return out;
    }
}

json_t *w26_sessioncon_578be4_constants(void) {
    json_t *o = json_object(), *actions = json_object();
    json_t *subgoals = json_array(), *envelope = json_array();
    size_t i;
    for (i = 0; i < W26_SC_ACTION_N; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(W26_SC_ACTIONS[i].name));
        json_append(pair, json_string(W26_SC_ACTIONS[i].arg));
        json_set(actions, W26_SC_ACTIONS[i].action, pair);
    }
    for (i = 0; i < W26_SC_SUBGOAL_N; i++) json_append(subgoals, json_string(W26_SC_SUBGOALS[i]));
    for (i = 0; i < W26_SC_ENVELOPE_N; i++) json_append(envelope, json_string(W26_SC_ENVELOPE[i]));
    json_set(o, "action_commands", actions);
    json_set(o, "subgoal_actions", subgoals);
    json_set(o, "envelope", envelope);
    return o;
}
