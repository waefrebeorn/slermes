/*
 * contract.c -- real C11 port of the tables and enums in upstream Hermes
 *   tools/connectors/contract.py.
 *
 * PoP: w20_contract_80eced_resolves @ tools/connectors/contract.py:resolves
 * PoP: w20_contract_80eced_allowed @ tools/connectors/contract.py:allowed
 * PoP: w20_contract_80eced_transitions @ tools/connectors/contract.py:TRANSITIONS
 * PoP: w20_contract_80eced_kinds @ tools/connectors/contract.py:KINDS
 * PoP: w20_contract_80eced_resolved_states @ tools/connectors/contract.py:RESOLVED_STATES
 * PoP: w20_contract_80eced_enum_values @ tools/connectors/contract.py:TargetState
 *
 * Everything in the module is a pure table lookup, so the whole contract is
 * ported here; `operation.py` (which enforces it against a live catalog) is a
 * separate module and stays upstream.
 */
#include "hermes_json.h"
#include "noc_w20_contract_80eced.h"

#include <string.h>

/* --- enums, in declaration order ------------------------------------------ */

static const char *const CONTRACTED_TARGET_STATES[] = {
    "pending", "initiated", "connected", "skipped", "failed", "expired", "not_connected", NULL
};

static const char *const CONTRACTED_ACTORS[] = {
    "user", "backend_watcher", "clock", NULL
};

static const char *const CONTRACTED_SETTLE_REASONS[] = {
    "all_resolved", "continue", "deadline", "interrupt", NULL
};

static const char *const CONTRACTED_KINDS[] = {
    "connector", "mcp", "plugin", "skill", NULL
};

/* --- transition table ----------------------------------------------------- */

typedef struct {
    const char *kind;
    const char *from;
    const char *to;
    const char *actor;
} contracted_edge;

static const contracted_edge CONTRACTED_EDGES[] = {
    /* connector */
    {"connector", "pending",   "initiated", "backend_watcher"},
    {"connector", "pending",   "failed",    "backend_watcher"},
    {"connector", "pending",   "skipped",   "user"},
    {"connector", "initiated", "connected", "backend_watcher"},
    {"connector", "initiated", "failed",    "backend_watcher"},
    {"connector", "initiated", "expired",   "clock"},
    {"connector", "initiated", "skipped",   "user"},
    {"connector", "failed",    "initiated", "user"},
    {"connector", "failed",    "skipped",   "user"},
    {"connector", "expired",   "initiated", "user"},
    {"connector", "expired",   "skipped",   "user"},
    /* mcp -- plugin and skill copy these edges verbatim */
    {"mcp", "pending",   "initiated", "backend_watcher"},
    {"mcp", "pending",   "failed",    "backend_watcher"},
    {"mcp", "pending",   "skipped",   "user"},
    {"mcp", "initiated", "connected", "backend_watcher"},
    {"mcp", "initiated", "failed",    "backend_watcher"},
    {"mcp", "initiated", "skipped",   "user"},
    {"mcp", "failed",    "initiated", "user"},
    {"mcp", "failed",    "skipped",   "user"},
    {NULL, NULL, NULL, NULL}
};

/* plugin and skill key off the mcp rows; the module builds them by copy. */
static int contracted_kind_is_mcp_copy(const char *kind) {
    return kind && (!strcmp(kind, "plugin") || !strcmp(kind, "skill"));
}

static int contracted_kind_known(const char *kind) {
    return kind && (!strcmp(kind, "connector") || !strcmp(kind, "mcp") || contracted_kind_is_mcp_copy(kind));
}

/* Rows this kind contributes to its TRANSITIONS map. */
static const char *contracted_row_kind(const char *kind) {
    return contracted_kind_is_mcp_copy(kind) ? "mcp" : kind;
}

static json_t *contracted_transitions_for(const char *kind) {
    json_t *out, *from_obj = NULL;
    const char *row_kind, *last_from = NULL;
    int i;
    if (!contracted_kind_known(kind)) return json_null();
    row_kind = contracted_row_kind(kind);
    out = json_object();
    for (i = 0; CONTRACTED_EDGES[i].kind; i++) {
        const contracted_edge *e = &CONTRACTED_EDGES[i];
        if (strcmp(e->kind, row_kind)) continue;
        if (!last_from || strcmp(last_from, e->from)) {
            from_obj = json_object();
            json_set(out, e->from, from_obj);
            last_from = e->from;
        }
        json_set(from_obj, e->to, json_string(e->actor));
    }
    return out;
}

int w20_contract_80eced_resolves(const char *kind, const char *state) {
    if (!kind || !state) return 0;
    if (!strcmp(state, "connected") || !strcmp(state, "skipped")) return 1;
    if (contracted_kind_is_mcp_copy(kind) && !strcmp(state, "failed")) return 1;
    return 0;
}

json_t *w20_contract_80eced_allowed(const char *kind, const char *current, const char *to) {
    const char *row_kind;
    int i;
    if (!contracted_kind_known(kind) || !current || !to) return json_null();
    row_kind = contracted_row_kind(kind);
    for (i = 0; CONTRACTED_EDGES[i].kind; i++) {
        const contracted_edge *e = &CONTRACTED_EDGES[i];
        if (strcmp(e->kind, row_kind)) continue;
        if (!strcmp(e->from, current) && !strcmp(e->to, to)) {
            return json_string(e->actor);
        }
    }
    return json_null();
}

json_t *w20_contract_80eced_transitions(const char *kind) {
    return contracted_transitions_for(kind);
}

static json_t *contracted_string_array(const char *const *values) {
    json_t *arr = json_array();
    int i;
    for (i = 0; values[i]; i++) json_append(arr, json_string(values[i]));
    return arr;
}

json_t *w20_contract_80eced_kinds(void) {
    return contracted_string_array(CONTRACTED_KINDS);
}

json_t *w20_contract_80eced_resolved_states(void) {
    json_t *arr = json_array();
    /* sorted(RESOLVED_STATES) */
    json_append(arr, json_string("connected"));
    json_append(arr, json_string("skipped"));
    return arr;
}

json_t *w20_contract_80eced_enum_values(const char *enum_name) {
    if (!enum_name) return json_null();
    if (!strcmp(enum_name, "TargetState")) return contracted_string_array(CONTRACTED_TARGET_STATES);
    if (!strcmp(enum_name, "Actor")) return contracted_string_array(CONTRACTED_ACTORS);
    if (!strcmp(enum_name, "SettleReason")) return contracted_string_array(CONTRACTED_SETTLE_REASONS);
    return json_null();
}
