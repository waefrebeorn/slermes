/*
 * memory_caller_scope.c - C home for upstream Hermes agent/memory_caller_scope.py.
 * PoP: w11_memorycall_6310dc_refused_tool_message @ agent/memory_caller_scope.py:refused_tool_message
 * PoP: w11_memorycall_6310dc_bound_scope @ agent/memory_caller_scope.py:bound_scope
 * PoP: w11_memorycall_6310dc_caller_scope @ agent/memory_caller_scope.py:caller_scope
 *
 * Module-level literal constants mirrored from the Python source (provenance):
 *   CALLER_PROFILE_REFUSAL = "memory_caller_profile_unresolved"
 *   SHARED_INSTANCE_REFUSAL = "memory_provider_instance_shared"
 *
 * Env/filesystem-dependent collaborators are injected as explicit seams from the
 * fixture (PORT-KIT: never mutate the process environment): caller_home models
 * _explicit_caller_home(), process_home models get_hermes_home(), multiplexed
 * models is_multiplex_active(), and `keys` models the _home_key() result per home.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_memorycall_6310dc.h"

static const char *const W11MC_CALLER_PROFILE_REFUSAL = "memory_caller_profile_unresolved";
static const char *const W11MC_SHARED_INSTANCE_REFUSAL = "memory_provider_instance_shared";
char *w11_memorycall_6310dc_refused_tool_message(const char *tool_name) {
    /* "Memory tool '<name>' refused. Ask the operator to check `hermes logs errors`." */
    size_t n = strlen("Memory tool '' refused. Ask the operator to check `hermes logs errors`.")
             + (tool_name ? strlen(tool_name) : 0) + 8;
    char *o=(char *)malloc(n);
    snprintf(o,n,"Memory tool '%s' refused. Ask the operator to check `hermes logs errors`.",
             tool_name ? tool_name : "");
    return o;
}
/* Python truthiness of a JSON value (None/False/0/""/[]/{} are falsy). */
static int w11mc_truthy(json_t *v) {
    if (!v || v->type==JSON_NULL) return 0;
    if (v->type==JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type==JSON_NUMBER) return v->num_val!=0.0 ? 1 : 0;
    if (v->type==JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return v->c.count>0 ? 1 : 0;
}
json_t *w11_memorycall_6310dc_bound_scope(json_t *home, json_t *identity, json_t *home_key) {
    json_t *o=json_object();
    json_set(o,"hermes_home", home ? json_copy(home) : json_null());
    json_set(o,"agent_identity", identity ? json_copy(identity) : json_null());
    json_set(o,"home_key", home_key ? json_copy(home_key) : json_null());
    return o;
}
/* Injected _home_key(): NULL home/"" -> NULL (unresolvable), else keys[home]. */
static char *w11mc_key_for(const char *home, json_t *keys) {
    if (!home || !*home || !keys || keys->type!=JSON_OBJECT) return NULL;
    json_t *v=json_obj_get(keys,home);
    if (v && v->type==JSON_STRING) return strdup(v->str_val);
    return NULL;
}
json_t *w11_memorycall_6310dc_caller_scope(json_t *bound, const char *caller_home,
                                           const char *process_home, int multiplexed, json_t *keys) {
    json_t *b = (bound && bound->type==JSON_OBJECT) ? bound : NULL;  /* bound or {} */
    json_t *bound_home = b ? json_obj_get(b,"hermes_home") : NULL;
    int has_home = w11mc_truthy(bound_home);
    const char *ch = caller_home;                 /* explicit caller profile, or NULL */
    char *ch_owned=NULL;
    if (!ch) {
        if (multiplexed) return NULL;             /* refused: no profile named on a multiplexed process */
        ch = process_home ? process_home : "";    /* single-profile: str(get_hermes_home()) */
        (void)ch_owned;
    }
    if (!has_home) {
        if (multiplexed) return NULL;             /* refused: manager recorded no profile */
        json_t *o=json_object();
        json_set(o,"hermes_home", json_string(ch));
        return o;
    }
    const char *home_str = (bound_home->type==JSON_STRING) ? bound_home->str_val : NULL;
    json_t *bk = json_obj_get(b,"home_key");
    char *bound_key_owned=NULL;
    const char *bound_key;
    if (bk && bk->type==JSON_STRING && bk->str_val[0]) {
        bound_key = bk->str_val;                  /* bound.get("home_key") truthy */
    } else {
        bound_key_owned = w11mc_key_for(home_str, keys);
        bound_key = bound_key_owned;              /* else _home_key(bound_home) */
    }
    char *caller_key = w11mc_key_for(ch, keys);
    if (!caller_key || !bound_key || strcmp(caller_key,bound_key)!=0) {
        free(caller_key); free(bound_key_owned); return NULL;
    }
    free(caller_key); free(bound_key_owned);
    json_t *o=json_object();
    json_set(o,"hermes_home", json_copy(bound_home));
    json_t *ident = json_obj_get(b,"agent_identity");
    if (w11mc_truthy(ident)) json_set(o,"agent_identity", json_copy(ident));
    return o;
}
