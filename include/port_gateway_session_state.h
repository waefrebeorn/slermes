/* port_gateway_session_state.h — C11 port of gateway/session_state.py
 *
 * Per-session gateway state consolidated into one container
 * (TurnState / ConversationState / PersistentState / SessionState) plus the
 * legacy dict-view adapters (SessionFieldView / TurnLeaseTokenView) and the
 * LEGACY_FIELD_SPECS migration table.
 *
 * Faithful port notes:
 *  - Every field is stored as a json_t* (covers null/bool/number/string/
 *    array/object — the Python "Any"). Native scalars (float ts, int
 *    generation, bool, str) are *represented* as json_t* so the view layer is
 *    uniform, exactly mirroring Python's heterogeneous dataclass fields.
 *  - The _UNSET_TIER sentinel is a distinct json_t address (g_unset_tier);
 *    _present_not_unset compares by pointer identity, like Python `is`.
 *  - The session registry is insertion-ordered (omap) to preserve iteration
 *    order, mirroring Python dict insertion order observed by tests.
 *  - name parity: C symbols match the Python names (relay_session_state_*).
 */

#ifndef PORT_GATEWAY_SESSION_STATE_H
#define PORT_GATEWAY_SESSION_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "../lib/libjson/json.h"
#include "../lib/libomap/omap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Presence-test discriminators (mirror _present_not_none/_nonzero/_not_unset). */
typedef enum {
    PRESENCE_NOT_NONE,   /* value->type != JSON_NULL */
    PRESENCE_NOT_ZERO,   /* bool(value) */
    PRESENCE_NOT_UNSET   /* value != &g_unset_tier */
} presence_kind_t;

/* Scope of a state field (turn / conversation / persistent). */
typedef enum {
    SCOPE_TURN,
    SCOPE_CONVERSATION,
    SCOPE_PERSISTENT
} state_scope_t;

/* Field identifiers within each scope. */
typedef enum {
    TURN_AGENT,
    TURN_STARTED_TS,
    TURN_LEASE,
    TURN_BUSY_ACK_TS,
    TURN_LEASE_TOKEN,
    TURN_LEASE_GENERATION,
    TURN_FIELD_COUNT
} turn_field_t;

typedef enum {
    CONV_MODEL_OVERRIDE,
    CONV_ONE_TURN_RESTORE,
    CONV_REASONING_OVERRIDE,
    CONV_SERVICE_TIER_OVERRIDE,
    CONV_LAST_RESOLVED_MODEL,
    CONV_QUEUED_EVENTS,
    CONV_SIDECAR_NOTES,
    CONV_EPHEMERAL_PIN,
    CONV_VC_LAST,
    CONV_FIELD_COUNT
} conv_field_t;

typedef enum {
    PERS_APPROVALS,
    PERS_UPDATE_PROMPT_PENDING,
    PERS_NATIVE_IMAGE_PATHS,
    PERS_PENDING_COMMAND_TEXT,
    PERS_RUN_GENERATION,
    PERS_FIELD_COUNT
} pers_field_t;

/* --- State structs (opaque to callers via the header typedefs) --- */

typedef struct session_turn_state {
    json_t *agent;            /* Any */
    json_t *started_ts;       /* number (0.0 = not running) */
    json_t *lease;            /* Any */
    json_t *busy_ack_ts;      /* number (0.0 = never) */
    json_t *lease_token;      /* Any */
    json_t *lease_generation; /* number (0 = None / unset) */
} session_turn_state_t;

typedef struct session_conversation_state {
    json_t *model_override;        /* dict | null */
    json_t *one_turn_restore;      /* dict | null */
    json_t *reasoning_override;    /* dict | null */
    json_t *service_tier_override; /* Any (default = _UNSET_TIER sentinel) */
    json_t *last_resolved_model;   /* string */
    json_t *queued_events;         /* array */
    json_t *sidecar_notes;         /* array of string */
    json_t *ephemeral_pin;         /* tuple/array | null */
    json_t *vc_last;               /* string | null */
} session_conversation_state_t;

typedef struct session_persistent_state {
    json_t *approvals;             /* dict | null */
    json_t *update_prompt_pending; /* bool */
    json_t *native_image_paths;    /* array of string */
    json_t *pending_command_text;  /* string | null */
    json_t *run_generation;        /* number (monotonic, never reset) */
} session_persistent_state_t;

typedef struct session_state {
    session_turn_state_t        turn;
    session_conversation_state_t conversation;
    session_persistent_state_t  persistent;
} session_state_t;

/* --- Registry: maps session_key -> session_state_t* (insertion-ordered) --- */

typedef struct session_state_registry session_state_registry_t;

/* Create/destroy a registry. The value_free callback frees a session_state_t*. */
session_state_registry_t *relay_session_state_registry_new(void);
void relay_session_state_registry_free(session_state_registry_t *reg);

/* Get (create-if-absent) the SessionState for a session key. Mirrors
 * GatewayRunner._session_state(key). */
session_state_t *relay_session_state_get_or_create(session_state_registry_t *reg,
                                                   const char *session_key);

/* Get (no-create) — returns NULL if absent. */
session_state_t *relay_session_state_get(session_state_registry_t *reg,
                                         const char *session_key);

/* Number of sessions currently present. */
size_t relay_session_state_count(session_state_registry_t *reg);

/* Iterate sessions in insertion order. out_keys/out_states are caller-allocated
 * arrays of size relay_session_state_count(); returns the count written. */
size_t relay_session_state_items(session_state_registry_t *reg,
                                 const char ***out_keys,
                                 session_state_t ***out_states);

/* --- State-scope clear (turn.clear / conversation.clear semantics) --- */

void relay_session_state_turn_clear(session_turn_state_t *t);
void relay_session_state_conversation_clear(session_conversation_state_t *c);

/* --- Legacy dict-field view (SessionFieldView) --- */

typedef struct session_field_view session_field_view_t;

/* Legacy attribute names from LEGACY_FIELD_SPECS. */
typedef enum {
    LEGACY_RUNNING_AGENTS,
    LEGACY_RUNNING_AGENTS_TS,
    LEGACY_ACTIVE_SESSION_LEASES,
    LEGACY_BUSY_ACK_TS,
    LEGACY_SESSION_MODEL_OVERRIDES,
    LEGACY_PENDING_ONE_TURN_MODEL_RESTORES,
    LEGACY_SESSION_REASONING_OVERRIDES,
    LEGACY_SESSION_SERVICE_TIER_OVERRIDES,
    LEGACY_LAST_RESOLVED_MODEL,
    LEGACY_QUEUED_EVENTS,
    LEGACY_PENDING_TURN_SIDECAR_NOTES,
    LEGACY_SESSION_EPHEMERAL_PIN,
    LEGACY_SESSION_VC_LAST,
    LEGACY_PENDING_APPROVALS,
    LEGACY_UPDATE_PROMPT_PENDING,
    LEGACY_PENDING_NATIVE_IMAGE_PATHS_BY_SESSION,
    LEGACY_PENDING_MESSAGES,
    LEGACY_SESSION_RUN_GENERATION,
    LEGACY_FIELD_COUNT
} legacy_field_t;

/* Build a live view over one legacy field across all sessions. Mirrors
 * legacy_dict_property(attr_name) returning a SessionFieldView. */
session_field_view_t *relay_session_field_view_new(session_state_registry_t *reg,
                                                   legacy_field_t attr);

void relay_session_field_view_free(session_field_view_t *view);

/* Mapping protocol (raise KeyError via return codes; NULL on absent). */
#define SFV_OK            0
#define SFV_KEY_ERROR     1   /* missing key / not-present */
#define SFV_TYPE_ERROR    2   /* bad key type */

/* get: returns borrowed json_t* (do NOT free); NULL + *rc=KEY_ERROR if absent. */
json_t *relay_session_field_view_get(session_field_view_t *view,
                                     const char *key, int *rc);
/* set: takes ownership-transfer of `value` (it is stored / freed by the view). */
int relay_session_field_view_set(session_field_view_t *view,
                                 const char *key, json_t *value);
/* del: removes the entry (resets to default). SFV_KEY_ERROR if absent. */
int relay_session_field_view_del(session_field_view_t *view, const char *key);
/* True if key present. */
bool relay_session_field_view_contains(session_field_view_t *view, const char *key);
/* Reset the field to default on every session (view.clear()). */
void relay_session_field_view_clear(session_field_view_t *view);
/* Iterate present keys in insertion order. Returns count; fills out_keys
 * (caller array of size relay_session_state_count(reg)). */
size_t relay_session_field_view_keys(session_field_view_t *view,
                                     const char ***out_keys);
/* __len__ / __repr__ / __ne__ (SessionFieldView) */
size_t relay_session_field_view_len(session_field_view_t *view);
char *relay_session_field_view_repr(session_field_view_t *view);
bool relay_session_field_view_ne(session_field_view_t *view, const omap_t *other);
/* Equality against a plain omap/json snapshot (dict comparison). */
bool relay_session_field_view_eq(session_field_view_t *view, const omap_t *other);

/* --- Legacy turn-lease-token view ((session_key, generation) -> token) --- */

typedef struct turn_lease_token_view turn_lease_token_view_t;

turn_lease_token_view_t *relay_turn_lease_token_view_new(session_state_registry_t *reg);
void relay_turn_lease_token_view_free(turn_lease_token_view_t *view);

/* key is a 2-element json array [session_key:string, generation:number]. */
json_t *relay_turn_lease_token_view_get(turn_lease_token_view_t *view,
                                        const json_t *key, int *rc);
int relay_turn_lease_token_view_set(turn_lease_token_view_t *view,
                                    const json_t *key, json_t *value);
int relay_turn_lease_token_view_del(turn_lease_token_view_t *view,
                                    const json_t *key);
bool relay_turn_lease_token_view_contains(turn_lease_token_view_t *view,
                                          const json_t *key);
void relay_turn_lease_token_view_clear(turn_lease_token_view_t *view);
/* __len__ (TurnLeaseTokenView) */
size_t relay_turn_lease_token_view_len(turn_lease_token_view_t *view);
/* Iterate present (session_key, generation) pairs in insertion order. */
size_t relay_turn_lease_token_view_keys(turn_lease_token_view_t *view,
                                        json_t ***out_keys);

/* The public _UNSET_TIER sentinel value. */
json_t *relay_session_state_unset_tier(void);

/* Convenience: deep-copy a json_t (used when values move into the registry). */
json_t *relay_session_state_copy(const json_t *v);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GATEWAY_SESSION_STATE_H */
