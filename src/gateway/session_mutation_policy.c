/*
 * session_mutation_policy.c — C11 port of upstream Hermes
 * gateway/session_mutation_policy.py (gate w25_sessionmut_836d46).
 *
 * PoP: w25_sessionmut_836d46_edit_policy @ gateway/session_mutation_policy.py:prepare_edit_policy
 * PoP: w25_sessionmut_836d46_workspace @ gateway/session_mutation_policy.py:prepare_workspace
 * PoP: w25_sessionmut_836d46_reasoning @ gateway/session_mutation_policy.py:prepare_reasoning
 * PoP: w25_sessionmut_836d46_constants @ gateway/session_mutation_policy.py:PREPARE
 *
 * Both policy-only edits rewrite the frozen LocalSessionPolicy the next turn runs
 * under, then re-bind it like a model switch.  The authority, the policy blob, the
 * cwd probe (a stat and up to three `git` spawns) and the config-secret recovery
 * are seams; what is ported is the decision they feed: the ACP-only edit gate, the
 * no-op when the selected edit policy is already frozen, the cwd/TERMINAL_CWD
 * merge and whether /reasoning pins a level or hands it back to the profile.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w25_sessionmut_836d46.h"

/* Python truthiness of a JSON value. */
static int sm_truthy(const json_t *v) {
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

/* Python '==' over the shapes these values hold; absent and null are both None. */
static int sm_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (b->type == JSON_BOOL && a->type == JSON_NUMBER) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!sm_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !sm_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

static json_t *sm_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

/* AuthorityRefusal('invalid_params', field=..., detail=...) as the client frame carries it. */
static json_t *sm_refusal(const char *field, const char *detail) {
    json_t *out = json_object();
    json_set(out, "status", json_string("refused"));
    json_set(out, "reason", json_string("invalid_params"));
    json_set(out, "field", field ? json_string(field) : json_null());
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    return out;
}

json_t *w25_sessionmut_836d46_edit_policy(const json_t *req) {
    const json_t *source = json_obj_get(req, "source");
    const json_t *old_policy = json_obj_get(req, "old_policy");
    const json_t *wanted = json_obj_get(req, "payload_policy");
    json_t *out;

    if (!source || source->type != JSON_STRING || !source->str_val
        || strcmp(source->str_val, "acp") != 0) {
        return sm_refusal("payload.policy",
                          "only an ACP session has an editor edit approval policy");
    }
    out = json_object();
    if (sm_equal(old_policy, wanted)) {
        json_set(out, "status", json_string("noop"));
        json_set(out, "edit_approval_policy", sm_copy_or_null(old_policy));
    } else {
        json_set(out, "status", json_string("rebound"));
        json_set(out, "edit_approval_policy", sm_copy_or_null(wanted));
    }
    return out;
}

json_t *w25_sessionmut_836d46_workspace(const json_t *req) {
    const json_t *cwd = json_obj_get(req, "cwd");
    json_t *out, *terminal, *git;

    /* probe(): a non-directory cwd refuses before anything is rewritten. */
    if (!sm_truthy(json_obj_get(req, "isdir"))) return sm_refusal(NULL, NULL);

    terminal = json_parse(json_get_str(req, "terminal_json", "{}"), NULL);
    if (!terminal || terminal->type != JSON_OBJECT) {
        if (terminal) json_free(terminal);
        terminal = json_object();
    }
    /* terminal['TERMINAL_CWD'] = cwd (restore_policy's invariant) */
    json_set(terminal, "TERMINAL_CWD", sm_copy_or_null(cwd));

    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "cwd", sm_copy_or_null(cwd));
    json_set(out, "terminal", terminal);
    git = json_object();
    json_set(git, "branch", sm_copy_or_null(json_obj_get(req, "branch")));
    json_set(git, "root", sm_copy_or_null(json_obj_get(req, "root")));
    json_set(out, "git", git);
    return out;
}

json_t *w25_sessionmut_836d46_reasoning(const json_t *req) {
    const json_t *effort = json_obj_get(req, "effort");
    const json_t *given = json_obj_get(req, "profile");
    json_t *calls = json_array(), *call, *out = json_object();

    if (effort && effort->type != JSON_NULL) {
        /* pin_reasoning_effort(frozen, payload['effort'], session=True) */
        call = json_object();
        json_set(call, "op", json_string("pin"));
        json_set(call, "value", json_copy(effort));
    } else {
        /* profile = {} if old.ignore_user_config else profile_config(authority) */
        json_t *profile;
        call = json_object();
        if (sm_truthy(json_obj_get(req, "ignore_user_config")) || !given
            || given->type != JSON_OBJECT) {
            profile = json_object();
        } else {
            profile = json_copy(given);
        }
        json_set(call, "op", json_string("unpin"));
        json_set(call, "profile", profile);
    }
    json_append(calls, call);
    json_set(out, "calls", calls);
    json_set(out, "frozen", sm_copy_or_null(json_obj_get(req, "frozen")));
    return out;
}

json_t *w25_sessionmut_836d46_constants(void) {
    json_t *out = json_object(), *prepare = json_array();
    json_append(prepare, json_string("reasoning"));
    json_append(prepare, json_string("workspace"));
    json_append(prepare, json_string("edit_policy"));
    json_set(out, "prepare", prepare);
    return out;
}
