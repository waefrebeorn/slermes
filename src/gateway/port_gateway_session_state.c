/* port_gateway_session_state.c — C11 port of gateway/session_state.py
 *
 * See port_gateway_session_state.h for the faithful-port contract.
 */

#include "port_gateway_session_state.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ===========================================================================
 * Sentinel + json helpers
 * ======================================================================== */

/* Distinct json_t address used as the _UNSET_TIER sentinel (like Python's
 * module-level `object()`). We never serialize it or free it. */
static json_t g_unset_tier = { JSON_NULL, { .str_val = (char *)"_UNSET_TIER" } };

json_t *relay_session_state_unset_tier(void) { return &g_unset_tier; }

json_t *relay_session_state_copy(const json_t *v) {
    if (!v) return json_null();
    /* Preserve the _UNSET_TIER sentinel by identity (Python `is` semantics). */
    if (v == &g_unset_tier) return (json_t *)&g_unset_tier;
    switch (v->type) {
    case JSON_NULL:   return json_null();
    case JSON_BOOL:   return json_bool(v->bool_val);
    case JSON_NUMBER: return json_number(v->num_val);
    case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
    case JSON_ARRAY: {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) {
            json_append(a, relay_session_state_copy(v->c.items[i]));
        }
        return a;
    }
    case JSON_OBJECT: {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++) {
            json_set(o, v->c.keys[i], relay_session_state_copy(v->c.items[i]));
        }
        return o;
    }
    }
    return json_null();
}

static void json_t_free(json_t *v) {
    if (v && v != &g_unset_tier) json_free(v);
}

/* ===========================================================================
 * State struct construction / destruction
 * ======================================================================== */

static session_state_t *session_state_new(void) {
    session_state_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    /* TurnState defaults */
    s->turn.agent = json_null();
    s->turn.started_ts = json_number(0.0);
    s->turn.lease = json_null();
    s->turn.busy_ack_ts = json_number(0.0);
    s->turn.lease_token = json_null();
    s->turn.lease_generation = json_number(0);
    /* ConversationState defaults */
    s->conversation.model_override = json_null();
    s->conversation.one_turn_restore = json_null();
    s->conversation.reasoning_override = json_null();
    s->conversation.service_tier_override = &g_unset_tier;
    s->conversation.last_resolved_model = json_string("");
    s->conversation.queued_events = json_array();
    s->conversation.sidecar_notes = json_array();
    s->conversation.ephemeral_pin = json_null();
    s->conversation.vc_last = json_null();
    /* PersistentState defaults */
    s->persistent.approvals = json_null();
    s->persistent.update_prompt_pending = json_bool(false);
    s->persistent.native_image_paths = json_array();
    s->persistent.pending_command_text = json_null();
    s->persistent.run_generation = json_number(0);
    return s;
}

static void session_state_free(session_state_t *s) {
    if (!s) return;
    json_t_free(s->turn.agent);
    json_t_free(s->turn.started_ts);
    json_t_free(s->turn.lease);
    json_t_free(s->turn.busy_ack_ts);
    json_t_free(s->turn.lease_token);
    json_t_free(s->turn.lease_generation);
    json_t_free(s->conversation.model_override);
    json_t_free(s->conversation.one_turn_restore);
    json_t_free(s->conversation.reasoning_override);
    if (s->conversation.service_tier_override != &g_unset_tier)
        json_t_free(s->conversation.service_tier_override);
    json_t_free(s->conversation.last_resolved_model);
    json_t_free(s->conversation.queued_events);
    json_t_free(s->conversation.sidecar_notes);
    json_t_free(s->conversation.ephemeral_pin);
    json_t_free(s->conversation.vc_last);
    json_t_free(s->persistent.approvals);
    json_t_free(s->persistent.update_prompt_pending);
    json_t_free(s->persistent.native_image_paths);
    json_t_free(s->persistent.pending_command_text);
    json_t_free(s->persistent.run_generation);
    free(s);
}

/* ===========================================================================
 * Registry
 * ======================================================================== */

struct session_state_registry {
    omap_t *sessions; /* session_key -> session_state_t* */
};

/* PoP: relay_session_state_registry_new @ gateway/session_state.py:SessionState */
/* PoP: relay_session_state_registry_new @ gateway/session_state.py:SessionState.__init__ */
session_state_registry_t *relay_session_state_registry_new(void) {
    session_state_registry_t *reg = calloc(1, sizeof(*reg));
    if (!reg) return NULL;
    reg->sessions = omap_new((omap_value_free_fn)session_state_free);
    return reg;
}

void relay_session_state_registry_free(session_state_registry_t *reg) {
    if (!reg) return;
    omap_free(reg->sessions);
    free(reg);
}

session_state_t *relay_session_state_get_or_create(session_state_registry_t *reg,
                                                  const char *session_key) {
    session_state_t *s = omap_get(reg->sessions, session_key);
    if (s) return s;
    s = session_state_new();
    omap_set(reg->sessions, session_key, s);
    return s;
}

/* PoP: relay_session_state_get @ gateway/session_state.py:SessionFieldView._sessions */
session_state_t *relay_session_state_get(session_state_registry_t *reg,
                                         const char *session_key) {
    return omap_get(reg->sessions, session_key);
}

size_t relay_session_state_count(session_state_registry_t *reg) {
    return omap_size(reg->sessions);
}

size_t relay_session_state_items(session_state_registry_t *reg,
                                const char ***out_keys,
                                session_state_t ***out_states) {
    size_t n = omap_size(reg->sessions);
    const char **keys = malloc(sizeof(char *) * (n ? n : 1));
    session_state_t **states = malloc(sizeof(session_state_t *) * (n ? n : 1));
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = NULL;
        void *v = NULL;
        omap_at(reg->sessions, i, &k, &v);
        keys[w] = k;
        states[w] = v;
        w++;
    }
    *out_keys = keys;
    *out_states = states;
    return w;
}

/* ===========================================================================
 * Clear semantics (turn.clear / conversation.clear)
 * ======================================================================== */

/* PoP: relay_session_state_turn_clear @ gateway/session_state.py:TurnState.clear */
void relay_session_state_turn_clear(session_turn_state_t *t) {
    json_t *agent = json_null();
    json_t *started = json_number(0.0);
    json_t *lease = json_null();
    json_t *busy = json_number(0.0);
    json_t_free(t->agent); t->agent = agent;
    json_t_free(t->started_ts); t->started_ts = started;
    json_t_free(t->lease); t->lease = lease;
    json_t_free(t->busy_ack_ts); t->busy_ack_ts = busy;
    /* lease_token / lease_generation are intentionally NOT cleared here — they
     * are owned by _release_turn_lease (#64934). */
}

/* PoP: relay_session_state_conversation_clear @ gateway/session_state.py:ConversationState.clear */
void relay_session_state_conversation_clear(session_conversation_state_t *c) {
    json_t *model = json_null();
    json_t *restore = json_null();
    json_t *reasoning = json_null();
    json_t *tier = (json_t *)&g_unset_tier;
    json_t *last = json_string("");
    json_t *queued = json_array();
    json_t *sidecar = json_array();
    json_t *pin = json_null();
    json_t *vc = json_null();
    json_t_free(c->model_override); c->model_override = model;
    json_t_free(c->one_turn_restore); c->one_turn_restore = restore;
    json_t_free(c->reasoning_override); c->reasoning_override = reasoning;
    if (c->service_tier_override != &g_unset_tier) json_t_free(c->service_tier_override);
    c->service_tier_override = tier;
    json_t_free(c->last_resolved_model); c->last_resolved_model = last;
    json_t_free(c->queued_events); c->queued_events = queued;
    json_t_free(c->sidecar_notes); c->sidecar_notes = sidecar;
    json_t_free(c->ephemeral_pin); c->ephemeral_pin = pin;
    json_t_free(c->vc_last); c->vc_last = vc;
}

/* ===========================================================================
 * Field table — one entry per (scope, field) with default + presence test.
 * ======================================================================== */

typedef json_t *(*default_fn)(void);
typedef bool (*presence_fn)(const json_t *v);

static json_t *dflt_null(void)   { return json_null(); }
static json_t *dflt_zero(void)   { return json_number(0.0); }
static json_t *dflt_zero_i(void) { return json_number(0); }
static json_t *dflt_str(void)    { return json_string(""); }
static json_t *dflt_arr(void)    { return json_array(); }
static json_t *dflt_bool(void)   { return json_bool(false); }
static json_t *dflt_unset(void)  { return (json_t *)&g_unset_tier; }

/* PoP: pres_not_none @ gateway/session_state.py:_present_not_none */
static bool pres_not_none(const json_t *v) { return v->type != JSON_NULL; }
/* PoP: pres_not_unset @ gateway/session_state.py:_present_not_unset */
static bool pres_not_unset(const json_t *v) { return v != &g_unset_tier; }
/* PoP: pres_not_zero @ gateway/session_state.py:_present_nonzero */
static bool pres_not_zero(const json_t *v) {
    switch (v->type) {
    case JSON_NULL:   return false;
    case JSON_BOOL:   return v->bool_val;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return v->c.count > 0;
    case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* Resolve a (scope, field) pair to the address of the json_t* slot inside a
 * session_state_t. This lets the view layer read/write uniformly. */
/* PoP: field_slot @ gateway/session_state.py:SessionFieldView._value */
static json_t **field_slot(session_state_t *s, state_scope_t scope, int field) {
    switch (scope) {
    case SCOPE_TURN:
        switch ((turn_field_t)field) {
        case TURN_AGENT:            return &s->turn.agent;
        case TURN_STARTED_TS:       return &s->turn.started_ts;
        case TURN_LEASE:            return &s->turn.lease;
        case TURN_BUSY_ACK_TS:      return &s->turn.busy_ack_ts;
        case TURN_LEASE_TOKEN:      return &s->turn.lease_token;
        case TURN_LEASE_GENERATION: return &s->turn.lease_generation;
        case TURN_FIELD_COUNT:      return NULL;
        }
        break;
    case SCOPE_CONVERSATION:
        switch ((conv_field_t)field) {
        case CONV_MODEL_OVERRIDE:        return &s->conversation.model_override;
        case CONV_ONE_TURN_RESTORE:      return &s->conversation.one_turn_restore;
        case CONV_REASONING_OVERRIDE:    return &s->conversation.reasoning_override;
        case CONV_SERVICE_TIER_OVERRIDE: return &s->conversation.service_tier_override;
        case CONV_LAST_RESOLVED_MODEL:   return &s->conversation.last_resolved_model;
        case CONV_QUEUED_EVENTS:         return &s->conversation.queued_events;
        case CONV_SIDECAR_NOTES:         return &s->conversation.sidecar_notes;
        case CONV_EPHEMERAL_PIN:         return &s->conversation.ephemeral_pin;
        case CONV_VC_LAST:               return &s->conversation.vc_last;
        case CONV_FIELD_COUNT:            return NULL;
        }
        break;
    case SCOPE_PERSISTENT:
        switch ((pers_field_t)field) {
        case PERS_APPROVALS:             return &s->persistent.approvals;
        case PERS_UPDATE_PROMPT_PENDING: return &s->persistent.update_prompt_pending;
        case PERS_NATIVE_IMAGE_PATHS:    return &s->persistent.native_image_paths;
        case PERS_PENDING_COMMAND_TEXT:  return &s->persistent.pending_command_text;
        case PERS_RUN_GENERATION:        return &s->persistent.run_generation;
        case PERS_FIELD_COUNT:           return NULL;
        }
        break;
    }
    return NULL;
}

/* ===========================================================================
 * SessionFieldView (legacy dict-view over one field across all sessions)
 * ======================================================================== */

struct session_field_view {
    session_state_registry_t *reg;
    state_scope_t scope;
    int field;
    default_fn dflt;
    presence_fn pres;
};

/* Legacy field spec table (mirrors LEGACY_FIELD_SPECS). */
typedef struct {
    legacy_field_t id;
    const char *attr;
    state_scope_t scope;
    int field;
    default_fn dflt;
    presence_fn pres;
} legacy_spec_t;

static const legacy_spec_t LEGACY_SPECS[] = {
    { LEGACY_RUNNING_AGENTS,                       "_running_agents",                 SCOPE_TURN,        TURN_AGENT,                  dflt_null,  pres_not_none },
    { LEGACY_RUNNING_AGENTS_TS,                    "_running_agents_ts",              SCOPE_TURN,        TURN_STARTED_TS,             dflt_zero,  pres_not_zero },
    { LEGACY_ACTIVE_SESSION_LEASES,                "_active_session_leases",          SCOPE_TURN,        TURN_LEASE,                  dflt_null,  pres_not_none },
    { LEGACY_BUSY_ACK_TS,                          "_busy_ack_ts",                    SCOPE_TURN,        TURN_BUSY_ACK_TS,            dflt_zero,  pres_not_zero },
    { LEGACY_SESSION_MODEL_OVERRIDES,              "_session_model_overrides",        SCOPE_CONVERSATION, CONV_MODEL_OVERRIDE,       dflt_null,  pres_not_none },
    { LEGACY_PENDING_ONE_TURN_MODEL_RESTORES,      "_pending_one_turn_model_restores", SCOPE_CONVERSATION, CONV_ONE_TURN_RESTORE,    dflt_null,  pres_not_none },
    { LEGACY_SESSION_REASONING_OVERRIDES,          "_session_reasoning_overrides",    SCOPE_CONVERSATION, CONV_REASONING_OVERRIDE,   dflt_null,  pres_not_none },
    { LEGACY_SESSION_SERVICE_TIER_OVERRIDES,       "_session_service_tier_overrides", SCOPE_CONVERSATION, CONV_SERVICE_TIER_OVERRIDE, dflt_unset, pres_not_unset },
    { LEGACY_LAST_RESOLVED_MODEL,                  "_last_resolved_model",            SCOPE_CONVERSATION, CONV_LAST_RESOLVED_MODEL,  dflt_str,   pres_not_zero },
    { LEGACY_QUEUED_EVENTS,                        "_queued_events",                  SCOPE_CONVERSATION, CONV_QUEUED_EVENTS,         dflt_arr,   pres_not_zero },
    { LEGACY_PENDING_TURN_SIDECAR_NOTES,           "_pending_turn_sidecar_notes",     SCOPE_CONVERSATION, CONV_SIDECAR_NOTES,        dflt_arr,   pres_not_zero },
    { LEGACY_SESSION_EPHEMERAL_PIN,                "_session_ephemeral_pin",          SCOPE_CONVERSATION, CONV_EPHEMERAL_PIN,        dflt_null,  pres_not_none },
    { LEGACY_SESSION_VC_LAST,                      "_session_vc_last",                SCOPE_CONVERSATION, CONV_VC_LAST,               dflt_null,  pres_not_none },
    { LEGACY_PENDING_APPROVALS,                    "_pending_approvals",              SCOPE_PERSISTENT,  PERS_APPROVALS,             dflt_null,  pres_not_none },
    { LEGACY_UPDATE_PROMPT_PENDING,                "_update_prompt_pending",          SCOPE_PERSISTENT,  PERS_UPDATE_PROMPT_PENDING, dflt_bool,  pres_not_zero },
    { LEGACY_PENDING_NATIVE_IMAGE_PATHS_BY_SESSION,"_pending_native_image_paths_by_session", SCOPE_PERSISTENT, PERS_NATIVE_IMAGE_PATHS, dflt_arr, pres_not_zero },
    { LEGACY_PENDING_MESSAGES,                     "_pending_messages",               SCOPE_PERSISTENT,  PERS_PENDING_COMMAND_TEXT,  dflt_null,  pres_not_none },
    { LEGACY_SESSION_RUN_GENERATION,               "_session_run_generation",         SCOPE_PERSISTENT,  PERS_RUN_GENERATION,        dflt_zero_i,pres_not_zero },
};

/* PoP: relay_session_field_view_new @ gateway/session_state.py:legacy_dict_property */
/* PoP: relay_session_field_view_new @ gateway/session_state.py:SessionFieldView.__init__ */
session_field_view_t *relay_session_field_view_new(session_state_registry_t *reg,
                                                  legacy_field_t attr) {
    if ((int)attr < 0 || (int)attr >= (int)LEGACY_FIELD_COUNT) return NULL;
    const legacy_spec_t *spec = &LEGACY_SPECS[attr];
    session_field_view_t *v = calloc(1, sizeof(*v));
    v->reg = reg;
    v->scope = spec->scope;
    v->field = spec->field;
    v->dflt = spec->dflt;
    v->pres = spec->pres;
    return v;
}

void relay_session_field_view_free(session_field_view_t *view) { free(view); }

/* PoP: SessionFieldView.__getitem__ @ gateway/session_state.py:SessionFieldView.__getitem__ */
/* PoP: relay_session_field_view_get @ gateway/session_state.py:SessionFieldView.__getitem__ */
json_t *relay_session_field_view_get(session_field_view_t *view,
                                     const char *key, int *rc) {
    session_state_t *s = relay_session_state_get(view->reg, key);
    if (!s) { *rc = SFV_KEY_ERROR; return NULL; }
    json_t **slot = field_slot(s, view->scope, view->field);
    json_t *value = *slot;
    if (!view->pres(value)) { *rc = SFV_KEY_ERROR; return NULL; }
    *rc = SFV_OK;
    return value; /* borrowed */
}

/* PoP: SessionFieldView.__setitem__ @ gateway/session_state.py:SessionFieldView.__setitem__ */
/* PoP: relay_session_field_view_set @ gateway/session_state.py:SessionFieldView.__setitem__ */
int relay_session_field_view_set(session_field_view_t *view,
                                 const char *key, json_t *value) {
    session_state_t *s = relay_session_state_get_or_create(view->reg, key);
    json_t **slot = field_slot(s, view->scope, view->field);
    json_t *copy = relay_session_state_copy(value);
    json_t *old = *slot;
    /* service_tier_override sentinel is not owned by the struct unless it was
     * already a heap value; the slot replace below handles ownership. */
    if (old != &g_unset_tier) json_t_free(old);
    *slot = copy;
    return SFV_OK;
}

/* PoP: SessionFieldView.__delitem__ @ gateway/session_state.py:SessionFieldView.__delitem__ */
/* PoP: relay_session_field_view_del @ gateway/session_state.py:SessionFieldView.__delitem__ */
int relay_session_field_view_del(session_field_view_t *view, const char *key) {
    session_state_t *s = relay_session_state_get(view->reg, key);
    if (!s) return SFV_KEY_ERROR;
    json_t **slot = field_slot(s, view->scope, view->field);
    if (!view->pres(*slot)) return SFV_KEY_ERROR;
    json_t *old = *slot;
    *slot = view->dflt();
    if (old != &g_unset_tier) json_t_free(old);
    return SFV_OK;
}

/* PoP: relay_session_field_view_contains @ gateway/session_state.py:SessionFieldView.__contains__ */
bool relay_session_field_view_contains(session_field_view_t *view, const char *key) {
    session_state_t *s = relay_session_state_get(view->reg, key);
    if (!s) return false;
    json_t **slot = field_slot(s, view->scope, view->field);
    return view->pres(*slot);
}

/* PoP: SessionFieldView.clear @ gateway/session_state.py:SessionFieldView.clear */
/* PoP: relay_session_field_view_clear @ gateway/session_state.py:SessionFieldView.clear */
void relay_session_field_view_clear(session_field_view_t *view) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    for (size_t i = 0; i < n; i++) {
        json_t **slot = field_slot(states[i], view->scope, view->field);
        json_t *old = *slot;
        *slot = view->dflt();
        if (old != &g_unset_tier) json_t_free(old);
    }
    free(keys); free(states);
}

/* PoP: SessionFieldView.__iter__ @ gateway/session_state.py:SessionFieldView.__iter__ */
/* PoP: relay_session_field_view_keys @ gateway/session_state.py:SessionFieldView.__iter__ */
size_t relay_session_field_view_keys(session_field_view_t *view,
                                     const char ***out_keys) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    /* collect present keys */
    const char **present = malloc(sizeof(char *) * (n ? n : 1));
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        json_t **slot = field_slot(states[i], view->scope, view->field);
        if (view->pres(*slot)) present[w++] = keys[i];
    }
    free(keys); free(states);
    *out_keys = present;
    return w;
}

/* PoP: relay_session_field_view_eq @ gateway/session_state.py:SessionFieldView.__eq__ */
bool relay_session_field_view_eq(session_field_view_t *view, const omap_t *other) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    if (n != omap_size(other)) { free(keys); free(states); return false; }
    for (size_t i = 0; i < n; i++) {
        json_t **slot = field_slot(states[i], view->scope, view->field);
        if (!view->pres(*slot)) {
            /* absent on this side */
            if (omap_contains(other, keys[i])) { free(keys); free(states); return false; }
            continue;
        }
        json_t *o = omap_get(other, keys[i]);
        if (!o) { free(keys); free(states); return false; }
        /* compare JSON equality via serialization */
        char *a = json_serialize(*slot);
        char *b = json_serialize(o);
        bool eq = a && b && strcmp(a, b) == 0;
        free(a); free(b);
        if (!eq) { free(keys); free(states); return false; }
    }
    free(keys); free(states);
    return true;
}

/* PoP: relay_session_field_view_len @ gateway/session_state.py:SessionFieldView.__len__ */
size_t relay_session_field_view_len(session_field_view_t *view) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        json_t **slot = field_slot(states[i], view->scope, view->field);
        if (view->pres(*slot)) w++;
    }
    free(keys); free(states);
    return w;
}

/* PoP: relay_session_field_view_repr @ gateway/session_state.py:SessionFieldView.__repr__ */
char *relay_session_field_view_repr(session_field_view_t *view) {
    const char **keys = NULL;
    size_t n = relay_session_field_view_keys(view, &keys);
    /* Build: SessionFieldView(scope.name, {k: v, ...}) */
    size_t cap = 64;
    char *buf = malloc(cap);
    size_t len = 0;
    const char *scope = view->scope == SCOPE_TURN ? "turn"
                      : view->scope == SCOPE_CONVERSATION ? "conversation" : "persistent";
    len += (size_t)snprintf(buf + len, cap - len, "SessionFieldView(%s, {", scope);
    for (size_t i = 0; i < n; i++) {
        session_state_t *s = relay_session_state_get(view->reg, keys[i]);
        json_t **slot = s ? field_slot(s, view->scope, view->field) : NULL;
        char *v = slot ? json_serialize(*slot) : NULL;
        len += (size_t)snprintf(buf + len, cap - len, "%s\"%s\": %s",
                                i ? ", " : "", keys[i], v ? v : "null");
        free(v);
    }
    len += (size_t)snprintf(buf + len, cap - len, "})");
    free(keys);
    return buf; /* caller frees */
}

/* PoP: relay_session_field_view_ne @ gateway/session_state.py:SessionFieldView.__ne__ */
bool relay_session_field_view_ne(session_field_view_t *view, const omap_t *other) {
    return !relay_session_field_view_eq(view, other);
}

/* ===========================================================================
 * TurnLeaseTokenView ((session_key, generation) -> token)
 * ======================================================================== */

struct turn_lease_token_view {
    session_state_registry_t *reg;
};

/* PoP: relay_turn_lease_token_view_new @ gateway/session_state.py:legacy_lease_token_property */
/* PoP: relay_turn_lease_token_view_new @ gateway/session_state.py:TurnLeaseTokenView.__init__ */
turn_lease_token_view_t *relay_turn_lease_token_view_new(session_state_registry_t *reg) {
    turn_lease_token_view_t *v = calloc(1, sizeof(*v));
    v->reg = reg;
    return v;
}

void relay_turn_lease_token_view_free(turn_lease_token_view_t *view) { free(view); }

/* PoP: split_key @ gateway/session_state.py:TurnLeaseTokenView._split */
static bool split_key(const json_t *key, const char **out_sk, long *out_gen) {
    if (!key || key->type != JSON_ARRAY || key->c.count != 2) return false;
    json_t *a = key->c.items[0];
    json_t *b = key->c.items[1];
    if (!a || a->type != JSON_STRING) return false;
    if (!b || b->type != JSON_NUMBER) return false;
    *out_sk = a->str_val;
    *out_gen = (long)b->num_val;
    return true;
}

/* PoP: TurnLeaseTokenView.__getitem__ @ gateway/session_state.py:TurnLeaseTokenView.__getitem__ */
/* PoP: relay_turn_lease_token_view_get @ gateway/session_state.py:TurnLeaseTokenView.__getitem__ */
json_t *relay_turn_lease_token_view_get(turn_lease_token_view_t *view,
                                        const json_t *key, int *rc) {
    const char *sk; long gen;
    if (!split_key(key, &sk, &gen)) { *rc = SFV_KEY_ERROR; return NULL; }
    session_state_t *s = relay_session_state_get(view->reg, sk);
    if (!s) { *rc = SFV_KEY_ERROR; return NULL; }
    if (s->turn.lease_token->type == JSON_NULL) { *rc = SFV_KEY_ERROR; return NULL; }
    long cur = (long)(s->turn.lease_generation->num_val);
    if (cur != gen) { *rc = SFV_KEY_ERROR; return NULL; }
    *rc = SFV_OK;
    return s->turn.lease_token; /* borrowed */
}

/* PoP: TurnLeaseTokenView.__setitem__ @ gateway/session_state.py:TurnLeaseTokenView.__setitem__ */
/* PoP: relay_turn_lease_token_view_set @ gateway/session_state.py:TurnLeaseTokenView.__setitem__ */
int relay_turn_lease_token_view_set(turn_lease_token_view_t *view,
                                    const json_t *key, json_t *value) {
    const char *sk; long gen;
    if (!split_key(key, &sk, &gen)) return SFV_KEY_ERROR;
    session_state_t *s = relay_session_state_get_or_create(view->reg, sk);
    json_t_free(s->turn.lease_token);
    json_t_free(s->turn.lease_generation);
    s->turn.lease_token = relay_session_state_copy(value);
    s->turn.lease_generation = json_number((double)gen);
    return SFV_OK;
}

/* PoP: TurnLeaseTokenView.__delitem__ @ gateway/session_state.py:TurnLeaseTokenView.__delitem__ */
/* PoP: relay_turn_lease_token_view_del @ gateway/session_state.py:TurnLeaseTokenView.__delitem__ */
int relay_turn_lease_token_view_del(turn_lease_token_view_t *view,
                                    const json_t *key) {
    const char *sk; long gen;
    if (!split_key(key, &sk, &gen)) return SFV_KEY_ERROR;
    session_state_t *s = relay_session_state_get(view->reg, sk);
    if (!s) return SFV_KEY_ERROR;
    if (s->turn.lease_token->type == JSON_NULL) return SFV_KEY_ERROR;
    if ((long)(s->turn.lease_generation->num_val) != gen) return SFV_KEY_ERROR;
    json_t_free(s->turn.lease_token);
    json_t_free(s->turn.lease_generation);
    s->turn.lease_token = json_null();
    s->turn.lease_generation = json_number(0);
    return SFV_OK;
}

/* PoP: relay_turn_lease_token_view_contains @ gateway/session_state.py:TurnLeaseTokenView.__contains__ */
bool relay_turn_lease_token_view_contains(turn_lease_token_view_t *view,
                                          const json_t *key) {
    const char *sk; long gen;
    if (!split_key(key, &sk, &gen)) return false;
    session_state_t *s = relay_session_state_get(view->reg, sk);
    if (!s) return false;
    if (s->turn.lease_token->type == JSON_NULL) return false;
    return (long)(s->turn.lease_generation->num_val) == gen;
}

void relay_turn_lease_token_view_clear(turn_lease_token_view_t *view) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    for (size_t i = 0; i < n; i++) {
        if (states[i]->turn.lease_token->type != JSON_NULL) {
            json_t_free(states[i]->turn.lease_token);
            json_t_free(states[i]->turn.lease_generation);
            states[i]->turn.lease_token = json_null();
            states[i]->turn.lease_generation = json_number(0);
        }
    }
    free(keys); free(states);
}

/* PoP: TurnLeaseTokenView.__iter__ @ gateway/session_state.py:TurnLeaseTokenView.__iter__ */
/* PoP: relay_turn_lease_token_view_keys @ gateway/session_state.py:TurnLeaseTokenView.__iter__ */
size_t relay_turn_lease_token_view_keys(turn_lease_token_view_t *view,
                                        json_t ***out_keys) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    json_t **pairs = malloc(sizeof(json_t *) * (n ? n : 1));
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        if (states[i]->turn.lease_token->type != JSON_NULL) {
            json_t *pair = json_array();
            json_append(pair, json_string(keys[i]));
            json_append(pair, json_number(states[i]->turn.lease_generation->num_val));
            pairs[w++] = pair;
        }
    }
    free(keys); free(states);
    *out_keys = pairs;
    return w;
}

/* PoP: relay_turn_lease_token_view_len @ gateway/session_state.py:TurnLeaseTokenView.__len__ */
size_t relay_turn_lease_token_view_len(turn_lease_token_view_t *view) {
    const char **keys = NULL;
    session_state_t **states = NULL;
    size_t n = relay_session_state_items(view->reg, &keys, &states);
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        if (states[i]->turn.lease_token->type != JSON_NULL) w++;
    }
    free(keys); free(states);
    return w;
}
